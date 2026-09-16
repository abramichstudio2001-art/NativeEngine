#pragma once

#include <cstdint>

namespace NativeEngine::UI {

struct StyleColor {
    float r{ 0.0f }, g{ 0.0f }, b{ 0.0f }, a{ 1.0f };
};

struct StyleTheme {
    StyleColor windowBackground{ 0.11f, 0.11f, 0.12f, 1.0f };
    StyleColor headerBackground{ 0.16f, 0.16f, 0.18f, 1.0f };
    StyleColor buttonBackground{ 0.22f, 0.22f, 0.25f, 1.0f };
    StyleColor buttonHovered{ 0.28f, 0.28f, 0.32f, 1.0f };
    StyleColor buttonActive{ 0.35f, 0.35f, 0.40f, 1.0f };
    StyleColor accentPrimary{ 0.00f, 0.48f, 0.80f, 1.0f }; // AAA Engine Blue
    StyleColor textColor{ 0.90f, 0.90f, 0.92f, 1.0f };
    StyleColor textDisabled{ 0.50f, 0.50f, 0.52f, 1.0f };
    StyleColor border{ 0.25f, 0.25f, 0.28f, 1.0f };

    float windowRounding{ 6.0f };
    float frameRounding{ 4.0f };
    float popupRounding{ 4.0f };
    float scrollbarRounding{ 4.0f };
    float grabRounding{ 3.0f };
    float windowBorderSize{ 1.0f };
};

inline StyleTheme GetDarkAAATheme() {
    return StyleTheme{};
}

} // namespace NativeEngine::UI
