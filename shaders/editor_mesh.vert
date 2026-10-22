#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uViewProj;
uniform mat4 uModel;

layout(location = 2) in vec2 aUV;

out vec3 vNormal;
out vec3 vWorld;
out vec2 vUV;

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorld = world.xyz;
    vUV = aUV;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uViewProj * world;
}
