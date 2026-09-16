#include "engine/ide/CodeEditorPanel.hpp"
#include "engine/core/Log.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <format>

namespace NativeEngine::IDE {

CodeEditorPanel::CodeEditorPanel() {
    m_CodeLines = {
        "using NativeEngine.Core;",
        "using NativeEngine.ECS;",
        "",
        "public class PlayerController : NativeScript {",
        "    public float moveSpeed = 5.0f;",
        "",
        "    public override void OnUpdate(float deltaTime) {",
        "        Vector3 pos = GetTransform().Position;",
        "        if (Input.IsKeyPressed(KeyCode.W)) pos.z += moveSpeed * deltaTime;",
        "        GetTransform().Position = pos;",
        "    }",
        "}"
    };
    m_Breakpoints.push_back(Breakpoint{ .line = 9, .enabled = true });
}

void CodeEditorPanel::OpenFile(const std::string& filePath) {
    m_CurrentFilePath = filePath;
    std::ifstream file(filePath);
    if (file.is_open()) {
        m_CodeLines.clear();
        std::string line;
        while (std::getline(file, line)) {
            m_CodeLines.push_back(line);
        }
    }
}

void CodeEditorPanel::SaveFile(const std::string& filePath) {
    std::ofstream file(filePath);
    if (file.is_open()) {
        for (const auto& line : m_CodeLines) {
            file << line << "\n";
        }
    }
    NE_LOG_INFO("IDE", "Saved source file: {}", filePath);
}

void CodeEditorPanel::ToggleBreakpoint(uint32_t line) {
    auto it = std::find_if(m_Breakpoints.begin(), m_Breakpoints.end(), [line](const Breakpoint& bp) {
        return bp.line == line;
    });

    if (it != m_Breakpoints.end()) {
        m_Breakpoints.erase(it);
    } else {
        m_Breakpoints.push_back(Breakpoint{ .line = line, .enabled = true });
    }
}

void CodeEditorPanel::Render(float x, float y, float width, float height) {
    auto& ui = UI::UIRenderer::GetInstance();

    // Header
    ui.DrawRectFilled(x, y, width, 24.0f, ui.GetTheme().headerBackground);
    ui.DrawText(x + 10.0f, y + 4.0f, std::format("Code Editor - {}", m_CurrentFilePath), ui.GetTheme().textColor);

    // Code area
    ui.DrawRectFilled(x, y + 24.0f, width, height - 24.0f, UI::StyleColor{ 0.08f, 0.08f, 0.09f, 1.0f });

    float lineY = y + 30.0f;
    for (size_t i = 0; i < m_CodeLines.size(); ++i) {
        uint32_t lineNum = static_cast<uint32_t>(i + 1);

        bool hasBp = std::any_of(m_Breakpoints.begin(), m_Breakpoints.end(), [lineNum](const Breakpoint& bp) {
            return bp.line == lineNum;
        });

        if (hasBp) {
            ui.DrawRectFilled(x + 5.0f, lineY + 3.0f, 10.0f, 10.0f, UI::StyleColor{ 0.9f, 0.2f, 0.2f, 1.0f });
        }

        // Line number
        ui.DrawText(x + 20.0f, lineY, std::format("{:>3}", lineNum), ui.GetTheme().textDisabled);

        // Code content
        ui.DrawText(x + 60.0f, lineY, m_CodeLines[i], ui.GetTheme().textColor);

        lineY += 20.0f;
        if (lineY + 20.0f > y + height) break;
    }
}

} // namespace NativeEngine::IDE
