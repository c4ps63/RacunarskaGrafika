#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform vec2 uOffset;
uniform float uZoom;

void main()
{
    // U walking režimu: zoom na deo mape
    // U measuring režimu: prikaži celu mapu
    
    // Texture koordinate se skaliraju i pomeraju
    TexCoord = (aTexCoord - 0.5) * uZoom + 0.5 + uOffset;
    
    gl_Position = vec4(aPos, 0.0, 1.0);
}