#version 460 core

in vec2 vUV;
in vec4 vColor;
flat in int vMode;

out vec4 outColor;

layout(binding = 0) uniform sampler2D uAtlas; // R8: glyph coverage, solid white block for rectangles

void main() {

    vec4 tex = texture(uAtlas, vUV);

    if(vMode == 1)
    {
        outColor = vColor * tex;
    }
    else
    {  
        outColor = vec4(vColor.rgb, vColor.a * tex.r);
    }
}
