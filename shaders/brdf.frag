#version 460 core

in VS_OUT {
    vec3 worldPos;
    vec2 uv;
    mat3 tbn;
} fs;

out vec4 outColor;

layout(binding = 0) uniform sampler2D uAlbedoMap;
layout(binding = 1) uniform sampler2D uNormalMap;
layout(binding = 2) uniform sampler2D uMetalRoughMap; // G = roughness, B = metallic

uniform vec4 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAO;
uniform vec3 uEmissive;
uniform vec3 uCameraPos;

const float PI = 3.14159265359;

// Directional sun; must match the clear color in PostProcess for fog blending.
const vec3 SUN_DIR = normalize(vec3(-0.4, 0.8, 0.3)); // direction towards the sun
const vec3 SUN_RADIANCE = vec3(3.0, 2.8, 2.5);
layout(binding = 3) uniform samplerCube uIrradianceMap;
layout(binding = 4) uniform samplerCube uPrefilterMap;
layout(binding = 5) uniform sampler2D uBrdfLut;

const float MAX_REFLECTION_LOD = 4.0; // prefilter mip count - 1
const float FOG_DENSITY = 0.008;

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
    vec3 Lo = vec3(0.0);

    {
        vec3 L = SUN_DIR;
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        float D = DistributionGGX(NdotH, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        vec3 F = FresnelSchlick(HdotV, F0);

        vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
        vec3 kD = (1.0 - F) * (1.0 - metallic);

        Lo += (kD * albedo / PI + specular) * SUN_RADIANCE * NdotL;
    }

    // Image-based lighting: diffuse irradiance + split-sum specular
    vec3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    vec3 irradiance = texture(uIrradianceMap, N).rgb;
    vec3 R = reflect(-V, N);
    vec3 prefiltered = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBrdf = texture(uBrdfLut, vec2(NdotV, roughness)).rg;
    vec3 ambient = (kD * irradiance * albedo + prefiltered * (kS * envBrdf.x + envBrdf.y)) * uAO;

    vec3 color = ambient + Lo + uEmissive;

    float fog = 1.0 - exp(-length(uCameraPos - fs.worldPos) * FOG_DENSITY);
    vec3 fogColor = textureLod(uPrefilterMap, normalize(vec3(-V.x, 0.05, -V.z)), MAX_REFLECTION_LOD).rgb;
    color = mix(color, fogColor, clamp(fog, 0.0, 1.0));

    // Linear HDR output; tone mapping happens in the composite pass.
    outColor = vec4(color, albedoSample.a);
}
