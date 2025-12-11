#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <random>

int endProgram(std::string message);
unsigned int createShader(const char* vsSource, const char* fsSource);
unsigned loadImageToTexture(const char* filePath);
GLFWcursor* loadImageToCursor(const char* filePath);
int randomInt(int min, int max);
void preprocessTexture(unsigned& texture, const char* filepath);