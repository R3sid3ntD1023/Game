#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;

layout(std430, binding = 0) readonly buffer Models {
    mat4 models[];
};

uniform mat4 uViewProj;

out VS_OUT {
    vec3 worldPos;
    vec2 uv;
    mat3 tbn;
} vs;

void main() {
    // gl_InstanceID does not include baseInstance, so add it explicitly.
    mat4 model = models[gl_BaseInstance + gl_InstanceID];
    vec4 world = model * vec4(aPosition, 1.0);

    mat3 normalMat = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMat * aNormal);
    vec3 T = normalize(mat3(model) * aTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * aTangent.w;

    vs.worldPos = world.xyz;
    vs.uv = aUV;
    vs.tbn = mat3(T, B, N);
    gl_Position = uViewProj * world;
}
