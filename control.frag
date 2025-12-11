in vec2 chTex;
out vec4 outCol;

uniform sampler2D tex;

void main()
{
    outCol = texture(tex, chTex);
}