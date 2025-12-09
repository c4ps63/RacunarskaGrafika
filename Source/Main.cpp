#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include "../Header/Util.h"
#include "../Header/BitmapFont.h"

// Konstante
const unsigned int WINDOW_WIDTH = 1200;
const unsigned int WINDOW_HEIGHT = 800;
const float MAP_ZOOM = 0.15f; // Pokazuje 1% mape u režimu hodanja
const float WALK_SPEED = 0.002f; // Brzina kretanja
const float POINT_RADIUS = 0.015f; // Radijus tačke za klik detekciju
const int CIRCLE_SEGMENTS = 20;
BitmapFont* bitmapFont = nullptr;
unsigned int fontShader;
unsigned int fontTexture;
float potpisAlpha = 0.75f;

// --- Fullscreen kontrola ---
bool isFullscreen = false;

// Pozicija i veličina prozora prije prelaza u fullscreen
int windowPosX = 100;
int windowPosY = 100;
int windowedWidth = WINDOW_WIDTH;
int windowedHeight = WINDOW_HEIGHT;

// GLFW window referenca (mora biti globalna da bismo ga mijenjali)
GLFWwindow* window = nullptr;

void toggleFullscreen() {
    isFullscreen = !isFullscreen;

    if (isFullscreen) {
        // Sačuvaj windowed parametre
        glfwGetWindowPos(window, &windowPosX, &windowPosY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        glfwSetWindowMonitor(
            window,
            monitor,
            0, 0,
            mode->width,
            mode->height,
            mode->refreshRate
        );
    }
    else {
        glfwSetWindowMonitor(
            window,
            nullptr,
            windowPosX,
            windowPosY,
            windowedWidth,
            windowedHeight,
            0 // refresh rate ignorisan u windowed modu
        );
    }
}


// Strukture
struct Point {
    float x, y;
    Point(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Line {
    Point start, end;
    float distance;
};

// Globalne promenljive za stanje
enum Mode { WALKING, MEASURING };
Mode currentMode = WALKING;

// Stanje hodanja
Point mapOffset(0.0f, 0.0f); // Pozicija kamere na mapi
float walkingDistance = 0.0f;

// Stanje merenja
std::vector<Point> measurePoints;
std::vector<Line> measureLines;
float totalMeasureDistance = 0.0f;

// OpenGL objekti
unsigned int mapShader, colorShader, iconShader;
unsigned int mapVAO, mapVBO;
unsigned int pinVAO, pinVBO;
unsigned int pointVAO, pointVBO;
unsigned int lineVAO, lineVBO;
unsigned int iconVAO, iconVBO;
unsigned int mapTexture;
unsigned int walkIconTexture, measureIconTexture, centerIconTexture, textBgTexture, potpisTexture;

// Funkcija za konverziju screen koordinata u NDC
Point screenToNDC(double xpos, double ypos) {
    float x = (xpos / windowedWidth) * 2.0f - 1.0f;
    float y = -((ypos / windowedHeight) * 2.0f - 1.0f);
    return Point(x, y);
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    // AŽURIRAJ NOVE globalne vrijednosti
    windowedWidth = width;
    windowedHeight = height;
}

// Funkcija za računanje distance
float calculateDistance(Point p1, Point p2) {
    return sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
}

// Callback za klik miša
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        Point clickPos = screenToNDC(xpos, ypos);

        // --- DETEKCIJA IKONICE ---
        float iconCenterX = 0.78f;
        float iconCenterY = 0.78f;
        float iconScale = 0.3f;
        float halfSize = iconScale * 0.5f;

        bool clickedIcon =
            (clickPos.x > iconCenterX - halfSize &&
                clickPos.x < iconCenterX + halfSize &&
                clickPos.y > iconCenterY - halfSize &&
                clickPos.y < iconCenterY + halfSize);

        if (clickedIcon) {
            currentMode = (currentMode == WALKING) ? MEASURING : WALKING;
            return;
        }

        if (currentMode == MEASURING) {

            int clickedIndex = -1;
            for (int i = 0; i < measurePoints.size(); i++) {
                // Pretvaramo map space u NDC radi poređenja
                Point mpNDC;
                mpNDC.x = measurePoints[i].x * 2.0f - 1.0f;
                mpNDC.y = measurePoints[i].y * 2.0f - 1.0f;

                if (calculateDistance(mpNDC, clickPos) < POINT_RADIUS) {
                    clickedIndex = i;
                    break;
                }
            }

            if (clickedIndex != -1) {
                // Brisanje tačke
                measurePoints.erase(measurePoints.begin() + clickedIndex);
            }
            else {
                // Dodavanje nove tačke – konverzija NDC -> map space [0,1]
                Point mapSpace;
                mapSpace.x = (clickPos.x + 1.0f) / 2.0f;
                mapSpace.y = (clickPos.y + 1.0f) / 2.0f;
                measurePoints.push_back(mapSpace);
            }

            // Rekonstrukcija linija
            measureLines.clear();
            totalMeasureDistance = 0.0f;
            for (int i = 0; i < measurePoints.size() - 1; i++) {
                Line line;
                line.start = measurePoints[i];
                line.end = measurePoints[i + 1];
                line.distance = calculateDistance(line.start, line.end) * 1000.0f;
                measureLines.push_back(line);
                totalMeasureDistance += line.distance;
            }
        }
    }
}





// Callback za tastaturu
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        currentMode = (currentMode == WALKING) ? MEASURING : WALKING;
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {

        if (isFullscreen) {
            toggleFullscreen();  // vrati se u windowed
        }
        else {
            glfwSetWindowShouldClose(window, true); // ako je već windowed, izađi
        }
    }
}

