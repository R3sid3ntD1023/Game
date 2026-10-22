#version 460 core
in vec3 vNormal;
in vec3 vWorld;
in vec2 vUV;

layout(binding = 0) uniform sampler2D uAlbedoMap;
layout(binding = 3) uniform samplerCube uIrradiance;

uniform int uHasTexture;  // 1: multiply uColor by uAlbedoMap
uniform float uSkyLight;  // multiplier on the sky ambient term
uniform float uFogDensity; // 0 = fog off

const int MAX_LIGHTS = 64;

uniform vec3 uColor;
uniform vec3 uEmissive;
uniform int uFlat; // 1: draw uColor unlit (wireframes, markers)
uniform int uLit;  // 1: scene lights + sky ambient, 0: fixed headlight-style shading
uniform vec3 uCameraPos;
uniform int uLightCount;
uniform vec4 uLightPosRange[MAX_LIGHTS];  // xyz position, w range
uniform vec4 uLightColorType[MAX_LIGHTS]; // rgb radiance, w type (0 point, 1 spot, 2 directional)
uniform vec4 uLightDirInner[MAX_LIGHTS];  // xyz direction the light shines along, w cos(inner)
uniform float uLightOuter[MAX_LIGHTS];    // cos(outer)

out vec4 FragColor;

const float PI = 3.14159265;

void main()
{
    if (uFlat == 1)
    {
        FragColor = vec4(uColor, 1.0);
        return;
    }
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCameraPos - vWorld);
    if (dot(n, v) < 0.0)
        n = -n;

    vec3 albedo = uColor;
    if (uHasTexture == 1)
        albedo *= texture(uAlbedoMap, vUV).rgb;

    if (uLit == 0)
    {
        float sun = max(dot(n, normalize(vec3(0.4, 1.0, 0.3))), 0.0);
        float sky = 0.5 + 0.5 * n.y;
        FragColor = vec4(albedo * (0.25 * sky + 0.85 * sun) + uEmissive, 1.0);
        return;
    }

    vec3 diffuse = texture(uIrradiance, n).rgb * uSkyLight;
    for (int i = 0; i < uLightCount; ++i)
    {
        int type = int(uLightColorType[i].w + 0.5);
        vec3 l;
        float atten = 1.0;
        if (type == 2)
        {
            l = -normalize(uLightDirInner[i].xyz);
        }
        else
        {
            vec3 toLight = uLightPosRange[i].xyz - vWorld;
            float d2 = dot(toLight, toLight);
            l = toLight * inversesqrt(max(d2, 1e-6));
            float r2 = uLightPosRange[i].w * uLightPosRange[i].w;
            float window = clamp(1.0 - (d2 * d2) / (r2 * r2), 0.0, 1.0);
            atten = window * window / (d2 + 1.0);
            if (type == 1)
                atten *= smoothstep(uLightOuter[i], uLightDirInner[i].w, dot(-l, normalize(uLightDirInner[i].xyz)));
        }
        diffuse += uLightColorType[i].rgb * atten * max(dot(n, l), 0.0);
    }
    vec3 color = albedo / PI * diffuse + uEmissive;
    if (uFogDensity > 0.0)
    {
        vec3 toFrag = vWorld - uCameraPos;
        float dist = length(toFrag);
        vec3 dir = toFrag / max(dist, 1e-4);
        vec3 fogColor = texture(uIrradiance, normalize(vec3(dir.x, 0.1, dir.z))).rgb * uSkyLight / PI * 2.0;
        color = mix(fogColor, color, exp(-dist * uFogDensity));
    }
    FragColor = vec4(color, 1.0);
}
