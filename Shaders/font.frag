#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;

void main()
{
    // Uzmi alpha kanal iz teksture (bela slova na providnoj pozadini)
    float alpha = texture(uTexture, TexCoord).a;
    
    // Primeni boju na slova
    FragColor = vec4(uColor.rgb, alpha);
}