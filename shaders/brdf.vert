#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in ivec4 aBoneIds;
layout(location = 5) in vec4 aBoneWeights;

struct Instance {
    mat4 model;
    uint boneOffset;       // 0xFFFFFFFF = not skinned
    float emissiveScale;   // per-instance emissive multiplier (flickering lamps)
    uint pad0, pad1;
};

layout(std430, binding = 0) readonly buffer Instances {
    Instance instances[];
};

layout(std430, binding = 1) readonly buffer Bones {
    mat4 bones[];
};

uniform mat4 uViewProj;

invariant gl_Position; // must match depth.vert exactly so the shading pass can use GL_LEQUAL

out VS_OUT {
    vec3 worldPos;
    vec2 uv;
    mat3 tbn;
} vs;

flat out float vEmissiveScale;

void main() {
    // gl_InstanceID does not include baseInstance, so add it explicitly.
    Instance inst = instances[gl_BaseInstance + gl_InstanceID];
    mat4 model = inst.model;

    vec4 localPos = vec4(aPosition, 1.0);
    vec3 localNormal = aNormal;
    vec3 localTangent = aTangent.xyz;

    if (inst.boneOffset != 0xFFFFFFFFu && dot(aBoneWeights, vec4(1.0)) > 0.0) {
        mat4 skin = aBoneWeights.x * bones[inst.boneOffset + aBoneIds.x]
                  + aBoneWeights.y * bones[inst.boneOffset + aBoneIds.y]
                  + aBoneWeights.z * bones[inst.boneOffset + aBoneIds.z]
                  + aBoneWeights.w * bones[inst.boneOffset + aBoneIds.w];
        localPos = skin * localPos;
        localNormal = mat3(skin) * localNormal;
        localTangent = mat3(skin) * localTangent;
    }

    vec4 world = model * localPos;

    mat3 normalMat = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMat * localNormal);
    vec3 T = normalize(mat3(model) * localTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * aTangent.w;

    vs.worldPos = world.xyz;
    vs.uv = aUV;
    vs.tbn = mat3(T, B, N);
    vEmissiveScale = inst.emissiveScale;
    gl_Position = uViewProj * world;
}
