#pragma once

#include "engine/ui/UIRenderer.hpp"
#include <string>
#include <vector>

namespace NativeEngine::IDE {

struct Breakpoint {
    uint32_t line;
    bool enabled{ true };
};

class CodeEditorPanel {
public:
    CodeEditorPanel();

    void OpenFile(const std::string& filePath);
    void SaveFile(const std::string& filePath);

    void Render(float x, float y, float width, float height);
    void ToggleBreakpoint(uint32_t line);

    const std::string& GetCurrentFile() const { return m_CurrentFilePath; }

private:
    std::string m_CurrentFilePath{ "scripts/PlayerController.cs" };
    std::vector<std::string> m_CodeLines;
    std::vector<Breakpoint> m_Breakpoints;
    uint32_t m_CursorLine{ 1 };
    uint32_t m_CursorColumn{ 1 };
};

} // namespace NativeEngine::IDE
