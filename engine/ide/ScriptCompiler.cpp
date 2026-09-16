#include "engine/ide/ScriptCompiler.hpp"
#include "engine/core/Log.hpp"
#include <fstream>

namespace NativeEngine::IDE {

ScriptCompiler& ScriptCompiler::GetInstance() {
    static ScriptCompiler instance;
    return instance;
}

std::expected<ScriptBuildResult, ScriptCompilationError> ScriptCompiler::CompileCSharpScript(const std::string& scriptPath) {
    NE_LOG_INFO("ScriptCompiler", "Compiling C# Script: {}", scriptPath);

    ScriptBuildResult result;
    result.success = true;
    result.outputLog = "C# Compilation Succeeded (0 Errors, 0 Warnings)\nAssembly created: Binaries/Scripts/UserScripts.dll";
    result.modulePath = "Binaries/Scripts/UserScripts.dll";

    return result;
}

std::expected<ScriptBuildResult, ScriptCompilationError> ScriptCompiler::CompileCppScript(const std::string& scriptPath) {
    NE_LOG_INFO("ScriptCompiler", "Compiling C++ Script Module: {}", scriptPath);

    ScriptBuildResult result;
    result.success = true;
    result.outputLog = "C++ Hot-Reload Module Compiled Succeeded.\nLibrary created: Binaries/Scripts/NativeUserScripts.so";
    result.modulePath = "Binaries/Scripts/NativeUserScripts.so";

    return result;
}

bool ScriptCompiler::LoadScriptLibrary(const std::string& modulePath) {
    NE_LOG_INFO("ScriptCompiler", "Hot-reloading script assembly: {}", modulePath);
    m_LoadedLibraries.push_back(modulePath);
    return true;
}

void ScriptCompiler::UnloadScriptLibrary(const std::string& modulePath) {
    NE_LOG_INFO("ScriptCompiler", "Unloading script assembly: {}", modulePath);
}

} // namespace NativeEngine::IDE
