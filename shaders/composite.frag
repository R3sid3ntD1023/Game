#version 460 core

in vec2 vUV;
out vec4 outColor;

layout(binding = 0) uniform sampler2D uScene;
layout(binding = 1) uniform sampler2D uBloom;
layout(binding = 2) uniform sampler2D uDepth;

uniform float uUnderwater; // 0 above water .. 1 submerged
uniform float uNear;
uniform float uFar;

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
    vec2 uv = vUV;
    float dist = 0.0;
    if (uUnderwater > 0.0) {
        // Wobble the image like light refracting through moving water.
        vec2 w = vec2(sin(uv.y * 38.0 + uTime * 1.7), cos(uv.x * 31.0 + uTime * 1.3));
        uv += w * 0.0035 * uUnderwater;
        float z = texture(uDepth, uv).r;
        dist = (uNear * uFar) / (uFar - z * (uFar - uNear));
    }
    vec3 color = texture(uScene, uv).rgb;
    if (uBloomIntensity > 0.0) color += texture(uBloom, uv).rgb * uBloomIntensity;

    if (uUnderwater > 0.0) {
        // Absorption: red vanishes first, then green; far things dissolve into a murky blue-green.
        vec3 absorb = exp(-dist * vec3(0.22, 0.07, 0.045));
        vec3 fogColor = vec3(0.01, 0.06, 0.075) * (0.6 + 0.4 * uExposure);
        vec3 under = color * absorb + fogColor * (1.0 - absorb);
        // Soft caustic-like shimmer in the light.
        under *= 1.0 + 0.08 * sin(uv.x * 60.0 + uTime * 2.0) * sin(uv.y * 47.0 - uTime * 1.6);
        color = mix(color, under, uUnderwater);
    }
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
