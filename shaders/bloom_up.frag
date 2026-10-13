#version 460 core

in vec2 vUV;
out vec4 outColor;

layout(binding = 0) uniform sampler2D uSrc;

uniform float uRadius; // tent filter radius in UV space

void main() {
    float x = uRadius;
    float y = uRadius;

    vec3 a = texture(uSrc, vUV + vec2(-x, y)).rgb;
    vec3 b = texture(uSrc, vUV + vec2(0, y)).rgb;
    vec3 c = texture(uSrc, vUV + vec2(x, y)).rgb;
    vec3 d = texture(uSrc, vUV + vec2(-x, 0)).rgb;
    vec3 e = texture(uSrc, vUV).rgb;
    vec3 f = texture(uSrc, vUV + vec2(x, 0)).rgb;
    vec3 g = texture(uSrc, vUV + vec2(-x, -y)).rgb;
    vec3 h = texture(uSrc, vUV + vec2(0, -y)).rgb;
    vec3 i = texture(uSrc, vUV + vec2(x, -y)).rgb;

    // Result is additively blended onto the next larger mip.
    outColor = vec4((e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i)) / 16.0, 1.0);
}
