#version 460 core

in vec3 vDir;
out vec4 outColor;

layout(binding = 6) uniform samplerCube uEnvMap;

uniform int uProcedural; // 0 = sample uEnvMap (HDR), 1 = analytic day/night sky
uniform int uBake;       // 1 = rendering the IBL source cube: no stars or discs
uniform vec3 uSunDir;    // unit vector towards the sun; the moon sits opposite
uniform float uOvercast; // 0 clear .. 1 fully clouded
uniform float uDark;     // 0 light grey clouds .. 1 storm clouds
uniform float uTime;     // seconds, drifts the cloud layer
uniform float uFlash;    // lightning brightness, lights the clouds from within

#include "noise.glsl"

const float STAR_CELLS = 70.0;    // cells per cube-face half width
const float STAR_DENSITY = 0.07;  // fraction of cells holding a star
const float STAR_RADIUS_PX = 0.7; // star radius in pixels (kept sub-2px so they stay tiny)

uvec3 Pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v;
}

float Unit(uint h) {
    return float(h & 0xFFFFu) / 65535.0;
}

// One jittered point per cube-face grid cell; its size follows the pixel footprint so it stays ~1px at any resolution.
vec3 Stars(vec3 d, float pixelAngle) {
    vec3 a = abs(d);
    vec2 uv;
    uint face;
    float dom;
    if (a.x >= a.y && a.x >= a.z) {
        uv = d.zy / a.x;
        face = d.x > 0.0 ? 0u : 1u;
        dom = a.x;
    } else if (a.y >= a.z) {
        uv = d.xz / a.y;
        face = d.y > 0.0 ? 2u : 3u;
        dom = a.y;
    } else {
        uv = d.xy / a.z;
        face = d.z > 0.0 ? 4u : 5u;
        dom = a.z;
    }

    vec2 g = uv * STAR_CELLS;
    vec2 cell = floor(g);
    vec2 f = g - cell;

    uvec3 h = Pcg3d(uvec3(uint(int(cell.x)), uint(int(cell.y)), face));
    if (Unit(h.x) > STAR_DENSITY)
        return vec3(0.0);

    vec2 center = 0.25 + 0.5 * vec2(Unit(h.y), Unit(h.z));
    float pixelInCells = STAR_CELLS * pixelAngle / dom;
    float radius = clamp(pixelInCells * STAR_RADIUS_PX, 1e-4, 0.2);
    float core = 1.0 - smoothstep(0.0, radius, length(f - center));

    float mag = float((h.x >> 16u) & 0xFFu) / 255.0;
    float brightness = mix(0.35, 1.8, mag * mag * mag);
    float warm = float((h.z >> 16u) & 0xFFu) / 255.0;
    vec3 tint = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.88, 0.7), warm);
    return tint * brightness * core;
}

vec3 SkyGradient(vec3 d, float daylight, float twilight) {
    vec3 zenith = mix(vec3(0.004, 0.007, 0.018), vec3(0.08, 0.22, 0.62), daylight);
    vec3 horizon = mix(vec3(0.020, 0.028, 0.045), vec3(0.55, 0.70, 0.92), daylight);

    float toward = max(dot(normalize(d.xz + 1e-5), normalize(uSunDir.xz + 1e-5)), 0.0);
    vec3 glow = vec3(1.0, 0.42, 0.18) * twilight * pow(toward, 3.0);

    float h = clamp(d.y, 0.0, 1.0);
    vec3 sky = mix(horizon, zenith, pow(h, 0.45));
    sky += glow * pow(1.0 - h, 4.0) * 1.4;

    vec3 ground = mix(vec3(0.002, 0.003, 0.005), vec3(0.10, 0.095, 0.085), daylight) + glow * 0.1;
    return mix(sky, ground, 1.0 - smoothstep(-0.08, 0.0, d.y));
}

void main() {
    vec3 d = normalize(vDir);
    float pixelAngle = max(length(dFdx(d)), length(dFdy(d)));

    if (uProcedural == 0) {
        outColor = vec4(textureLod(uEnvMap, d, 0.0).rgb, 1.0);
        return;
    }

    float sunElev = uSunDir.y;
    float daylight = smoothstep(-0.12, 0.28, sunElev);
    float twilight = exp(-pow(sunElev / 0.16, 2.0));
    vec3 color = SkyGradient(d, daylight, twilight);

    // Overcast drains the colour towards grey (also baked into the lighting cube, so ambient light follows the weather).
    float lum = dot(color, vec3(0.3, 0.55, 0.15));
    vec3 grey = vec3(lum) * mix(0.9, 0.4, uDark);
    color = mix(color, grey, uOvercast * 0.85);

    if (uBake == 0) {
        float aboveHorizon = smoothstep(-0.01, 0.02, d.y);
        float clear = 1.0 - uOvercast;

        float sd = dot(d, uSunDir);
        vec3 sunColor = mix(vec3(1.0, 0.55, 0.25), vec3(1.0, 0.95, 0.8), smoothstep(0.0, 0.4, sunElev));
        color += sunColor * (40.0 * smoothstep(0.99985, 0.99995, sd) + 0.6 * pow(max(sd, 0.0), 220.0)) * aboveHorizon * clear * clear;

        float night = 1.0 - smoothstep(-0.2, 0.02, sunElev);
        float md = max(dot(d, -uSunDir), 0.0);
        float moonVis = (1.0 - daylight) * aboveHorizon * clear * clear;
        color += vec3(6.0, 6.5, 8.0) * smoothstep(0.9988, 0.9992, md) * moonVis;
        color += vec3(0.5, 0.6, 0.9) * 0.12 * pow(md, 120.0) * moonVis;

        color += Stars(d, pixelAngle) * night * smoothstep(0.0, 0.08, d.y) * clear * clear;

        // Cloud layer projected onto a flat deck; coverage grows with uOvercast.
        if (d.y > 0.0 && uOvercast > 0.01) {
            vec2 cuv = d.xz / (d.y + 0.2) * 0.9 + vec2(uTime * 0.012, uTime * 0.005);
            float n = Fbm(cuv) * 0.65 + 0.35 * Fbm(cuv * 3.1 + 5.0);
            float edge = mix(0.72, 0.18, uOvercast);
            float dens = smoothstep(edge, edge + 0.22, n) * smoothstep(0.0, 0.12, d.y);
            float lit = mix(0.25, 1.0, smoothstep(0.1, 0.7, n)); // thick centres are darker
            vec3 cloudCol = vec3(mix(0.05, 0.75, daylight)) * mix(1.0, 0.3, uDark) * mix(1.0, 0.65, (1.0 - lit) * 0.6);
            cloudCol += vec3(1.0, 0.45, 0.2) * twilight * 0.25 * (1.0 - uDark);
            cloudCol += vec3(0.7, 0.78, 1.0) * uFlash * (1.2 + 3.0 * (1.0 - lit)) ;
            color = mix(color, cloudCol, dens);
        }
    }

    outColor = vec4(color, 1.0);
}
