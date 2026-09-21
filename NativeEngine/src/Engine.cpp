#include "Engine.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Simple Vertex Shader
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// Simple Fragment Shader
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 objectColor;
void main() {
    FragColor = vec4(objectColor, 1.0);
}
)";

namespace NativeEngine {

    Engine::Engine() : window(nullptr) {
        // Initialize default game object
        GameObject cube;
        cube.name = "MainCube";
        cube.posZ = -5.0f;
        gameObjects.push_back(cube);
    }

    Engine::~Engine() {}

    bool Engine::Initialize() {
        // Init GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        // OpenGL Version 3.3 Core Profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(1280, 720, "NativeEngine by Dynamic Productions", NULL, NULL);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Enable vsync

        // Init GLEW
        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW" << std::endl;
            return false;
        }

        // Setup ImGui
        SetupImGui();

        std::cout << "NativeEngine Initialized Successfully!" << std::endl;
        return true;
    }

    void Engine::SetupImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup Style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void Engine::Run() {
        // Compile Shaders
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        GLuint shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Cube Vertices
        float vertices[] = {
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
             0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
        };

        GLuint VBO, VAO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glEnable(GL_DEPTH_TEST);

        while (!glfwWindowShouldClose(window)) {
            ProcessInput();

            // Render
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Start ImGui Frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Render IDE UI
            RenderIDE();

            // Render 3D Scene
            RenderScene(shaderProgram, VAO);

            // End ImGui Frame
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteProgram(shaderProgram);
    }

    void Engine::RenderScene(unsigned int shaderProgram, unsigned int VAO) {
        glUseProgram(shaderProgram);

        // Projection Matrix
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // View Matrix
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -6.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glBindVertexArray(VAO);
        
        for (const auto& obj : gameObjects) {
            if (!obj.isActive) continue;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(obj.posX, obj.posY, obj.posZ));
            model = glm::rotate(model, glm::radians(obj.rotX), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(obj.rotY), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(obj.rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(obj.scale));

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            
            // Set color based on selection (simplified)
            glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.2f, 0.6f, 0.9f);
            
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    void Engine::RenderIDE() {
        // Main DockSpace (Simplified for compatibility)
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        
        ImGui::Begin("NativeEngine IDE", nullptr, 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar);

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Project")) {}
                if (ImGui::MenuItem("Open Project")) {}
                if (ImGui::MenuItem("Save Project")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(window, true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo")) {}
                if (ImGui::MenuItem("Redo")) {}
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About NativeEngine")) {}
                ImGui::EndMenu();
            }
            // Prototype Badge in Menu Bar
            ImGui::SameLine(ImGui::GetWindowWidth() - 150);
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "PROTOTYPE");
            ImGui::EndMenuBar();
        }

        // Create docked panels manually
        ImGui::Columns(2, "MyColumns");
        
        // Left Panel - Hierarchy
        ImGui::BeginChild("LeftPane", ImVec2(0, 0), true);
        ImGui::Text("Hierarchy");
        ImGui::Separator();
        for (size_t i = 0; i < gameObjects.size(); ++i) {
            if (ImGui::Selectable(gameObjects[i].name.c_str(), false)) {
                // Selection logic could go here
            }
        }
        if (ImGui::Button("Add Cube")) {
            GameObject newCube;
            newCube.name = "Cube_" + std::to_string(gameObjects.size());
            newCube.posZ = -5.0f;
            gameObjects.push_back(newCube);
        }
        ImGui::EndChild();
        
        ImGui::NextColumn();
        
        // Right Panel - Inspector and others
        ImGui::BeginChild("RightPane", ImVec2(0, 0), true);
        
        ImGui::Text("Inspector");
        ImGui::Separator();
        if (!gameObjects.empty()) {
            GameObject& selected = gameObjects[0]; // Simplified: always edit first
            ImGui::Text("Properties for: %s", selected.name.c_str());
            ImGui::Separator();
            ImGui::DragFloat3("Position", &selected.posX, 0.1f);
            ImGui::DragFloat3("Rotation", &selected.rotX, 1.0f);
            ImGui::DragFloat("Scale", &selected.scale, 0.1f, 0.1f, 10.0f);
            ImGui::Checkbox("Active", &selected.isActive);
        } else {
            ImGui::Text("No object selected");
        }
        
        ImGui::Separator();
        ImGui::Text("Console");
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[Info] NativeEngine Ready.");
        ImGui::TextColored(ImVec4(0, 1, 1, 1), "[System] Built by Dynamic Productions");
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[WARNING] This is a PROTOTYPE version.");
        
        ImGui::EndChild();
        
        ImGui::Columns(1);
        ImGui::End(); // End Main Window
    }

    void Engine::ProcessInput() {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    }

    void Engine::Shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

}
