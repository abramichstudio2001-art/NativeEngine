#include "engine/core/Application.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Memory.hpp"
#include "engine/core/JobSystem.hpp"
#include <chrono>
#include <thread>

namespace NativeEngine::Core {

Application* Application::s_Instance = nullptr;

Application::Application(const ApplicationSpecification& spec)
    : m_Specification(spec) {
    s_Instance = this;
}

Application::~Application() {
    s_Instance = nullptr;
}

Application& Application::GetInstance() {
    return *s_Instance;
}

std::expected<void, ApplicationError> Application::Run() {
    Logger::GetInstance().Initialize();
    NE_LOG_INFO("Core", "Starting Application: {}", m_Specification.title);

    JobSystem::GetInstance().Initialize();

    auto initResult = OnInit();
    if (!initResult) {
        NE_LOG_CRITICAL("Core", "Application initialization failed!");
        return initResult;
    }

    m_Running = true;

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_Running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Sequence Logic update, Render pass, and UI pass deterministically
        OnUpdate(deltaTime);
        OnRender();
        OnUIRender();

        if (m_Specification.headless) {
            // In headless mode for automated verification, yield briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    OnShutdown();
    JobSystem::GetInstance().Shutdown();
    Logger::GetInstance().Shutdown();

    return {};
}

void Application::RequestClose() {
    m_Running = false;
}

} // namespace NativeEngine::Core
