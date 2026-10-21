#version 460 core
#include "../pipelines/ForwardMesh.frag"
#include "../noise.glsl"

// Blades darken at the root and brighten towards the tip; hue varies per clump.
// The normal stays upward on back faces so both sides of a blade light the same.
Surface GetSurface() {
    vec4 a = texture(uAlbedoMap, fs.uv) * uBaseColor;
    float variation = ValueNoise(fs.worldPos.xz * 0.45);
    vec3 base = a.rgb;
    vec3 tint = mix(base * vec3(1.15, 1.0, 0.7), base * vec3(0.8, 1.1, 0.9), variation);

    vec3 tn = texture(uNormalMap, fs.uv).xyz * 2.0 - 1.0;
    Surface s;
    s.albedo = mix(tint * 0.3, tint * 1.5, clamp(fs.uv.y, 0.0, 1.0));
    s.alpha = a.a;
    s.metallic = uMetallic * texture(uMetalRoughMap, fs.uv).b;
    s.roughness = uRoughness * texture(uMetalRoughMap, fs.uv).g;
    s.normal = normalize(fs.tbn * tn);
    return s;
}
