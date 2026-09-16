#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <expected>
#include <filesystem>

namespace NativeEngine::Renderer {

enum class ShaderLanguage {
    HLSL,
    GLSL
};

enum class ShaderStage {
    Vertex,
    Pixel,
    Compute,
    Mesh,
    Task,
    RayGen,
    RayClosestHit,
    RayMiss
};

struct ShaderCompilationResult {
    bool success{ false };
    std::string errorMessage;
    std::vector<uint32_t> spirvCode;
    std::vector<uint8_t> dxilCode;
    std::string reflectionInfo;
};

class ShaderCompiler {
public:
    static ShaderCompiler& GetInstance();

    ShaderCompilationResult CompileShader(const std::string& sourceCode,
                                           ShaderStage stage,
                                           ShaderLanguage lang = ShaderLanguage::HLSL,
                                           const std::string& entryPoint = "main");

    ShaderCompilationResult CompileFile(const std::string& filePath,
                                        ShaderStage stage,
                                        ShaderLanguage lang = ShaderLanguage::HLSL,
                                        const std::string& entryPoint = "main");

    void EnableHotReloading(bool enable) { m_HotReloadingEnabled = enable; }
    bool CheckAndReloadShaders();

private:
    ShaderCompiler() = default;

    bool m_HotReloadingEnabled{ true };
    std::unordered_map<std::string, std::filesystem::file_time_type> m_FileCache;
};

} // namespace NativeEngine::Renderer
