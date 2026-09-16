#pragma once

#include "engine/renderer/RenderContext.hpp"
#include "engine/renderer/CommandList.hpp"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

namespace NativeEngine::Renderer {

enum class PassType {
    Graphics,
    Compute,
    RayTracing,
    Ui
};

struct RenderPassResource {
    std::string name;
    TextureDesc desc;
    bool isOutput{ false };
};

class RenderGraphPass {
public:
    std::string name;
    PassType type{ PassType::Graphics };
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::function<void(CommandList& cmdList)> executeCallback;
    uint64_t executionTimeUs{ 0 };
};

class RenderGraph {
public:
    RenderGraph() = default;

    void AddPass(const std::string& name, PassType type,
                 const std::vector<std::string>& inputs,
                 const std::vector<std::string>& outputs,
                 std::function<void(CommandList& cmdList)> callback) {
        RenderGraphPass pass;
        pass.name = name;
        pass.type = type;
        pass.inputs = inputs;
        pass.outputs = outputs;
        pass.executeCallback = callback;
        m_Passes.push_back(std::move(pass));
    }

    void DeclareResource(const std::string& name, const TextureDesc& desc) {
        m_DeclaredResources[name] = desc;
    }

    void Compile() {
        // Build execution order (topological sort / dependency resolution)
        m_CompiledPasses = m_Passes;
    }

    void Execute() {
        for (auto& pass : m_CompiledPasses) {
            CommandList cmdList;
            cmdList.BeginPass(pass.name);
            if (pass.executeCallback) {
                pass.executeCallback(cmdList);
            }
            cmdList.EndPass();
            m_PassCommands[pass.name] = cmdList.GetCommands();
        }
    }

    const std::vector<RenderGraphPass>& GetPasses() const { return m_CompiledPasses; }
    const std::unordered_map<std::string, std::vector<Command>>& GetPassCommands() const { return m_PassCommands; }

private:
    std::vector<RenderGraphPass> m_Passes;
    std::vector<RenderGraphPass> m_CompiledPasses;
    std::unordered_map<std::string, TextureDesc> m_DeclaredResources;
    std::unordered_map<std::string, std::vector<Command>> m_PassCommands;
};

} // namespace NativeEngine::Renderer
