#version 460 core
#include "../pipelines/ForwardMesh.frag"
#include "../noise.glsl"

// Terrain layers picked per pixel from height (sand near the waterline), slope (rock on cliffs)
// and large-scale noise (dry grass and mud regions), each with its own fine-grained texture.
Surface GetSurface() {
    vec3 wp = fs.worldPos;
    vec3 n = normalize(fs.tbn[2]);
    vec2 p = wp.xz;
    float large = Fbm(p * 0.018);
    float mid = Fbm(p * 0.12 + 7.0);
    float fine = Fbm(p * 1.7);
    float grain = ValueNoise(p * 9.0);

    vec3 grassA = vec3(0.035, 0.11, 0.025), grassB = vec3(0.09, 0.15, 0.04);
    vec3 grass = mix(grassA, grassB, smoothstep(0.35, 0.7, mid)) * (0.75 + 0.5 * fine);
    vec3 dry = vec3(0.16, 0.13, 0.05) * (0.7 + 0.6 * fine);
    vec3 mud = vec3(0.07, 0.045, 0.028) * (0.7 + 0.6 * grain);
    vec3 sand = vec3(0.34, 0.27, 0.15) * (0.85 + 0.3 * grain);
    vec3 rock = vec3(0.16, 0.155, 0.16) * (0.6 + 0.8 * fine) * (0.85 + 0.3 * ValueNoise(p * 0.8));

    vec3 c = mix(grass, dry, smoothstep(0.55, 0.75, large));
    c = mix(c, mud, smoothstep(0.62, 0.72, Fbm(p * 0.035 + 31.0)));
    float rough = 0.92;

    float sandMask = 1.0 - smoothstep(0.6, 1.8 + 0.8 * mid, wp.y);
    c = mix(c, sand, sandMask);
    rough = mix(rough, 0.85, sandMask);

    float slope = 1.0 - clamp(n.y, 0.0, 1.0);
    float rockMask = smoothstep(0.22, 0.4, slope + 0.06 * (fine - 0.5));
    c = mix(c, rock, rockMask);
    rough = mix(rough, 0.7, rockMask);

    vec4 a = texture(uAlbedoMap, fs.uv) * uBaseColor;
    Surface s;
    s.albedo = c;
    s.alpha = a.a;
    s.metallic = 0.0;
    s.roughness = rough;
    s.normal = gl_FrontFacing ? n : -n;
    return s;
}
