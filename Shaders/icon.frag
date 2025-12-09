#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uAlpha;   

void main()
{
    //FragColor = texture(uTexture, TexCoord);
    vec4 tex = texture(uTexture, TexCoord);
    FragColor = vec4(tex.rgb, tex.a * uAlpha);
}