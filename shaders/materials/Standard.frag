#version 460 core
#include "../pipelines/ForwardMesh.frag"

// Textured PBR: albedo/normal/metal-rough maps modulated by the material factors.
Surface GetSurface() {
    vec4 a = texture(uAlbedoMap, fs.uv) * uBaseColor;
    vec3 mr = texture(uMetalRoughMap, fs.uv).rgb;
    vec3 tn = texture(uNormalMap, fs.uv).xyz * 2.0 - 1.0;
    vec3 N = normalize(fs.tbn * tn);
    if (!gl_FrontFacing)
        N = -N; // double-sided materials light their back faces

    Surface s;
    s.albedo = a.rgb;
    s.alpha = a.a;
    s.metallic = uMetallic * mr.b;
    s.roughness = uRoughness * mr.g;
    s.normal = N;
    return s;
}
