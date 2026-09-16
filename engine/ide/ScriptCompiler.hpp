#pragma once

#include <string>
#include <vector>
#include <expected>

namespace NativeEngine::IDE {

enum class ScriptCompilationError {
    FileNotFound,
    SyntaxError,
    LinkError,
    BuildFailed
};

struct ScriptBuildResult {
    bool success{ false };
    std::string outputLog;
    std::string modulePath;
};

class ScriptCompiler {
public:
    static ScriptCompiler& GetInstance();

    std::expected<ScriptBuildResult, ScriptCompilationError> CompileCSharpScript(const std::string& scriptPath);
    std::expected<ScriptBuildResult, ScriptCompilationError> CompileCppScript(const std::string& scriptPath);

    bool LoadScriptLibrary(const std::string& modulePath);
    void UnloadScriptLibrary(const std::string& modulePath);

private:
    ScriptCompiler() = default;

    std::vector<std::string> m_LoadedLibraries;
};

} // namespace NativeEngine::IDE
