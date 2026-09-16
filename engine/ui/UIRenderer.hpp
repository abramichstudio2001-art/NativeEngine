#pragma once

#include "engine/ui/StyleTheme.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace NativeEngine::UI {

enum class PrimitiveType {
    Rect,
    RectFilled,
    Text,
    Line,
    Image
};

struct DrawPrimitive {
    PrimitiveType type;
    float posX, posY, width, height;
    StyleColor color;
    std::string text;
    uint64_t textureId{ 0 };
};

class UIRenderer {
public:
    static UIRenderer& GetInstance();

    void Initialize();
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void DrawRect(float x, float y, float w, float h, StyleColor color);
    void DrawRectFilled(float x, float y, float w, float h, StyleColor color);
    void DrawText(float x, float y, const std::string& text, StyleColor color);
    void DrawImage(float x, float y, float w, float h, uint64_t textureId);

    void SetTheme(const StyleTheme& theme) { m_Theme = theme; }
    const StyleTheme& GetTheme() const { return m_Theme; }
    const std::vector<DrawPrimitive>& GetDrawPrimitives() const { return m_Primitives; }

private:
    UIRenderer() = default;

    StyleTheme m_Theme{ GetDarkAAATheme() };
    std::vector<DrawPrimitive> m_Primitives;
};

} // namespace NativeEngine::UI
