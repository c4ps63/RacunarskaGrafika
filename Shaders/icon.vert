#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform vec2 uPos;     // pozicija ikone u NDC
uniform float uScale;  // veli?ina ikone

void main()
{
    vec2 scaled = aPos * uScale;
    vec2 positioned = scaled + uPos;

    gl_Position = vec4(positioned, 0.0, 1.0);
    TexCoord = aTexCoord;
}