// Inicijalizacija mape
void initMap() {
    float mapVertices[] = {
        // Pozicije      // Texture coords
        -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f,    0.0f, 1.0f
    };

    glGenVertexArrays(1, &mapVAO);
    glGenBuffers(1, &mapVBO);

    glBindVertexArray(mapVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mapVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mapVertices), mapVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// Inicijalizacija pina
void initPin() {
    std::vector<float> pinVertices;

    // Krug za pin (crveni)
    float centerX = 0.0f;
    float centerY = 0.0f;
    float radius = 0.03f;

    pinVertices.push_back(centerX);
    pinVertices.push_back(centerY);

    for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
        float angle = i * 2.0f * 3.14159f / CIRCLE_SEGMENTS;
        pinVertices.push_back(cos(angle) * radius + centerX);
        pinVertices.push_back(sin(angle) * radius + centerY);
    }

    glGenVertexArrays(1, &pinVAO);
    glGenBuffers(1, &pinVBO);

    glBindVertexArray(pinVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pinVBO);
    glBufferData(GL_ARRAY_BUFFER, pinVertices.size() * sizeof(float), pinVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// Inicijalizacija ikona
void initIcons() {
    float iconVertices[] = {
        //  aPos(x,y)       aTexCoord(u,v)
        -0.5f, -0.5f,       0.0f, 0.0f,
         0.5f, -0.5f,       1.0f, 0.0f,
         0.5f,  0.5f,       1.0f, 1.0f,
        -0.5f,  0.5f,       0.0f, 1.0f
    };

    glGenVertexArrays(1, &iconVAO);
    glGenBuffers(1, &iconVBO);

    glBindVertexArray(iconVAO);
    glBindBuffer(GL_ARRAY_BUFFER, iconVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(iconVertices), iconVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}


// Iscrtavanje linija
void drawLines() {
    if (measureLines.empty()) return;

    glUseProgram(colorShader);

    for (const auto& line : measureLines) {
        // Konvertuj iz map space [0,1] u NDC [-1,1]
        float startX = line.start.x * 2.0f - 1.0f;
        float startY = line.start.y * 2.0f - 1.0f;
        float endX = line.end.x * 2.0f - 1.0f;
        float endY = line.end.y * 2.0f - 1.0f;

        float lineVertices[] = {
            startX, startY,
            endX,   endY
        };

        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_DYNAMIC_DRAW);

        glBindVertexArray(lineVAO);
        unsigned int colorLoc = glGetUniformLocation(colorShader, "uColor");
        glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f); // Crna linija

        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, 2);
    }
}


