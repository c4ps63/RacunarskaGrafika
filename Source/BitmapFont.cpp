#include "../Header/BitmapFont.h"
#include <iostream>

// Reference na window dimenzije iz main.cpp
const unsigned int WINDOW_WIDTH = 1200;
const unsigned int WINDOW_HEIGHT = 800;

BitmapFont::BitmapFont()
    : fontTexture(0), shaderProgram(0), VAO(0), VBO(0),
    gridWidth(10), gridHeight(1), firstChar('0'),
    charWidth(0.0f), charHeight(0.0f) {
}

BitmapFont::~BitmapFont() {
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
}

void BitmapFont::Init(unsigned int texture, unsigned int shader, int gridW, int gridH, int firstASCII) {
    fontTexture = texture;
    shaderProgram = shader;
    gridWidth = gridW;
    gridHeight = gridH;
    firstChar = firstASCII;

    // Izra?unaj dimenzije jednog karaktera u texture koordinatama
    charWidth = 1.0f / gridWidth;
    charHeight = 1.0f / gridHeight;

    // Kreiraj VAO i VBO za quad rendering
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Alocira prostor za 6 verteksa (2 trougla) * 4 floata (x, y, u, v)
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    // Position attribute (location 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute (location 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    std::cout << "BitmapFont inicijalizovan: Grid " << gridWidth << "x" << gridHeight << std::endl;
}

void BitmapFont::GetCharUV(char c, float& u1, float& v1, float& u2, float& v2) {
    // Izra?unaj poziciju karaktera u grid-u
    int charIndex = (int)c - firstChar;

    // Ako karakter nije u opsegu, koristi space
    if (charIndex < 0 || charIndex >= gridWidth * gridHeight) {
        charIndex = 0; // Space
    }

    int col = charIndex % gridWidth;
    int row = charIndex / gridWidth;

    // Texture koordinate (0,0 je top-left u našoj slici)
    u1 = col * charWidth;
    v1 = 1.0f - row * charHeight;
    u2 = u1 + charWidth;
    v2 = v1 - charHeight;
}

void BitmapFont::RenderText(const std::string& text, float x, float y, float scale, float r, float g, float b) {
    if (fontTexture == 0 || shaderProgram == 0) {
        std::cout << "BitmapFont nije inicijalizovan!" << std::endl;
        return;
    }

    // Aktiviraj shader i teksturu
    glUseProgram(shaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glUniform1i(glGetUniformLocation(shaderProgram, "uTexture"), 0);

    // Postavi boju (ako shader ima uColor uniform)
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    if (colorLoc != -1) {
        glUniform4f(colorLoc, r, g, b, 1.0f);
    }

    glBindVertexArray(VAO);

    // Veli?ina jednog karaktera na ekranu (u pikselima)
    float charScreenWidth = 32.0f * scale;  // 32px je bazna širina karaktera
    float charScreenHeight = 32.0f * scale;

    float startX = x;
    float currentX = x;
    float currentY = y;

    for (char c : text) {
        // Novi red
        if (c == '\n') {
            currentX = startX;
            currentY += charScreenHeight;
            continue;
        }

        // Space - samo pomeri kursor
        if (c == ' ') {
            currentX += charScreenWidth * 0.5f; // Space je manji od obi?nog karaktera
            continue;
        }

        // Dobavi texture koordinate za karakter
        float u1, v1, u2, v2;
        GetCharUV(c, u1, v1, u2, v2);

        // Pretvori screen koordinate u NDC (Normalized Device Coordinates)
        float x1 = (currentX / WINDOW_WIDTH) * 2.0f - 1.0f;
        float y1 = -((currentY / WINDOW_HEIGHT) * 2.0f - 1.0f);
        float x2 = ((currentX + charScreenWidth) / WINDOW_WIDTH) * 2.0f - 1.0f;
        float y2 = -(((currentY + charScreenHeight) / WINDOW_HEIGHT) * 2.0f - 1.0f);

        // Kreiraj quad za karakter (2 trougla)
        float vertices[6][4] = {
            // Pozicija (x, y)    Texture (u, v)
            { x1, y1,             u1, v1 },  // Top-left
            { x1, y2,             u1, v2 },  // Bottom-left
            { x2, y2,             u2, v2 },  // Bottom-right

            { x1, y1,             u1, v1 },  // Top-left
            { x2, y2,             u2, v2 },  // Bottom-right
            { x2, y1,             u2, v1 }   // Top-right
        };

        // Ažuriraj VBO sa vertex podacima
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Renderuj karakter
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Pomeri kursor za slede?i karakter
        currentX += charScreenWidth;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}