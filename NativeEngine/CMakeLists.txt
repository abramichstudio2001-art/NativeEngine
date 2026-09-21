cmake_minimum_required(VERSION 3.15)
project(NativeEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Dependencies automatically (GLFW, GLEW, ImGui, GLM)
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# GLEW (Use system package)
find_package(GLEW REQUIRED)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# Dear ImGui 
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    file(GLOB IMGUI_SOURCES 
        "${imgui_SOURCE_DIR}/*.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    )
    add_library(imgui_lib STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui_lib PUBLIC 
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${glfw_SOURCE_DIR}/include
    )
    # Ensure imgui builds after glfw
    add_dependencies(imgui_lib glfw)
endif()

# Define executable
add_executable(NativeEngine 
    src/main.cpp
    src/Engine.cpp
)

# Link Libraries
target_link_libraries(NativeEngine PRIVATE 
    glfw
    GLEW::GLEW
    imgui_lib
    glm::glm
)

# Platform Specific Links
if(WIN32)
    target_link_libraries(NativeEngine PRIVATE opengl32 glu32)
    set_target_properties(NativeEngine PROPERTIES WIN32_EXECUTABLE TRUE)
    # Add Windows Resource file for icon/version if needed
elseif(APPLE)
    target_link_libraries(NativeEngine PRIVATE "-framework OpenGL")
else()
    target_link_libraries(NativeEngine PRIVATE GL)
endif()

target_include_directories(NativeEngine PRIVATE 
    ${glfw_SOURCE_DIR}/include
)

message(STATUS "NativeEngine by Dynamic Productions configured successfully.")
