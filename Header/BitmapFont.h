#pragma once
#include <GL/glew.h>
#include <string>

class BitmapFont {
private:
    unsigned int fontTexture;
    unsigned int shaderProgram;
    unsigned int VAO, VBO;

    int gridWidth;      // Broj karaktera po širini (16)
    int gridHeight;     // Broj karaktera po visini (6)
    int firstChar;      // Prvi ASCII karakter (32 = space)

    float charWidth;    // Širina jednog karaktera u texture koordinatama
    float charHeight;   // Visina jednog karaktera u texture koordinatama

public:
    BitmapFont();
    ~BitmapFont();

    // U?itaj font teksturu i postavi parametre
    void Init(unsigned int texture, unsigned int shader, int gridW = 16, int gridH = 6, int firstASCII = 32);

    // Renderuj tekst na ekranu
    // x, y - screen koordinate (0,0 = top-left, width,height = bottom-right)
    // scale - veli?ina (1.0 = normalna, 2.0 = duplo ve?a)
    // r, g, b - boja teksta (0.0 - 1.0)
    void RenderText(const std::string& text, float x, float y, float scale, float r, float g, float b);

private:
    // Dobavi texture koordinate za odre?eni karakter
    void GetCharUV(char c, float& u1, float& v1, float& u2, float& v2);
};