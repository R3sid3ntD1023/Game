#version 460 core

in vec2 vUV;
out vec4 outColor;

layout(binding = 0) uniform sampler2D uScene;
layout(binding = 1) uniform sampler2D uBloom;

uniform float uBloomIntensity;
uniform float uExposure;

vec3 ACESFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3 color = texture(uScene, vUV).rgb;
    if (uBloomIntensity > 0.0) color += texture(uBloom, vUV).rgb * uBloomIntensity;
    color = ACESFilm(color * uExposure);
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
