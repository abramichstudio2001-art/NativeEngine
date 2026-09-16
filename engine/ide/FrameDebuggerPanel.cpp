#include "engine/ide/FrameDebuggerPanel.hpp"
#include <format>

namespace NativeEngine::IDE {

void FrameDebuggerPanel::Render(float x, float y, float width, float height, const Renderer::RenderGraph& renderGraph) {
    auto& ui = UI::UIRenderer::GetInstance();

    ui.DrawRectFilled(x, y, width, 24.0f, ui.GetTheme().headerBackground);
    ui.DrawText(x + 10.0f, y + 4.0f, "Frame Debugger & Render Graph Execution Viewer", ui.GetTheme().textColor);

    ui.DrawRectFilled(x, y + 24.0f, width, height - 24.0f, ui.GetTheme().windowBackground);

    float passY = y + 35.0f;
    const auto& passes = renderGraph.GetPasses();

    for (const auto& pass : passes) {
        ui.DrawRectFilled(x + 10.0f, passY, width - 20.0f, 26.0f, ui.GetTheme().headerBackground);
        ui.DrawText(x + 20.0f, passY + 4.0f, std::format("Pass: {} [{}]", pass.name, pass.type == Renderer::PassType::Graphics ? "Graphics" : "Compute/RT"), ui.GetTheme().accentPrimary);
        passY += 30.0f;

        if (passY + 30.0f > y + height) break;
    }
}

} // namespace NativeEngine::IDE
