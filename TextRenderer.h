#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#include <string>

struct Character {
    unsigned int TextureID;
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    unsigned int Advance;
};

class TextRenderer {
public:
    std::map<char, Character> Characters;
    unsigned int VAO, VBO;
    unsigned int shaderProgram;

    TextRenderer(unsigned int shader);
    void LoadFont(const char* fontPath, unsigned int fontSize);
    void RenderText(std::string text, float x, float y, float scale, glm::vec3 color);
};