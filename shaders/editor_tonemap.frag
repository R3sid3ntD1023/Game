#version 460 core

layout(binding = 0) uniform sampler2D uScene;

uniform float uExposure;
uniform int uTonemap; // 0: copy as is

out vec4 outColor;

vec3 ACESFilm(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 c = texelFetch(uScene, ivec2(gl_FragCoord.xy), 0).rgb;
    if (uTonemap == 1)
        c = pow(ACESFilm(c * uExposure), vec3(1.0 / 2.2));
    outColor = vec4(c, 1.0);
}
