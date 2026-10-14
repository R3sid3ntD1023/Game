#version 460 core

in VS_OUT {
    vec3 worldPos;
    vec2 uv;
    mat3 tbn;
} fs;

flat in float vEmissiveScale;

out vec4 outColor;

layout(binding = 0) uniform sampler2D uAlbedoMap;
layout(binding = 1) uniform sampler2D uNormalMap;
layout(binding = 2) uniform sampler2D uMetalRoughMap; // G = roughness, B = metallic
layout(binding = 3) uniform samplerCube uIrradianceMap;
layout(binding = 4) uniform samplerCube uPrefilterMap;
layout(binding = 5) uniform sampler2D uBrdfLut;

uniform vec4 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAO;
uniform vec3 uEmissive;
uniform vec3 uCameraPos;

// Forward+ light data (see gfx/ForwardPlus.h and light_cull.comp)
struct Light {
    vec4 positionRange;  // xyz position, w range
    vec4 colorType;      // rgb radiance, w type (0 point, 1 spot, 2 directional)
    vec4 directionInner; // xyz direction the light travels, w cos(inner)
    vec4 outer;          // x cos(outer)
};

const uint MAX_PER_TILE = 64u;
const int TILE_SIZE = 16;

layout(std430, binding = 3) readonly buffer Lights { Light lights[]; };
layout(std430, binding = 4) readonly buffer TileCounts { uint tileCounts[]; };
layout(std430, binding = 5) readonly buffer TileIndices { uint tileIndices[]; };

uniform int uTileCountX;

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0; // prefilter mip count - 1
const float FOG_DENSITY = 0.018;
const vec3 FOG_FLOOR = vec3(0.004, 0.006, 0.01);

// GGX / Trowbridge-Reitz normal distribution
float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Smith geometry with Schlick-GGX
float GeometrySchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Cook-Torrance contribution of one light arriving from direction L with the given radiance.
vec3 Shade(vec3 L, vec3 radiance, vec3 N, vec3 V, float NdotV, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    vec3 F = FresnelSchlick(HdotV, F0);

    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 EvaluateLight(Light light, vec3 N, vec3 V, float NdotV, vec3 F0, vec3 albedo, float metallic, float roughness) {
    int type = int(light.colorType.w + 0.5);
    vec3 L;
    float attenuation = 1.0;

    if (type == 2) { // directional
        L = -normalize(light.directionInner.xyz);
    } else {
        vec3 toLight = light.positionRange.xyz - fs.worldPos;
        float d2 = dot(toLight, toLight);
        float r2 = light.positionRange.w * light.positionRange.w;
        float window = clamp(1.0 - (d2 * d2) / (r2 * r2), 0.0, 1.0); // smooth cutoff at the range
        attenuation = window * window / (d2 + 1.0);
        L = normalize(toLight);
        if (type == 1) // spot
            attenuation *= smoothstep(light.outer.x, light.directionInner.w, dot(-L, normalize(light.directionInner.xyz)));
    }
    return Shade(L, light.colorType.rgb * attenuation, N, V, NdotV, F0, albedo, metallic, roughness);
}

void main() {
    vec4 albedoSample = texture(uAlbedoMap, fs.uv) * uBaseColor;
    vec3 albedo = albedoSample.rgb;
    vec3 mr = texture(uMetalRoughMap, fs.uv).rgb;
    float roughness = clamp(uRoughness * mr.g, 0.04, 1.0);
    float metallic = clamp(uMetallic * mr.b, 0.0, 1.0);

    vec3 tn = texture(uNormalMap, fs.uv).xyz * 2.0 - 1.0;
    vec3 N = normalize(fs.tbn * tn);
    vec3 V = normalize(uCameraPos - fs.worldPos);
    float NdotV = max(dot(N, V), 1e-4);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Lights assigned to this pixel's screen tile by the culling pass
    ivec2 tile = ivec2(gl_FragCoord.xy) / TILE_SIZE;
    uint tileIndex = uint(tile.y * uTileCountX + tile.x);
    uint count = tileCounts[tileIndex];

    vec3 Lo = vec3(0.0);
    for (uint i = 0u; i < count; ++i)
        Lo += EvaluateLight(lights[tileIndices[tileIndex * MAX_PER_TILE + i]], N, V, NdotV, F0, albedo, metallic, roughness);

    // Image-based lighting: diffuse irradiance + split-sum specular
    vec3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    vec3 irradiance = texture(uIrradianceMap, N).rgb;
    vec3 R = reflect(-V, N);
    vec3 prefiltered = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBrdf = texture(uBrdfLut, vec2(NdotV, roughness)).rg;
    vec3 ambient = (kD * irradiance * albedo + prefiltered * (kS * envBrdf.x + envBrdf.y)) * uAO;

    vec3 color = ambient + Lo + uEmissive * vEmissiveScale;

    float fog = 1.0 - exp(-length(uCameraPos - fs.worldPos) * FOG_DENSITY);
    vec3 fogColor = FOG_FLOOR + textureLod(uPrefilterMap, normalize(vec3(-V.x, 0.05, -V.z)), MAX_REFLECTION_LOD).rgb;
    color = mix(color, fogColor, clamp(fog, 0.0, 1.0));

    // Linear HDR output; tone mapping happens in the composite pass.
    outColor = vec4(color, albedoSample.a);
}
