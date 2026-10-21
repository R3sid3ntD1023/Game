#version 460 core

in vec2 vUV;

layout(binding = 0) uniform sampler2D uAlbedoMap;
uniform vec4 uBaseColor;
uniform float uAlphaCutoff;

void main() {
    if (texture(uAlbedoMap, vUV).a * uBaseColor.a < uAlphaCutoff)
        discard;
}
