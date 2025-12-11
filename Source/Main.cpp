#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <map>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "../Header/Util.h"

unsigned bus;
unsigned doorClosed;
unsigned doorOpen;
unsigned control;

bool isOpen = true;
bool controlEntered = false;
bool controlOn = false;

int travelers = 0;
int punishments = 0;
bool hasStopped = false;

int screenWidth = 800;
int screenHeight = 800;
float position = 0.03f;
float speed = 0.0004f;

GLFWcursor* cursor;

enum BusState {
    MOVING,
    STOPPED_AT_STATION
};

unsigned int stationShader;
unsigned int busShader;
unsigned int textShader;
unsigned int lineShader;
unsigned int doorShader;
unsigned int controlShader;

BusState busState = MOVING;
float stopTimer = 0.0f;          
const float STOP_DURATION = 10.0f; 
float originalSpeed = speed;      

struct Character {
    unsigned int TextureID; 
    float SizeX, SizeY;     
    float BearingX, BearingY; 
    unsigned int Advance;   
};

std::map<char, Character> Characters;
unsigned int textVAO, textVBO;
unsigned int VAOcontrol, VBOcontrol, EBOcontrol;

void static manageTravelers(GLFWwindow* window, int button, int action, int mods) {
    if (isOpen) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && travelers < 50)
            travelers++;
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS && travelers > 0)
            travelers--;
    }
}

void static loadFont(const char* fontPath) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "Greška u inicijalizaciji FreeType\n";
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath, 0, &face)) {
        std::cout << "Greška u učitavanju fonta\n";
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48); 

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            (float)face->glyph->bitmap.width,
            (float)face->glyph->bitmap.rows,
            (float)face->glyph->bitmap_left,
            (float)face->glyph->bitmap_top,
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void static callControl(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_K && action == GLFW_PRESS && isOpen) {
        if (!controlEntered) {
            controlOn = !controlOn;
            if (controlOn) {
                travelers++;
                controlEntered = true;
            }
        }
    }
}
bool static isBusNear(float busX, float busY, const std::vector<float>& stations, float threshold = 0.06f)
{
    bool nearAnyStation = false;

    for (size_t i = 0; i < stations.size(); i += 2) {
        float sx = stations[i];
        float sy = stations[i + 1];

        float dx = busX - sx;
        float dy = busY - sy;

        if (dx * dx + dy * dy < threshold * threshold) {
            nearAnyStation = true;
            break;
        }
    }

    isOpen = nearAnyStation;

    return nearAnyStation;
}

void static updateBus(float busX, float busY, std::vector<float> stations, float& position, float deltaTime) {
    bool nearStation = isBusNear(busX, busY, stations, 0.06f);

    if (busState == MOVING) {
        if (nearStation && !hasStopped) {
            busState = STOPPED_AT_STATION;
            stopTimer = 0.0f;
            if (controlOn) {
                controlOn = false;
                controlEntered = false;
                punishments += randomInt(0, travelers - 1);
                travelers--;
            }
            hasStopped = true;
        }
        else {
            position += originalSpeed * deltaTime * 60.0f;
            if (position >= 1.0f) position -= 1.0f;
        }
    }
    else if (busState == STOPPED_AT_STATION) {
        stopTimer += deltaTime;
        if (stopTimer >= 10.0f) {
            busState = MOVING;
            std::cout << "Autobus kreće sa stanice!" << std::endl;
        }
    }
    if (!nearStation) {
        hasStopped = false;
    }
}

