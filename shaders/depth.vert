#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aUV;
layout(location = 4) in ivec4 aBoneIds;
layout(location = 5) in vec4 aBoneWeights;

struct Instance {
    mat4 model;
    uint boneOffset;
    float emissiveScale;
    float windStrength;
    uint pad1;
};

layout(std430, binding = 0) readonly buffer Instances { Instance instances[]; };
layout(std430, binding = 1) readonly buffer Bones { mat4 bones[]; };

uniform mat4 uViewProj;

#include "wind.glsl"

out vec2 vUV;

invariant gl_Position; // must match brdf.vert exactly so the shading pass can use GL_LEQUAL

void main() {
    Instance inst = instances[gl_BaseInstance + gl_InstanceID];
    vUV = aUV;

    vec4 localPos = vec4(aPosition, 1.0);
    if (inst.boneOffset != 0xFFFFFFFFu && dot(aBoneWeights, vec4(1.0)) > 0.0) {
        mat4 skin = aBoneWeights.x * bones[inst.boneOffset + aBoneIds.x]
                  + aBoneWeights.y * bones[inst.boneOffset + aBoneIds.y]
                  + aBoneWeights.z * bones[inst.boneOffset + aBoneIds.z]
                  + aBoneWeights.w * bones[inst.boneOffset + aBoneIds.w];
        localPos = skin * localPos;
    }
    vec4 world = inst.model * localPos;
    world.xyz += WindOffset(world.xyz, localPos.y, inst.windStrength);
    gl_Position = uViewProj * world;
}
