#version 460 core

in float vSlope;
in vec2 vWorldXZ;

uniform float uAlpha;
uniform int uMode;       // 0 = walkable (green with dark outline), 1 = slope heat map
uniform vec2 uOrigin;    // world XZ of cell (0,0)
uniform float uCellSize;

out vec4 FragColor;

void main() {
    if (uMode == 1) {
        // flat -> green, moderate -> yellow, steep -> red
        vec3 c = vSlope < 0.5 ? mix(vec3(0.1, 0.8, 0.2), vec3(1.0, 0.9, 0.1), vSlope * 2.0)
                              : mix(vec3(1.0, 0.9, 0.1), vec3(1.0, 0.1, 0.05), (vSlope - 0.5) * 2.0);
        FragColor = vec4(c, uAlpha);
        return;
    }

    // Distance to the nearest cell edge; fwidth keeps the outline ~1.5px wide at any range.
    vec2 g = (vWorldXZ - uOrigin) / uCellSize;
    vec2 d = abs(fract(g + 0.5) - 0.5);
    vec2 w = max(fwidth(g) * 1.5, vec2(1e-5));
    float edge = 1.0 - smoothstep(0.0, 1.0, min(d.x / w.x, d.y / w.y));

    vec3 fill = vec3(0.15, 0.85, 0.25);
    vec3 outline = vec3(0.0, 0.3, 0.05);
    FragColor = vec4(mix(fill, outline, edge), mix(uAlpha, 0.9, edge));
}
