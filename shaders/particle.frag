#version 460 core

in vec2 vUV;
in vec4 vColor;

uniform int uStreak;

out vec4 outColor;

void main() {
    vec2 c = vUV * 2.0 - 1.0;
    float a = vColor.a;
    if (uStreak == 1) {
        a *= (1.0 - abs(c.x)) * vUV.y; // thin soft streak fading towards its tail
    } else if (uStreak == 2) {
        float r = length(c);
        a *= smoothstep(0.55, 0.75, r) * smoothstep(1.0, 0.85, r); // thin expanding ring
    } else {
        a *= smoothstep(1.0, 0.5, length(c)); // round droplet
    }
    outColor = vec4(vColor.rgb, a);
}
