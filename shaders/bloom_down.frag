#version 460 core

in vec2 vUV;
out vec4 outColor;

layout(binding = 0) uniform sampler2D uSrc;

uniform vec2 uTexelSize; // 1 / source resolution
uniform int uFirst;      // first pass applies the bright-pass threshold
uniform float uThreshold;

vec3 Prefilter(vec3 c) {
    float brightness = max(c.r, max(c.g, c.b));
    float knee = uThreshold * 0.5;
    float soft = clamp(brightness - uThreshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    float contribution = max(soft, brightness - uThreshold) / max(brightness, 1e-4);
    return c * max(contribution, 0.0);
}

vec3 Fetch(vec2 offset) {
    return texture(uSrc, vUV + offset * uTexelSize).rgb;
}

void main() {
    // 13-tap downsample (Call of Duty: Advanced Warfare)
    vec3 a = Fetch(vec2(-2, 2)),  b = Fetch(vec2(0, 2)),  c = Fetch(vec2(2, 2));
    vec3 d = Fetch(vec2(-2, 0)),  e = Fetch(vec2(0, 0)),  f = Fetch(vec2(2, 0));
    vec3 g = Fetch(vec2(-2, -2)), h = Fetch(vec2(0, -2)), i = Fetch(vec2(2, -2));
    vec3 j = Fetch(vec2(-1, 1)),  k = Fetch(vec2(1, 1));
    vec3 l = Fetch(vec2(-1, -1)), m = Fetch(vec2(1, -1));

    vec3 color = e * 0.125;
    color += (a + c + g + i) * 0.03125;
    color += (b + d + f + h) * 0.0625;
    color += (j + k + l + m) * 0.125;

    outColor = vec4(uFirst == 1 ? Prefilter(color) : color, 1.0);
}
