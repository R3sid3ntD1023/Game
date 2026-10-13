#version 460 core

in vec3 vDir;
out vec4 outColor;

layout(binding = 6) uniform samplerCube uEnvMap;

void main() {
    outColor = vec4(textureLod(uEnvMap, normalize(vDir), 0.0).rgb, 1.0);
}
