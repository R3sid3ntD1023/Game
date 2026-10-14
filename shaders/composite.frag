#version 460 core

in vec2 vUV;
out vec4 outColor;

layout(binding = 0) uniform sampler2D uScene;
layout(binding = 1) uniform sampler2D uBloom;

uniform float uBloomIntensity;
uniform float uExposure;
uniform float uTime;
uniform float uVignette; // 0 = none
uniform float uGrain;    // film grain strength

vec3 ACESFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

float Hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec3 color = texture(uScene, vUV).rgb;
    if (uBloomIntensity > 0.0) color += texture(uBloom, vUV).rgb * uBloomIntensity;
    color = ACESFilm(color * uExposure);

    // Cold, slightly desaturated grade
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, 0.85) * vec3(0.94, 0.99, 1.08);

    vec2 d = vUV - 0.5;
    color *= clamp(1.0 - uVignette * dot(d, d) * 2.2, 0.0, 1.0);

    color = pow(color, vec3(1.0 / 2.2));
    color += (Hash(vUV * vec2(1280.0, 720.0) + fract(uTime) * 61.0) - 0.5) * uGrain;
    outColor = vec4(color, 1.0);
}
