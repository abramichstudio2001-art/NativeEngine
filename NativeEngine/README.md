# NativeEngine
**By Dynamic Productions**

A modern, user-friendly 3D Game Engine with a built-in IDE, written in C++.

## Features
- **Built-in IDE**: Dockable interface with Hierarchy, Inspector, Scene View, and Console.
- **Real-time Rendering**: OpenGL 3.3 Core Profile based rendering.
- **User Friendly**: Intuitive ImGui-based interface for manipulating 3D objects.
- **Cross-Platform**: Supports Windows (.exe), macOS, and Linux.

## Prerequisites
- CMake 3.15+
- C++17 Compiler (MSVC, GCC, or Clang)
- Internet connection (for fetching dependencies via CMake)

## How to Build

### Windows (Visual Studio)
1. Open a terminal in the `NativeEngine` folder.
2. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```
3. Configure with CMake:
   ```bash
   cmake .. -G "Visual Studio 16 2019"
   ```
   *(Or use "Visual Studio 17 2022")*
4. Build the solution:
   ```bash
   cmake --build . --config Release
   ```
5. Your `.exe` will be located in `build/Release/NativeEngine.exe`.

### Linux / macOS
1. Open a terminal in the `NativeEngine` folder.
2. Create a build directory:
   ```bash
   mkdir build && cd build
   ```
3. Configure and Build:
   ```bash
   cmake ..
   make -j$(nproc)
   ```
4. Run the engine:
   ```bash
   ./NativeEngine
   ```

## Controls
- **ESC**: Close the application.
- **Mouse/Keyboard**: Interact with the IDE windows (dock, resize, click).
- **Inspector**: Modify Position, Rotation, and Scale of objects in real-time.

## Project Structure
- `src/main.cpp`: Entry point.
- `src/Engine.cpp`: Core engine logic, rendering loop, and UI implementation.
- `src/Engine.h`: Header definitions.
- `CMakeLists.txt`: Build configuration.

## License
Proprietary - Dynamic Productions
