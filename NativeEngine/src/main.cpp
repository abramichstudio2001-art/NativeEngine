#include "Engine.h"
#include <iostream>

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "NativeEngine by Dynamic Productions" << std::endl;
    std::cout << "STATUS: PROTOTYPE" << std::endl;
    std::cout << "========================================" << std::endl;

    NativeEngine::Engine engine;

    if (!engine.Initialize()) {
        std::cerr << "Failed to initialize engine." << std::endl;
        return -1;
    }

    std::cout << "Entering main loop..." << std::endl;
    engine.Run();

    std::cout << "Shutting down..." << std::endl;
    engine.Shutdown();

    return 0;
}
