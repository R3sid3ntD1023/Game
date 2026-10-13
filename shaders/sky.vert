#version 460 core

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;

out vec3 vDir;

void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vec2 ndc = p * 2.0 - 1.0;
    vec4 far = uInvViewProj * vec4(ndc, 1.0, 1.0);
    vDir = far.xyz / far.w - uCameraPos;
    gl_Position = vec4(ndc, 1.0, 1.0); // depth = 1.0, drawn with GL_LEQUAL behind all geometry
}
