#version 460 core

struct Particle {
    vec4 posSize; // xyz position, w size (width for streaks)
    vec4 vel;     // streaks: xyz direction of travel, w length
    vec4 color;
};

layout(std430, binding = 2) readonly buffer Particles {
    Particle particles[];
};

uniform mat4 uViewProj;
uniform vec3 uCameraPos;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform int uStreak; // 0 = camera-facing quad, 1 = rain streak along `vel`, 2 = flat splash ring on the ground

out vec2 vUV;
out vec4 vColor;

const vec2 kCorners[6] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));

void main() {
    Particle p = particles[gl_InstanceID];
    vec2 c = kCorners[gl_VertexID];
    vec3 world;

    if (uStreak == 1) {
        vec3 axis = p.vel.xyz;
        vec3 toCam = normalize(uCameraPos - p.posSize.xyz);
        vec3 side = normalize(cross(axis, toCam));
        // c.y = 1 is the leading end, c.y = -1 the tail trailing behind it
        world = p.posSize.xyz + side * (c.x * p.posSize.w * 0.5) + axis * ((c.y * 0.5 - 0.5) * p.vel.w);
    } else if (uStreak == 2) {
        world = p.posSize.xyz + vec3(c.x, 0.0, c.y) * (p.posSize.w * 0.5);
    } else {
        world = p.posSize.xyz + (uCameraRight * c.x + uCameraUp * c.y) * (p.posSize.w * 0.5);
    }

    vUV = c * 0.5 + 0.5;
    vColor = p.color;
    gl_Position = uViewProj * vec4(world, 1.0);
}
