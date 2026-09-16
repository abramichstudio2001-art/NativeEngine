#pragma once

#include <string>
#include <vector>
#include <memory>
#include <expected>
#include <cstdint>

namespace NativeEngine::Renderer {

enum class GraphicsAPI {
    Vulkan1_3,
    DirectX12
};

struct RenderContextConfig {
    GraphicsAPI api = GraphicsAPI::Vulkan1_3;
    bool enableValidationLayers = true;
    bool enableRayTracing = true;
    bool enableMeshShaders = true;
    uint32_t maxBindlessDescriptors = 10000;
};

struct TextureDesc {
    uint32_t width{ 1 };
    uint32_t height{ 1 };
    uint32_t format{ 0 }; // RGBA8 / RGBA16F / Depth
    std::string name{ "Texture" };
};

struct BufferDesc {
    size_t size{ 0 };
    uint32_t usageFlags{ 0 };
    std::string name{ "Buffer" };
};

using RenderResourceID = uint64_t;

class RenderContext {
public:
    static RenderContext& GetInstance();

    bool Initialize(const RenderContextConfig& config);
    void Shutdown();

    RenderResourceID CreateTexture(const TextureDesc& desc);
    RenderResourceID CreateBuffer(const BufferDesc& desc);
    void DestroyResource(RenderResourceID handle);

    void BeginFrame();
    void EndFrame();

    GraphicsAPI GetAPI() const { return m_Config.api; }
    bool IsRayTracingSupported() const { return m_Config.enableRayTracing; }
    bool IsMeshShaderSupported() const { return m_Config.enableMeshShaders; }
    uint64_t GetFrameCount() const { return m_FrameCount; }

private:
    RenderContext() = default;

    RenderContextConfig m_Config;
    uint64_t m_FrameCount{ 0 };
    RenderResourceID m_NextResourceId{ 100 };
    bool m_Initialized{ false };
};

} // namespace NativeEngine::Renderer
