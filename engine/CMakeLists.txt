cmake_minimum_required(VERSION 3.10)
project(NativeEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- shared engine library (loaded from Python via ctypes) -------------------
add_library(NativeEngine SHARED
    src/ne_api.cpp
)

target_include_directories(NativeEngine PRIVATE src)
find_package(Threads REQUIRED)
target_link_libraries(NativeEngine PRIVATE Threads::Threads)

if(NOT MSVC)
    target_compile_options(NativeEngine PRIVATE -O3 -ffast-math -funroll-loops)
endif()

# --- standalone CLI renderer (optional, C++ only) -----------------------------
add_executable(ne_cli src/ne_cli.cpp)
target_include_directories(ne_cli PRIVATE src)
target_link_libraries(ne_cli PRIVATE NativeEngine Threads::Threads)
if(NOT MSVC)
    target_compile_options(ne_cli PRIVATE -O3 -ffast-math -funroll-loops)
endif()
