#include "engine/core/Log.hpp"
#include "engine/core/Memory.hpp"
#include "engine/core/JobSystem.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/renderer/RenderContext.hpp"
#include "engine/renderer/RenderGraph.hpp"
#include "engine/renderer/ShaderCompiler.hpp"
#include "engine/ui/WorkspaceManager.hpp"
#include "engine/ide/ShaderGraphPanel.hpp"
#include "engine/ide/ScriptCompiler.hpp"
#include <iostream>
#include <cassert>

using namespace NativeEngine;

void TestCoreAndMemory() {
    Core::Logger::GetInstance().Initialize();
    NE_LOG_INFO("Test", "Running Core Memory Test");

    void* ptr = Core::DynamicAllocate(1024, 16, Core::AllocationCategory::EngineCore, "TestBuffer");
    assert(ptr != nullptr);

    auto metrics = Core::MemoryTracker::GetInstance().GetMetrics();
    assert(metrics.totalAllocatedBytes >= 1024);

    Core::DynamicFree(ptr);
    NE_LOG_INFO("Test", "Memory Test Passed");
}

void TestJobSystem() {
    NE_LOG_INFO("Test", "Running JobSystem Test");
    Core::JobSystem::GetInstance().Initialize(4);

    Core::JobCounter counter;
    std::atomic<int> sum{ 0 };

    Core::JobSystem::GetInstance().DispatchBatch(100, 10, [&sum](uint32_t idx) {
        sum.fetch_add(1, std::memory_order_relaxed);
    }, &counter);

    counter.Wait();
    assert(sum.load() == 100);

    Core::JobSystem::GetInstance().Shutdown();
    NE_LOG_INFO("Test", "JobSystem Test Passed");
}

void TestECS() {
    NE_LOG_INFO("Test", "Running ECS Test");
    ECS::Registry registry;
    ECS::Entity e = registry.CreateEntity("TestEntity");

    registry.AddComponent<ECS::MeshRendererComponent>(e, ECS::MeshRendererComponent{ .meshName = "TestMesh" });
    assert(registry.HasComponent<ECS::MeshRendererComponent>(e));
    assert(registry.GetComponent<ECS::MeshRendererComponent>(e).meshName == "TestMesh");

    auto view = registry.View<ECS::TagComponent, ECS::MeshRendererComponent>();
    assert(view.size() == 1);

    NE_LOG_INFO("Test", "ECS Test Passed");
}

void TestShaderGraphAndScriptCompiler() {
    NE_LOG_INFO("Test", "Running ShaderGraph & ScriptCompiler Test");
    IDE::ShaderGraphPanel shaderGraph;
    std::string hlsl = shaderGraph.GenerateHLSLCode();
    assert(!hlsl.empty());

    auto csRes = IDE::ScriptCompiler::GetInstance().CompileCSharpScript("Player.cs");
    assert(csRes.has_value() && csRes->success);

    NE_LOG_INFO("Test", "ShaderGraph & ScriptCompiler Test Passed");
}

int main() {
    std::cout << "Starting NativeEngine Subsystem Unit & Integration Tests...\n";
    TestCoreAndMemory();
    TestJobSystem();
    TestECS();
    TestShaderGraphAndScriptCompiler();
    std::cout << "ALL NATIVEENGINE TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
