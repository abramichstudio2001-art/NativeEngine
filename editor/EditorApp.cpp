#include "engine/core/Application.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Memory.hpp"
#include "engine/core/JobSystem.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/renderer/RenderContext.hpp"
#include "engine/renderer/RenderGraph.hpp"
#include "engine/renderer/ShaderCompiler.hpp"
#include "engine/ui/UIRenderer.hpp"
#include "engine/ui/WorkspaceManager.hpp"
#include "engine/ui/widgets/ViewportWindow.hpp"
#include "engine/ui/widgets/SceneHierarchyPanel.hpp"
#include "engine/ui/widgets/InspectorPanel.hpp"
#include "engine/ide/CodeEditorPanel.hpp"
#include "engine/ide/ShaderGraphPanel.hpp"
#include "engine/ide/MemoryProfilerPanel.hpp"
#include "engine/ide/FrameDebuggerPanel.hpp"
#include "engine/ide/ScriptCompiler.hpp"
#include <iostream>

using namespace NativeEngine;

class NativeEditorApp : public Core::Application {
public:
    explicit NativeEditorApp(const Core::ApplicationSpecification& spec)
        : Core::Application(spec) {}

protected:
    std::expected<void, Core::ApplicationError> OnInit() override {
        NE_LOG_INFO("Editor", "Initializing NativeEngine AAA Integrated IDE & Subsystems");

        // Initialize Renderer Context
        Renderer::RenderContextConfig config{
            .api = Renderer::GraphicsAPI::Vulkan1_3,
            .enableValidationLayers = true,
            .enableRayTracing = true,
            .enableMeshShaders = true
        };
        Renderer::RenderContext::GetInstance().Initialize(config);

        // Initialize UI Engine
        UI::UIRenderer::GetInstance().Initialize();

        // Register Workspace Windows Layout
        auto& ws = UI::WorkspaceManager::GetInstance();
        ws.RegisterWindow("Viewport", 0, 0, 1000, 600, "Center");
        ws.RegisterWindow("Scene Hierarchy", 1000, 0, 300, 600, "Left");
        ws.RegisterWindow("Inspector", 1300, 0, 300, 600, "Right");
        ws.RegisterWindow("Code Editor", 0, 600, 800, 400, "BottomLeft");
        ws.RegisterWindow("Shader Graph", 800, 600, 800, 400, "BottomRight");
        ws.SaveLayoutToJSON("workspace_layout.json");

        // Populate ECS Scene
        m_SelectedEntity = m_Registry.CreateEntity("Main Camera");
        m_Registry.AddComponent<ECS::CameraComponent>(m_SelectedEntity);

        ECS::Entity light = m_Registry.CreateEntity("Directional Light");
        m_Registry.AddComponent<ECS::LightComponent>(light, ECS::LightComponent{
            .type = ECS::LightType::Directional,
            .intensity = 2.5f
        });

        ECS::Entity cube = m_Registry.CreateEntity("PBR Demo Cube");
        m_Registry.AddComponent<ECS::MeshRendererComponent>(cube);
        m_Registry.AddComponent<ECS::MaterialComponent>(cube);

        // Build Render Graph Pipeline
        m_RenderGraph.AddPass("ShadowPass", Renderer::PassType::Graphics, {}, { "ShadowMap" }, [](Renderer::CommandList& cmd) {
            cmd.BindPipeline("ShadowMapPipeline");
            cmd.Draw(36);
        });

        m_RenderGraph.AddPass("GBufferPass", Renderer::PassType::Graphics, { "ShadowMap" }, { "GBufferA", "GBufferB", "Depth" }, [](Renderer::CommandList& cmd) {
            cmd.BindPipeline("GBufferPipeline");
            cmd.DrawIndexed(1024);
        });

        m_RenderGraph.AddPass("RayTracingPass", Renderer::PassType::RayTracing, { "GBufferA", "GBufferB" }, { "RayTracingOutput" }, [](Renderer::CommandList& cmd) {
            cmd.BindPipeline("PathTracerPipeline");
            cmd.TraceRays(1920, 1080);
        });

        m_RenderGraph.AddPass("CompositeUIPass", Renderer::PassType::Ui, { "RayTracingOutput" }, { "FinalColor" }, [](Renderer::CommandList& cmd) {
            cmd.BindPipeline("UIPipeline");
            cmd.Draw(6);
        });

        m_RenderGraph.Compile();

        // Compile Initial Script & Shader Graph
        IDE::ScriptCompiler::GetInstance().CompileCSharpScript("scripts/PlayerController.cs");
        IDE::ScriptCompiler::GetInstance().CompileCppScript("scripts/NativeGameModule.cpp");

        NE_LOG_INFO("Editor", "NativeEngine Subsystems and Editor Workspace initialized successfully.");
        return {};
    }

    void OnUpdate(float deltaTime) override {
        // Hot-reload checks
        Renderer::ShaderCompiler::GetInstance().CheckAndReloadShaders();
    }

    void OnRender() override {
        Renderer::RenderContext::GetInstance().BeginFrame();
        m_RenderGraph.Execute();
        Renderer::RenderContext::GetInstance().EndFrame();
    }

    void OnUIRender() override {
        UI::UIRenderer::GetInstance().BeginFrame();

        // Render IDE Panels and UI Layout
        m_ViewportWindow.Render(0, 0, 1000, 600, 101);
        m_HierarchyPanel.Render(1000, 0, 300, 600, m_Registry, m_SelectedEntity);
        m_InspectorPanel.Render(1300, 0, 300, 600, m_Registry, m_SelectedEntity);

        m_CodeEditorPanel.Render(0, 600, 500, 400);
        m_ShaderGraphPanel.Render(500, 600, 500, 400);
        m_MemoryProfilerPanel.Render(1000, 600, 300, 400);
        m_FrameDebuggerPanel.Render(1300, 600, 300, 400, m_RenderGraph);

        UI::UIRenderer::GetInstance().EndFrame();

        // In headless mode for unit testing verification, stop application after 5 frames
        if (GetSpecification().headless && Renderer::RenderContext::GetInstance().GetFrameCount() >= 5) {
            RequestClose();
        }
    }

    void OnShutdown() override {
        UI::UIRenderer::GetInstance().Shutdown();
        Renderer::RenderContext::GetInstance().Shutdown();
        NE_LOG_INFO("Editor", "Editor shutdown finished.");
    }

private:
    ECS::Registry m_Registry;
    ECS::Entity m_SelectedEntity{ ECS::NullEntity };
    Renderer::RenderGraph m_RenderGraph;

    UI::Widgets::ViewportWindow m_ViewportWindow;
    UI::Widgets::SceneHierarchyPanel m_HierarchyPanel;
    UI::Widgets::InspectorPanel m_InspectorPanel;

    IDE::CodeEditorPanel m_CodeEditorPanel;
    IDE::ShaderGraphPanel m_ShaderGraphPanel;
    IDE::MemoryProfilerPanel m_MemoryProfilerPanel;
    IDE::FrameDebuggerPanel m_FrameDebuggerPanel;
};

int main(int argc, char** argv) {
    Core::ApplicationSpecification spec{
        .title = "NativeEngine IDE Editor - C++23 Modern Subsystem Architecture",
        .width = 1600,
        .height = 1000,
        .vsync = true,
        .headless = true // Run 5 frames headless in environment for verification
    };

    NativeEditorApp app(spec);
    auto res = app.Run();
    if (!res) {
        std::cerr << "Application failed to run!\n";
        return -1;
    }

    std::cout << "NativeEngine process finished cleanly with code 0.\n";
    return 0;
}
