#include "engine/ui/UIRenderer.hpp"
#include "engine/core/Log.hpp"

namespace NativeEngine::UI {

UIRenderer& UIRenderer::GetInstance() {
    static UIRenderer instance;
    return instance;
}

void UIRenderer::Initialize() {
    m_Primitives.reserve(10000);
    NE_LOG_INFO("UI", "UIRenderer initialized with GPU accelerated vector primitive batcher");
}

void UIRenderer::Shutdown() {
    m_Primitives.clear();
    NE_LOG_INFO("UI", "UIRenderer shutdown complete");
}

void UIRenderer::BeginFrame() {
    m_Primitives.clear();
}

void UIRenderer::EndFrame() {
    // Submit draw commands to renderer batch
}

void UIRenderer::DrawRect(float x, float y, float w, float h, StyleColor color) {
    m_Primitives.push_back(DrawPrimitive{ PrimitiveType::Rect, x, y, w, h, color, "", 0 });
}

void UIRenderer::DrawRectFilled(float x, float y, float w, float h, StyleColor color) {
    m_Primitives.push_back(DrawPrimitive{ PrimitiveType::RectFilled, x, y, w, h, color, "", 0 });
}

void UIRenderer::DrawText(float x, float y, const std::string& text, StyleColor color) {
    m_Primitives.push_back(DrawPrimitive{ PrimitiveType::Text, x, y, 0, 0, color, text, 0 });
}

void UIRenderer::DrawImage(float x, float y, float w, float h, uint64_t textureId) {
    m_Primitives.push_back(DrawPrimitive{ PrimitiveType::Image, x, y, w, h, StyleColor{}, "", textureId });
}

} // namespace NativeEngine::UI
