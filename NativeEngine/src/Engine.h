#pragma once

#include <string>
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace NativeEngine {

    struct GameObject {
        std::string name;
        float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
        float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
        float scale = 1.0f;
        bool isActive = true;
    };

    class Engine {
    public:
        Engine();
        ~Engine();

        bool Initialize();
        void Run();
        void Shutdown();

    private:
        GLFWwindow* window;
        std::vector<GameObject> gameObjects;
        
        void SetupImGui();
        void RenderScene(unsigned int shaderProgram, unsigned int VAO);
        void RenderIDE();
        void ProcessInput();
    };

}