float static catRom(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5f * ((2 * p1) +
        (-p0 + p2) * t +
        (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 +
        (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}

void formVAOs(float* curveVertices, size_t curveSize, unsigned int& VAOcurve,
    float* stationVertices, size_t stationSize, unsigned int& VAOstation,
    float* busVertices, size_t busSize, unsigned int& VAObus,
    unsigned int* busIndices, size_t indicesSize, unsigned int& EBObus,
    float* doorQuad, size_t doorSize, unsigned int& VAOdoor, unsigned int& EBOdoor,
    float* controlQuad, size_t controlSize, unsigned int& VAOcontrol, unsigned int& EBOcontrol) {

    // Curve VAO setup
    unsigned int VBOcurve;
    glGenVertexArrays(1, &VAOcurve);
    glGenBuffers(1, &VBOcurve);
    glBindVertexArray(VAOcurve);
    glBindBuffer(GL_ARRAY_BUFFER, VBOcurve);
    glBufferData(GL_ARRAY_BUFFER, curveSize, curveVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Station VAO setup
    unsigned int VBOstation;
    glGenVertexArrays(1, &VAOstation);
    glGenBuffers(1, &VBOstation);
    glBindVertexArray(VAOstation);
    glBindBuffer(GL_ARRAY_BUFFER, VBOstation);
    glBufferData(GL_ARRAY_BUFFER, stationSize, stationVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Bus VAO setup
    unsigned int VBObus;
    glGenVertexArrays(1, &VAObus);
    glGenBuffers(1, &VBObus);
    glGenBuffers(1, &EBObus);
    glBindVertexArray(VAObus);
    glBindBuffer(GL_ARRAY_BUFFER, VBObus);
    glBufferData(GL_ARRAY_BUFFER, busSize, busVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBObus);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesSize, busIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Door VAO setup
    unsigned int VBOdoor;
    unsigned int doorIdx[] = { 0, 1, 2, 0, 2, 3 };
    glGenVertexArrays(1, &VAOdoor);
    glGenBuffers(1, &VBOdoor);
    glGenBuffers(1, &EBOdoor);
    glBindVertexArray(VAOdoor);
    glBindBuffer(GL_ARRAY_BUFFER, VBOdoor);
    glBufferData(GL_ARRAY_BUFFER, doorSize, doorQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOdoor);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(doorIdx), doorIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Control VAO setup
    unsigned int VBOcontrol;
    unsigned int controlIdx[] = { 0, 1, 2, 0, 2, 3 };
    glGenVertexArrays(1, &VAOcontrol);
    glGenBuffers(1, &VBOcontrol);
    glGenBuffers(1, &EBOcontrol);
    glBindVertexArray(VAOcontrol);
    glBindBuffer(GL_ARRAY_BUFFER, VBOcontrol);
    glBufferData(GL_ARRAY_BUFFER, controlSize, controlQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOcontrol);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(controlIdx), controlIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void static drawNumbers(unsigned int shader, std::string text, float x, float y, float scale, float colorR, float colorG, float colorB) {
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST); 

    glUseProgram(shader);

    float projection[16] = {
        2.0f / screenWidth, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / screenHeight, 0.0f, 0.0f, 
        0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f 
    };
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, projection);

    glUniform3f(glGetUniformLocation(shader, "textColor"), colorR, colorG, colorB);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (auto c : text) {
        Character ch = Characters[c];

        float xpos = x + ch.BearingX * scale;
        float ypos = y - (ch.SizeY - ch.BearingY) * scale;

        float w = ch.SizeX * scale;
        float h = ch.SizeY * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 1.0f },
            { xpos,     ypos,       0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 0.0f },

            { xpos,     ypos + h,   0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 0.0f },
            { xpos + w, ypos + h,   1.0f, 1.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
}

void static drawText(unsigned int shader, std::string text, float x, float y, float scale, float colorR, float colorG, float colorB) {
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(shader);

    float projection[16] = {
        2.0f / screenWidth, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / screenHeight, 0.0f, 0.0f, 
        0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f 
    };
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, projection);

    glUniform3f(glGetUniformLocation(shader, "textColor"), colorR, colorG, colorB);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (auto c : text) {
        Character ch = Characters[c];

        float xpos = x + ch.BearingX * scale;
        float ypos = y - (ch.BearingY * scale);

        float w = ch.SizeX * scale;
        float h = ch.SizeY * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 1.0f },
            { xpos,     ypos,       0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 0.0f },

            { xpos,     ypos + h,   0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 0.0f },
            { xpos + w, ypos + h,   1.0f, 1.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
}

std::vector<float> static drawStations(unsigned int stationShader, unsigned int textShader,
    unsigned int VAOstation, int numStations, float* points, float* stationVertices) {

    glEnable(GL_PROGRAM_POINT_SIZE);

    std::vector<float> stationPositions;
    stationPositions.reserve(numStations * 2);

    glUseProgram(stationShader);
    float intensity = 0.7f + 0.3f * sin(0.008f * 6.28318f);
    glUniform3f(glGetUniformLocation(stationShader, "uColor"), intensity, 0.0f, 0.0f);
    glPointSize(80.0f);
    glBindVertexArray(VAOstation);
    glDrawArrays(GL_POINTS, 0, numStations);

    for (int i = 0; i < numStations; i++) {
        float ndcX = points[i * 2];
        float ndcY = points[i * 2 + 1];
        float pixelX = (ndcX + 1.0f) * 0.5f * screenWidth - 12.0f;
        float pixelY = (1.0f - ndcY) * 0.5f * screenHeight - 12.0f; 
        drawNumbers(textShader, std::to_string(i), pixelX, pixelY, 0.6f, 1.0f, 1.0f, 1.0f);

        stationPositions.push_back(ndcX);
        stationPositions.push_back(ndcY);
    }

    return stationPositions;
}

void static drawCurve(unsigned int stationShader, unsigned int VAOcurve, int curvePoints) {
    glUseProgram(stationShader);
    glLineWidth(6.0f);          
    glEnable(GL_LINE_SMOOTH);    
    glBindVertexArray(VAOcurve);
    glDrawArrays(GL_LINE_LOOP, 0, curvePoints);
}

void static drawBus(unsigned int busShader, unsigned int VAObus, unsigned int busTexture,
    std::vector<float>& curve, int curvePoints, float& position,
    std::vector<float> stations, float deltaTime)
{
    float floatIndex = position * curvePoints;
    int i0 = (int)floatIndex % curvePoints;
    int i1 = (i0 + 1) % curvePoints;
    float t = floatIndex - (int)floatIndex;
    float busX = curve[i0 * 6] + t * (curve[i1 * 6] - curve[i0 * 6]);
    float busY = curve[i0 * 6 + 1] + t * (curve[i1 * 6 + 1] - curve[i0 * 6 + 1]);

	updateBus(busX, busY, stations, position, deltaTime);

    glUseProgram(busShader);
    glUniform2f(glGetUniformLocation(busShader, "uPos"), busX, busY);
    glUniform1f(glGetUniformLocation(busShader, "uB"), 0.0f);
    glUniform1f(glGetUniformLocation(busShader, "uA"), 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, busTexture);
    glUniform1i(glGetUniformLocation(busShader, "uTex0"), 0);
    glBindVertexArray(VAObus);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void static drawDoor(unsigned int doorShader, unsigned int VAOdoor, bool isOpen)
{
    GLint lastProgram = 0;
    GLint lastVAO = 0;
    GLint lastTexture = 0;
    GLint lastViewport[4] = {0, 0, 0, 0};
    GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);

    glGetIntegerv(GL_CURRENT_PROGRAM, &lastProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &lastVAO);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
    glGetIntegerv(GL_VIEWPORT, lastViewport);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(doorShader);

    const float doorWidth  = 700.0f;
    const float doorHeight = 900.0f;
    const float doorX = 60.0f;     
    const float doorY = 80.0f;      

    float ortho[16] = {
        2.0f / screenWidth, 0.0f,               0.0f, 0.0f,
        0.0f,               2.0f / screenHeight, 0.0f, 0.0f,
        0.0f,               0.0f,               1.0f, 0.0f,
        -1.0f + 2.0f * doorX / screenWidth,
        -1.0f + 2.0f * doorY / screenHeight,
        0.0f, 1.0f
    };

    GLint projLoc = glGetUniformLocation(doorShader, "projection");
    if (projLoc != -1)
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, ortho);

    GLint scaleLoc = glGetUniformLocation(doorShader, "scale");
    if (scaleLoc != -1)
        glUniform2f(scaleLoc, doorWidth, doorHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, isOpen ? doorOpen : doorClosed);

    GLint texLoc = glGetUniformLocation(doorShader, "tex");
    if (texLoc != -1)
        glUniform1i(texLoc, 0);

    glBindVertexArray(VAOdoor);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glUseProgram(lastProgram);
    glBindVertexArray(lastVAO);
    glBindTexture(GL_TEXTURE_2D, lastTexture);
    glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);

    if (depthTestWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}

void static drawControl(unsigned int controlShader)
{
    GLint lastProgram, lastVAO, lastTexture, lastViewport[4];
    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_CURRENT_PROGRAM, &lastProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &lastVAO);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
    glGetIntegerv(GL_VIEWPORT, lastViewport);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(controlShader);

    const float controlWidth = 150.0f;
    const float controlHeight = 150.0f;
    const float controlX = 120.0f;      
    const float controlY = screenHeight - controlHeight - 20.0f;  

    float ortho[16] = {
        2.0f / screenWidth, 0.0f,               0.0f, 0.0f,
        0.0f,               2.0f / screenHeight, 0.0f, 0.0f,
        0.0f,               0.0f,               1.0f, 0.0f,
        -1.0f + 2.0f * controlX / screenWidth,
        -1.0f + 2.0f * controlY / screenHeight,
        0.0f, 1.0f
    };

    GLint projLoc = glGetUniformLocation(controlShader, "projection");
    if (projLoc != -1)
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, ortho);

    GLint scaleLoc = glGetUniformLocation(controlShader, "scale");
    if (scaleLoc != -1)
        glUniform2f(scaleLoc, controlWidth, controlHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, control);

    GLint texLoc = glGetUniformLocation(controlShader, "tex");
    if (texLoc != -1)
        glUniform1i(texLoc, 0);

    glBindVertexArray(VAOcontrol);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glUseProgram(lastProgram);
    glBindVertexArray(lastVAO);
    glBindTexture(GL_TEXTURE_2D, lastTexture);
    glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
}

void static setup() {
    preprocessTexture(bus, "./Resources/bus.png");
    preprocessTexture(doorClosed, "./Resources/door_closed.png");
    preprocessTexture(doorOpen, "./Resources/door_open.png");
    preprocessTexture(control, "./Resources/control.png");
    loadFont("./Resources/ARIAL.TTF");

    stationShader = createShader("station.vert", "station.frag");
    busShader = createShader("bus.vert", "bus.frag");
    textShader = createShader("text.vert", "text.frag");
    lineShader = createShader("line.vert", "line.frag");
    doorShader = createShader("door.vert", "door.frag");
    controlShader = createShader("control.vert", "control.frag");
}

int main()
{
    glfwInit();
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    screenWidth = mode->width;
    screenHeight = mode->height;
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Autobus", monitor, NULL);
    if (window == NULL) {
        std::cout << "GRESKA: Prozor nije kreiran!" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSetMouseButtonCallback(window, manageTravelers);
    glfwSetKeyCallback(window, callControl);

    cursor = loadImageToCursor("Resources/cursor.png");
    glfwSetCursor(window, cursor);

    if (glewInit() != GLEW_OK) {
        std::cout << "GRESKA: GLEW init failed!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    setup();

    float points[20] = {
        -0.15f,  0.80f,
         0.20f,  0.75f,
         0.55f,  0.55f,
         0.70f,  0.20f,
         0.55f, -0.25f,
         0.30f, -0.55f,
        -0.05f, -0.65f,
        -0.45f, -0.55f,
        -0.70f, -0.15f,
        -0.60f,  0.40f
    };

    std::vector<float> curve;
    const int N = 10;
    const int SEG = 30;
    float intensity = 0.7f + 0.3f * sin((0.008f-1) * 6.28318f);

    for (int i = 0; i < N; i++) {
        int i0 = (i - 1 + N) % N;
        int i1 = i;
        int i2 = (i + 1) % N;
        int i3 = (i + 2) % N;

        float x0 = points[i0 * 2], y0 = points[i0 * 2 + 1];
        float x1 = points[i1 * 2], y1 = points[i1 * 2 + 1];
        float x2 = points[i2 * 2], y2 = points[i2 * 2 + 1];
        float x3 = points[i3 * 2], y3 = points[i3 * 2 + 1];

        for (int s = 0; s < SEG; s++) {
            float t = (float)s / (float)SEG;
            float x = catRom(x0, x1, x2, x3, t);
            float y = catRom(y0, y1, y2, y3, t);
            curve.push_back(x);
            curve.push_back(y);
            curve.push_back(intensity); 
            curve.push_back(0.0f); 
            curve.push_back(0.0f); 
            curve.push_back(1.0f); 
        }
    }

    int curvePoints = curve.size() / 6; 
    float stationsWithColor[60]; 
    for (int i = 0; i < N; i++) {
        stationsWithColor[i * 6 + 0] = points[i * 2];    
        stationsWithColor[i * 6 + 1] = points[i * 2 + 1]; 
        stationsWithColor[i * 6 + 2] = intensity; 
        stationsWithColor[i * 6 + 3] = 0.0f; 
        stationsWithColor[i * 6 + 4] = 0.0f; 
        stationsWithColor[i * 6 + 5] = 1.0f; 
    }


    float busVertices[] = {
    -0.08f, -0.1f,   0.0f, 0.0f,   0.0f, 1.0f,
     0.08f, -0.1f,   1.0f, 0.0f,   0.0f, 1.0f,
     0.08f,  0.1f,   1.0f, 1.0f,   0.0f, 1.0f,
    -0.08f,  0.1f,   0.0f, 1.0f,   0.0f, 1.0f
    };

    unsigned int busIndices[] = {
        0, 1, 2,
        2, 3, 0
    };

    unsigned int doorIdx[] = { 0,1,2, 0,2,3 };

    float doorQuad[] = {
    0.0f,        0.15f,   0.0f, 1.0f,
    0.15f,       0.15f,   1.0f, 1.0f,
    0.15f,       0.0f,    1.0f, 0.0f,
    0.0f,        0.0f,    0.0f, 0.0f
    };

    float controlQuad[] = {
        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 0.0f,  0.0f, 0.0f
    };

    unsigned int VAOcurve, VAOstation, VAObus, EBObus;
    unsigned int VAOdoor, EBOdoor, VBOdoor;
    unsigned int EBOcontrol; 

    formVAOs(curve.data(), curve.size() * sizeof(float), VAOcurve,
        stationsWithColor, sizeof(stationsWithColor), VAOstation,
        busVertices, sizeof(busVertices), VAObus,
        busIndices, sizeof(busIndices), EBObus,
        doorQuad, sizeof(doorQuad), VAOdoor, EBOdoor,
        controlQuad, sizeof(controlQuad), VAOcontrol, EBOcontrol);

    glClearColor(0.3f, 0.5f, 0.8f, 1.0f);
    float lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) 
            break;

        glClear(GL_COLOR_BUFFER_BIT);

        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        drawDoor(doorShader, VAOdoor, isOpen);
        drawCurve(lineShader, VAOcurve, curvePoints);
        std::vector<float> stations = drawStations(stationShader, textShader, VAOstation, N, points, stationsWithColor);
        drawBus(busShader, VAObus, bus, curve, curvePoints, position, stations, deltaTime);
        if (controlOn) {
            drawControl(controlShader);  
        }

        std::string info = "Broj putnika: " + std::to_string(travelers) +
            "  Broj naplacenih kazni: " + std::to_string(punishments);

        drawText(textShader, info,
            screenWidth - 730.0f,     
            60.0f,                   
            0.8f,                    
            0.6f, 0.3f, 0.2f);      

        drawText(textShader,
            "Veljko Joksovic SV56/2022",
            screenWidth - 450.0f,                    
            screenHeight - 50.0f,     
            0.7f,
            0.8f, 0.6f, 0.4f);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    std::cout << "Zatvaranje programa..." << std::endl;
    glDeleteProgram(stationShader);
    glDeleteProgram(busShader);
    glDeleteProgram(textShader);
	glDeleteProgram(lineShader);
	glDeleteProgram(doorShader);
	glDeleteProgram(controlShader);
    glDeleteVertexArrays(1, &VAOcurve);
    glDeleteVertexArrays(1, &VAOstation);
    glDeleteVertexArrays(1, &VAObus);
    glDeleteVertexArrays(1, &textVAO);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}