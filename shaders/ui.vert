#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in int aMode;

uniform vec2 uScreen; // framebuffer size in pixels

out vec2 vUV;
out vec4 vColor;
flat out int vMode;

void main() {
    vUV = aUV;
    vColor = aColor;
    vMode = aMode;
    gl_Position = vec4(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0, 0.0, 1.0);
}
