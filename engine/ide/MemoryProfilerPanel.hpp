#pragma once

#include "engine/ui/UIRenderer.hpp"
#include "engine/core/Memory.hpp"

namespace NativeEngine::IDE {

class MemoryProfilerPanel {
public:
    MemoryProfilerPanel() = default;

    void Render(float x, float y, float width, float height);
};

} // namespace NativeEngine::IDE
