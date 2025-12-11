#version 330 core
layout (location = 0) in vec2 aPos;      
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;
uniform vec2 scale = vec2(200.0, 300.0);  
uniform vec2 offset = vec2(50.0, 50.0);    

void main()
{
    vec2 pos = aPos * scale + offset;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
    TexCoord = aTexCoord;
}