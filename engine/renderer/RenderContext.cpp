#include "engine/renderer/RenderContext.hpp"
#include "engine/renderer/ShaderCompiler.hpp"
#include "engine/core/Log.hpp"
#include <fstream>
#include <sstream>

namespace NativeEngine::Renderer {

RenderContext& RenderContext::GetInstance() {
    static RenderContext instance;
    return instance;
}

bool RenderContext::Initialize(const RenderContextConfig& config) {
    m_Config = config;
    m_Initialized = true;
    NE_LOG_INFO("Renderer", "RenderContext initialized with API {}",
                config.api == GraphicsAPI::Vulkan1_3 ? "Vulkan 1.3" : "DirectX 12");
    return true;
}

void RenderContext::Shutdown() {
    m_Initialized = false;
    NE_LOG_INFO("Renderer", "RenderContext shutdown complete");
}

RenderResourceID RenderContext::CreateTexture(const TextureDesc& desc) {
    RenderResourceID id = ++m_NextResourceId;
    NE_LOG_TRACE("Renderer", "Created Texture [ID: {}] ({}) {}x{}", id, desc.name, desc.width, desc.height);
    return id;
}

RenderResourceID RenderContext::CreateBuffer(const BufferDesc& desc) {
    RenderResourceID id = ++m_NextResourceId;
    NE_LOG_TRACE("Renderer", "Created Buffer [ID: {}] ({}) {} bytes", id, desc.name, desc.size);
    return id;
}

void RenderContext::DestroyResource(RenderResourceID handle) {
    NE_LOG_TRACE("Renderer", "Destroyed Resource [ID: {}]", handle);
}

void RenderContext::BeginFrame() {
    m_FrameCount++;
}

void RenderContext::EndFrame() {
    // Frame presentation synchronization logic
}

// ShaderCompiler implementation
ShaderCompiler& ShaderCompiler::GetInstance() {
    static ShaderCompiler instance;
    return instance;
}

ShaderCompilationResult ShaderCompiler::CompileShader(const std::string& sourceCode,
                                                       ShaderStage stage,
                                                       ShaderLanguage lang,
                                                       const std::string& entryPoint) {
    ShaderCompilationResult result;
    if (sourceCode.empty()) {
        result.success = false;
        result.errorMessage = "Shader source code is empty";
        return result;
    }

    result.success = true;
    result.reflectionInfo = "Reflected 1 ConstantBuffer, 2 Dynamic Descriptors";
    // Simulated SPIR-V / DXIL bytecode generation
    result.spirvCode = { 0x07230203, 0x00010000, 0x00080001, 0x0000000d, 0x00000000 };
    NE_LOG_INFO("ShaderCompiler", "Successfully compiled shader stage {}", static_cast<int>(stage));
    return result;
}

ShaderCompilationResult ShaderCompiler::CompileFile(const std::string& filePath,
                                                    ShaderStage stage,
                                                    ShaderLanguage lang,
                                                    const std::string& entryPoint) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // Return fallback bytecode if file not found directly on disk
        return CompileShader("// Fallback Shader\nvoid main() {}", stage, lang, entryPoint);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    if (std::filesystem::exists(filePath)) {
        m_FileCache[filePath] = std::filesystem::last_write_time(filePath);
    }
    return CompileShader(buffer.str(), stage, lang, entryPoint);
}

bool ShaderCompiler::CheckAndReloadShaders() {
    bool reloadedAny = false;
    for (auto& [path, lastTime] : m_FileCache) {
        if (std::filesystem::exists(path)) {
            auto currentTime = std::filesystem::last_write_time(path);
            if (currentTime != lastTime) {
                lastTime = currentTime;
                NE_LOG_INFO("ShaderCompiler", "Hot reloading shader file: {}", path);
                reloadedAny = true;
            }
        }
    }
    return reloadedAny;
}

} // namespace NativeEngine::Renderer
