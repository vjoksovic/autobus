#version 330 core

in vec4 channelCol;
out vec4 outCol;

uniform vec3 uColor;

void main()
{
    vec2 coord = gl_PointCoord - vec2(0.5);
    if(length(coord) > 0.5)
        discard;
    outCol = vec4(uColor, 1.0);
    
}