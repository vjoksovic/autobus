#version 330 core

in vec2 chTex;
out vec4 outCol;

uniform sampler2D uTex0;
uniform sampler2D uTex1; 

void main()
{
    outCol = texture(uTex0, chTex);
 
} 