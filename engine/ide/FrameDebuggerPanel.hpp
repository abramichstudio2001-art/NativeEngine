#pragma once

#include "engine/ui/UIRenderer.hpp"
#include "engine/renderer/RenderGraph.hpp"

namespace NativeEngine::IDE {

class FrameDebuggerPanel {
public:
    FrameDebuggerPanel() = default;

    void Render(float x, float y, float width, float height, const Renderer::RenderGraph& renderGraph);
};

} // namespace NativeEngine::IDE
