#pragma once

#include "engine/ui/UIRenderer.hpp"
#include <string>

namespace NativeEngine::UI::Widgets {

class ViewportWindow {
public:
    ViewportWindow() = default;

    void Render(float x, float y, float width, float height, uint64_t renderTargetTextureId) {
        m_PosX = x;
        m_PosY = y;
        m_Width = width;
        m_Height = height;

        auto& ui = UIRenderer::GetInstance();
        // Window Title Bar
        ui.DrawRectFilled(x, y, width, 24.0f, ui.GetTheme().headerBackground);
        ui.DrawText(x + 10.0f, y + 4.0f, "Viewport - Realtime Ray Tracing & Deferred Debugger", ui.GetTheme().textColor);

        // Render Target Canvas Viewport area
        if (renderTargetTextureId != 0) {
            ui.DrawImage(x, y + 24.0f, width, height - 24.0f, renderTargetTextureId);
        } else {
            ui.DrawRectFilled(x, y + 24.0f, width, height - 24.0f, StyleColor{ 0.05f, 0.05f, 0.07f, 1.0f });
            ui.DrawText(x + width * 0.35f, y + height * 0.45f, "[ 3D Viewport Ray Tracing Offscreen Target ]", ui.GetTheme().accentPrimary);
        }

        // Overlay FPS & Camera Info
        ui.DrawText(x + 15.0f, y + 35.0f, "FPS: 144.0 (0.69 ms)", StyleColor{ 0.2f, 1.0f, 0.4f, 1.0f });
        ui.DrawText(x + 15.0f, y + 55.0f, "Cam: [X: 0.0, Y: 2.5, Z: -10.0]", StyleColor{ 0.8f, 0.8f, 0.8f, 1.0f });
    }

private:
    float m_PosX{ 0 }, m_PosY{ 0 }, m_Width{ 800 }, m_Height{ 600 };
};

} // namespace NativeEngine::UI::Widgets
