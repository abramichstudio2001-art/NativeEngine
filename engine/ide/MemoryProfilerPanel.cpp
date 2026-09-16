#include "engine/ide/MemoryProfilerPanel.hpp"
#include <format>

namespace NativeEngine::IDE {

void MemoryProfilerPanel::Render(float x, float y, float width, float height) {
    auto& ui = UI::UIRenderer::GetInstance();
    auto metrics = Core::MemoryTracker::GetInstance().GetMetrics();

    ui.DrawRectFilled(x, y, width, 24.0f, ui.GetTheme().headerBackground);
    ui.DrawText(x + 10.0f, y + 4.0f, "Memory Profiler & Subsystem Heap Visualizer", ui.GetTheme().textColor);

    ui.DrawRectFilled(x, y + 24.0f, width, height - 24.0f, ui.GetTheme().windowBackground);

    float textY = y + 35.0f;
    ui.DrawText(x + 15.0f, textY, std::format("Total Allocated: {:.2f} MB", metrics.totalAllocatedBytes / (1024.0f * 1024.0f)), ui.GetTheme().textColor);
    textY += 20.0f;
    ui.DrawText(x + 15.0f, textY, std::format("Total Freed:     {:.2f} MB", metrics.totalFreedBytes / (1024.0f * 1024.0f)), ui.GetTheme().textColor);
    textY += 20.0f;
    ui.DrawText(x + 15.0f, textY, std::format("Active Heap:    {:.2f} MB", (metrics.totalAllocatedBytes - metrics.totalFreedBytes) / (1024.0f * 1024.0f)), ui.GetTheme().accentPrimary);
    textY += 20.0f;
    ui.DrawText(x + 15.0f, textY, std::format("Peak Heap:      {:.2f} MB", metrics.peakAllocatedBytes / (1024.0f * 1024.0f)), ui.GetTheme().textColor);
    textY += 30.0f;

    ui.DrawText(x + 15.0f, textY, "Heap Allocation Breakdown by Subsystem:", ui.GetTheme().textColor);
    textY += 25.0f;

    // Timeline bar visualizer
    float barWidth = width - 40.0f;
    ui.DrawRectFilled(x + 20.0f, textY, barWidth, 30.0f, UI::StyleColor{ 0.15f, 0.15f, 0.18f, 1.0f });

    size_t activeMem = metrics.totalAllocatedBytes > metrics.totalFreedBytes ? metrics.totalAllocatedBytes - metrics.totalFreedBytes : 1;
    float currentOffset = x + 20.0f;

    const char* cats[] = { "Core", "Renderer", "UI", "Scripting", "Audio", "Physics" };
    UI::StyleColor colors[] = {
        UI::StyleColor{ 0.2f, 0.6f, 1.0f, 1.0f },
        UI::StyleColor{ 0.9f, 0.3f, 0.3f, 1.0f },
        UI::StyleColor{ 0.3f, 0.8f, 0.3f, 1.0f },
        UI::StyleColor{ 0.8f, 0.8f, 0.2f, 1.0f },
        UI::StyleColor{ 0.7f, 0.3f, 0.9f, 1.0f },
        UI::StyleColor{ 0.3f, 0.9f, 0.9f, 1.0f }
    };

    for (int i = 0; i < 6; ++i) {
        float portion = static_cast<float>(metrics.categoryBytes[i]) / activeMem;
        float segmentW = barWidth * portion;
        if (segmentW > 0.0f) {
            ui.DrawRectFilled(currentOffset, textY, segmentW, 30.0f, colors[i]);
            currentOffset += segmentW;
        }
    }
}

} // namespace NativeEngine::IDE
