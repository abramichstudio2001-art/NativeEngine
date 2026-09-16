#pragma once

#include <expected>
#include <string>
#include <memory>
#include <atomic>
#include <functional>

namespace NativeEngine::Core {

enum class ApplicationError {
    WindowInitializationFailed,
    GraphicsInitializationFailed,
    SubsystemFailure,
    RuntimeError
};

struct ApplicationSpecification {
    std::string title = "NativeEngine Editor";
    uint32_t width = 1920;
    uint32_t height = 1080;
    bool vsync = true;
    bool headless = false;
};

class Application {
public:
    explicit Application(const ApplicationSpecification& spec);
    virtual ~Application();

    static Application& GetInstance();

    std::expected<void, ApplicationError> Run();
    void RequestClose();

    bool IsRunning() const { return m_Running; }
    const ApplicationSpecification& GetSpecification() const { return m_Specification; }

protected:
    virtual std::expected<void, ApplicationError> OnInit() { return {}; }
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}
    virtual void OnUIRender() {}
    virtual void OnShutdown() {}

private:
    ApplicationSpecification m_Specification;
    std::atomic<bool> m_Running{ false };
    static Application* s_Instance;
};

} // namespace NativeEngine::Core
