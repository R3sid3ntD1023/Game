// Shared vertex wind. Included by depth.vert and brdf.vert so every pass bends foliage identically
// (otherwise shadows and the depth prepass would not follow the visible geometry).
uniform float uTime;
uniform vec2 uWindDir; // world XZ direction, length = overall wind strength (0..1)

// World-space displacement for a vertex. `height` is the vertex height above the model origin;
// `strength` is the per-material sway in metres (0 = rigid). Anchored at the base, so height 0 stays put.
vec3 WindOffset(vec3 worldPos, float height, float strength) {
    if (strength <= 0.0)
        return vec3(0.0);
    float phase = dot(worldPos.xz, vec2(0.31, 0.23));
    float gust = 0.5 + 0.5 * sin(uTime * 1.3 + phase) + 0.25 * sin(uTime * 2.9 + phase * 2.3);
    float flutter = 0.12 * sin(uTime * 7.0 + worldPos.x * 1.7 + worldPos.z * 2.1 + height * 3.0);
    float weight = sqrt(clamp(height * 0.25, 0.0, 1.0));
    vec2 dir = uWindDir;
    vec2 side = vec2(-dir.y, dir.x);
    vec2 d = dir * gust + side * flutter;
    return vec3(d.x, -0.15 * gust * length(dir), d.y) * strength * weight;
}
