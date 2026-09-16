cmake_minimum_required(VERSION 3.28)
project(NativeEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(Threads REQUIRED)
find_package(Vulkan QUIET)
find_package(glfw3 QUIET)

# Engine Core Library
add_library(EngineCore STATIC
    engine/core/Log.cpp
    engine/core/Memory.cpp
    engine/core/JobSystem.cpp
    engine/core/Application.cpp
    engine/renderer/RenderContext.cpp
    engine/ui/UIRenderer.cpp
    engine/ide/CodeEditorPanel.cpp
    engine/ide/ShaderGraphPanel.cpp
    engine/ide/MemoryProfilerPanel.cpp
    engine/ide/FrameDebuggerPanel.cpp
    engine/ide/ScriptCompiler.cpp
)

target_include_directories(EngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(EngineCore PUBLIC
    Threads::Threads
)

if(Vulkan_FOUND)
    target_link_libraries(EngineCore PUBLIC Vulkan::Vulkan)
endif()

# NativeEngine Editor Executable
add_executable(NativeEngineEditor
    editor/EditorApp.cpp
)

target_link_libraries(NativeEngineEditor PRIVATE
    EngineCore
)

# NativeEngine Unit & Integration Test Executable
add_executable(NativeEngineTests
    tests/EngineTests.cpp
)

target_link_libraries(NativeEngineTests PRIVATE
    EngineCore
)
