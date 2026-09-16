#pragma once

#include "engine/renderer/RenderContext.hpp"
#include <vector>
#include <string>
#include <variant>
#include <cstring>
#include <algorithm>

namespace NativeEngine::Renderer {

enum class CommandType {
    BeginPass,
    EndPass,
    BindPipeline,
    SetPushConstants,
    Draw,
    DrawIndexed,
    Dispatch,
    DispatchMesh,
    TraceRays,
    ResourceBarrier
};

struct DrawCmd {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

struct DrawIndexedCmd {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};

struct DispatchCmd {
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
};

struct TraceRaysCmd {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

struct PushConstantCmd {
    uint32_t size;
    uint8_t data[128];
};

struct Command {
    CommandType type{ CommandType::BeginPass };
    std::string passName{};
    std::variant<DrawCmd, DrawIndexedCmd, DispatchCmd, TraceRaysCmd, PushConstantCmd> payload{};
};

class CommandList {
public:
    CommandList() = default;

    void BeginPass(const std::string& name) {
        Command cmd{};
        cmd.type = CommandType::BeginPass;
        cmd.passName = name;
        m_Commands.push_back(std::move(cmd));
    }

    void EndPass() {
        Command cmd{};
        cmd.type = CommandType::EndPass;
        m_Commands.push_back(std::move(cmd));
    }

    void BindPipeline(const std::string& pipelineName) {
        Command cmd{};
        cmd.type = CommandType::BindPipeline;
        cmd.passName = pipelineName;
        m_Commands.push_back(std::move(cmd));
    }

    void SetPushConstants(const void* data, uint32_t size) {
        PushConstantCmd pcmd{};
        pcmd.size = std::min(size, 128u);
        std::memcpy(pcmd.data, data, pcmd.size);

        Command cmd{};
        cmd.type = CommandType::SetPushConstants;
        cmd.payload = pcmd;
        m_Commands.push_back(std::move(cmd));
    }

    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) {
        Command cmd{};
        cmd.type = CommandType::Draw;
        cmd.payload = DrawCmd{ vertexCount, instanceCount, firstVertex, firstInstance };
        m_Commands.push_back(std::move(cmd));
    }

    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) {
        Command cmd{};
        cmd.type = CommandType::DrawIndexed;
        cmd.payload = DrawIndexedCmd{ indexCount, instanceCount, firstIndex, vertexOffset, firstInstance };
        m_Commands.push_back(std::move(cmd));
    }

    void Dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) {
        Command cmd{};
        cmd.type = CommandType::Dispatch;
        cmd.payload = DispatchCmd{ x, y, z };
        m_Commands.push_back(std::move(cmd));
    }

    void TraceRays(uint32_t width, uint32_t height, uint32_t depth = 1) {
        Command cmd{};
        cmd.type = CommandType::TraceRays;
        cmd.payload = TraceRaysCmd{ width, height, depth };
        m_Commands.push_back(std::move(cmd));
    }

    void Clear() { m_Commands.clear(); }
    const std::vector<Command>& GetCommands() const { return m_Commands; }

private:
    std::vector<Command> m_Commands;
};

} // namespace NativeEngine::Renderer
