#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aUV;

uniform mat4 uViewProj;

out float vSlope;
out vec2 vWorldXZ;

void main() {
    vSlope = aUV.x;
    vWorldXZ = aPosition.xz;
    gl_Position = uViewProj * vec4(aPosition, 1.0);
}