// Iscrtavanje tačaka
void drawPoints() {
    if (measurePoints.empty()) return;

    glUseProgram(colorShader);

    for (const auto& point : measurePoints) {
        std::vector<float> circleVertices;
        float radius = 0.01f;

        // Konvertuj map space [0,1] -> NDC [-1,1]
        float ndcX = point.x * 2.0f - 1.0f;
        float ndcY = point.y * 2.0f - 1.0f;

        // Centar kruga
        circleVertices.push_back(ndcX);
        circleVertices.push_back(ndcY);

        // Generisanje kruga oko centra
        for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
            float angle = i * 2.0f * 3.14159f / CIRCLE_SEGMENTS;
            circleVertices.push_back(cos(angle) * radius + ndcX);
            circleVertices.push_back(sin(angle) * radius + ndcY);
        }

        glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
        glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float), circleVertices.data(), GL_DYNAMIC_DRAW);

        glBindVertexArray(pointVAO);
        unsigned int colorLoc = glGetUniformLocation(colorShader, "uColor");
        glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f); // Bela tačka

        glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
    }
}


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Map Measurement Tool", NULL, NULL);
    if (window == NULL) return endProgram("Prozor nije uspeo da se kreira.");
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    glfwMakeContextCurrent(window);
    if (glewInit() != GLEW_OK) return endProgram("GLEW nije uspeo da se inicijalizuje.");

    // Callbacks
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);

    // Učitaj kursor kompasa
    GLFWcursor* compassCursor = loadImageToCursor("Resources/compass.png");
    if (compassCursor) glfwSetCursor(window, compassCursor);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Kreiraj šejdere
    mapShader = createShader("Shaders/map.vert", "Shaders/map.frag");
    colorShader = createShader("Shaders/color.vert", "Shaders/color.frag");
    iconShader = createShader("Shaders/icon.vert", "Shaders/icon.frag");    
    fontShader = createShader("Shaders/font.vert", "Shaders/font.frag");
    // Učitaj teksture
    mapTexture = loadImageToTexture("Resources/novi-sad-map-0.png");
    fontTexture = loadImageToTextureRGBA("Resources/font.png");
    textBgTexture = loadImageToTextureRGBA("Resources/skrol.png");

    std::cout << "mapTexture ID: " << mapTexture << std::endl;  // Mora biti > 0

    if (mapTexture == 0) {
        std::cout << "GRESKA: Mapa nije ucitana!" << std::endl;
        return -1;  // Zaustavi program da vidiš grešku
    }
    walkIconTexture = loadImageToTextureRGBA("Resources/walk.png");
    measureIconTexture = loadImageToTextureRGBA("Resources/ruler.png");
    centerIconTexture = loadImageToTextureRGBA("Resources/centar.png");
    potpisTexture = loadImageToTextureRGBA("Resources/potpis.png");



    // Inicijalizuj geometriju
    initMap();
    initPin();
    initIcons();
    
    //DA LI SEOVDJE INICIJALIZUJE FONT???????
    bitmapFont = new BitmapFont();
    bitmapFont->Init(fontTexture, fontShader, 10, 1, '0');

    // Inicijalizuj VAO/VBO za tačke i linije
    glGenVertexArrays(1, &pointVAO);
    glGenBuffers(1, &pointVBO);
    glBindVertexArray(pointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    Point lastMapOffset = mapOffset;

    while (!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::high_resolution_clock::now();
        glClear(GL_COLOR_BUFFER_BIT);

        if (currentMode == WALKING) {
            // Obrada inputa za kretanje
            lastMapOffset = mapOffset;

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                mapOffset.y += WALK_SPEED;  // Gore
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                mapOffset.y -= WALK_SPEED;  // Dole (BILO JE -, TREBA +)
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                mapOffset.x -= WALK_SPEED;  // Levo
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                mapOffset.x += WALK_SPEED;  // Desno
            }

            // Ograniči kretanje na granicu mape
            mapOffset.x = fmax(-0.5f + MAP_ZOOM, fmin(0.5f - MAP_ZOOM, mapOffset.x));
            mapOffset.y = fmax(-0.5f + MAP_ZOOM, fmin(0.5f - MAP_ZOOM, mapOffset.y));

            // Ažuriraj pređenu distancu
            // Pretvori mapOffset u map space (isto kao measuring mode)
            // MAP_ZOOM je 0.15 -> pokriva samo 15% mape
            float visibleMapSize = MAP_ZOOM * 2.0f; // 0.3 u NDC
            Point lastGlobal((lastMapOffset.x + 0.5f) / 1.0f,
                (lastMapOffset.y + 0.5f) / 1.0f);

            Point currentGlobal((mapOffset.x + 0.5f) / 1.0f,
                (mapOffset.y + 0.5f) / 1.0f);

            walkingDistance += calculateDistance(lastGlobal, currentGlobal) * 1000.0f;


            // Iscrtaj mapu (zoom-ovanu)
            glUseProgram(mapShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mapTexture);
            glUniform1i(glGetUniformLocation(mapShader, "uTexture"), 0);
            glUniform2f(glGetUniformLocation(mapShader, "uOffset"), mapOffset.x, mapOffset.y);
            glUniform1f(glGetUniformLocation(mapShader, "uZoom"), MAP_ZOOM);

            glBindVertexArray(mapVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            // Iscrtaj pin
            // --- Iscrtaj pin IKONU U CENTRU EKRANA ---
            glUseProgram(iconShader);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, centerIconTexture);    // stavi pin ikoncu ako imaš
            glUniform1i(glGetUniformLocation(iconShader, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(iconShader, "uAlpha"), 1.0f);


            // Centar ekrana
            glUniform2f(glGetUniformLocation(iconShader, "uPos"), 0.0f, 0.0f);

            // Veličina (probaj 0.1f ili 0.15f)
            glUniform1f(glGetUniformLocation(iconShader, "uScale"), 0.15f);

            glBindVertexArray(iconVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            // Iscrtaj ikonu za hodanje
            glUseProgram(iconShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, walkIconTexture);
            glUniform1i(glGetUniformLocation(iconShader, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(iconShader, "uAlpha"), 1.0f);


            // pozicija ikone u gornjem desnom uglu
            glUniform2f(glGetUniformLocation(iconShader, "uPos"), 0.78f, 0.78f);
            glUniform1f(glGetUniformLocation(iconShader, "uScale"), 0.3f);
            glUniform1f(glGetUniformLocation(iconShader, "uAlpha"), 1.0f);


            glBindVertexArray(iconVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            // Ikonica za potpis
            glUseProgram(iconShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, potpisTexture);
            glUniform1i(glGetUniformLocation(iconShader, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(iconShader, "uAlpha"), potpisAlpha);


            // pozicija ikone u donjem desnom uglu
            glUniform2f(glGetUniformLocation(iconShader, "uPos"), 0.80f, -0.75f);
            glUniform1f(glGetUniformLocation(iconShader, "uScale"), 0.3f);

            glBindVertexArray(iconVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            // --- POZADINA ZA TEKST --- //
            glUseProgram(iconShader);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textBgTexture);
            glUniform1i(glGetUniformLocation(iconShader, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(iconShader, "uAlpha"), 1.0f);


            // Pozicija (NDC koordinate) — npr. gornji levi deo ekrana
            float bx = -0.735f;  
            float by =  0.735f;  

            glUniform2f(glGetUniformLocation(iconShader, "uPos"), bx, by);

            // Skaliranje pozadine (širina/visina)
            glUniform1f(glGetUniformLocation(iconShader, "uScale"), 0.4f);

            glBindVertexArray(iconVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            // TODO: Ispiši distancu na ekranu (potreban text rendering)
            //std::stringstream ss;
            //ss << std::fixed << std::setprecision(2);
            //ss << walkingDistance;
            //bitmapFont->RenderText(ss.str(), 75.0f, 95.0f, 0.7f, 0.0f, 0.0f, 0.0f); // Bela boja
            // Dobij glavni broj pre nule
            int displayNumber = static_cast<int>(walkingDistance); // samo ceo broj, bez decimala
            std::stringstream ss;
            ss << displayNumber;
            bitmapFont->RenderText(ss.str(), 75.0f, 95.0f, 0.7f, 0.0f, 0.0f, 0.0f);


        }
        else { // MEASURING mode
            // Iscrtaj celu mapu
            glUseProgram(mapShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mapTexture);
            glUniform1i(glGetUniformLocation(mapShader, "uTexture"), 0);
            glUniform2f(glGetUniformLocation(mapShader, "uOffset"), 0.0f, 0.0f);
            glUniform1f(glGetUniformLocation(mapShader, "uZoom"), 1.0f);

            glBindVertexArray(mapVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            // Iscrtaj linije i tačke
            drawLines();
            drawPoints();

            // Iscrtaj ikonu za merenje
            glUseProgram(iconShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, measureIconTexture);
            glUniform1i(glGetUniformLocation(iconShader, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(iconShader, "uAlpha"), 1.0f);
            glUniform2f(glGetUniformLocation(iconShader, "uPos"), 0.78f, 0.78f);
            glUniform1f(glGetUniformLocation(iconShader, "uScale"), 0.3f);

            glBindVertexArray(iconVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            // Ikonica za potpis
            glUseProgram(iconShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, potpisTexture);
            glUniform1i(glGetUniformLocation(iconShader, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(iconShader, "uAlpha"), potpisAlpha);


            // pozicija ikone u donjem desnom uglu
            glUniform2f(glGetUniformLocation(iconShader, "uPos"), 0.80f, -0.75f);
            glUniform1f(glGetUniformLocation(iconShader, "uScale"), 0.3f);

            glBindVertexArray(iconVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            // --- POZADINA ZA TEKST --- //
            glUseProgram(iconShader);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textBgTexture);
            glUniform1i(glGetUniformLocation(iconShader, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(iconShader, "uAlpha"), 1.0f);

            // Pozicija (NDC koordinate) — npr. gornji levi deo ekrana
            float bx = -0.735f;
            float by = 0.735f;

            glUniform2f(glGetUniformLocation(iconShader, "uPos"), bx, by);

            // Skaliranje pozadine (širina/visina)
            glUniform1f(glGetUniformLocation(iconShader, "uScale"), 0.4f);

            glBindVertexArray(iconVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);


            // TODO: Ispiši ukupnu distancu (potreban text rendering)
            int displayNumber = static_cast<int>(totalMeasureDistance); // samo ceo broj
            std::stringstream ss;
            ss << displayNumber;
            bitmapFont->RenderText(ss.str(), 75.0f, 95.0f, 0.7f, 0.0f, 0.0f, 0.0f);

        }

        glfwSwapBuffers(window);
        glfwPollEvents();
        
        // --- frame limiter ---
        auto frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> frameDuration = frameEnd - frameStart;
        float frameTime = 1.0f / 75.0f; // 75 FPS
        if (frameDuration.count() < frameTime) {
            std::this_thread::sleep_for(
                std::chrono::duration<float>(frameTime - frameDuration.count())
            );
        }
    }

    // Cleanup
    glDeleteVertexArrays(1, &mapVAO);
    glDeleteBuffers(1, &mapVBO);
    glDeleteVertexArrays(1, &pinVAO);
    glDeleteBuffers(1, &pinVBO);
    glDeleteVertexArrays(1, &pointVAO);
    glDeleteBuffers(1, &pointVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &lineVBO);
    glDeleteVertexArrays(1, &iconVAO);
    glDeleteBuffers(1, &iconVBO);

    glDeleteProgram(mapShader);
    glDeleteProgram(colorShader);
    glDeleteProgram(iconShader);
    delete bitmapFont;
    glDeleteProgram(fontShader);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}