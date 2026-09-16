#pragma once

#include "engine/ecs/Registry.hpp"
#include "engine/ui/UIRenderer.hpp"
#include <string>

namespace NativeEngine::UI::Widgets {

class SceneHierarchyPanel {
public:
    SceneHierarchyPanel() = default;

    void Render(float x, float y, float width, float height, ECS::Registry& registry, ECS::Entity& selectedEntity) {
        auto& ui = UIRenderer::GetInstance();

        ui.DrawRectFilled(x, y, width, 24.0f, ui.GetTheme().headerBackground);
        ui.DrawText(x + 10.0f, y + 4.0f, "Scene Hierarchy", ui.GetTheme().textColor);

        ui.DrawRectFilled(x, y + 24.0f, width, height - 24.0f, ui.GetTheme().windowBackground);

        float currentY = y + 30.0f;
        const auto& entities = registry.GetEntities();

        for (ECS::Entity entity : entities) {
            std::string tag = "Entity " + std::to_string(entity);
            if (registry.HasComponent<ECS::TagComponent>(entity)) {
                tag = registry.GetComponent<ECS::TagComponent>(entity).tag;
            }

            bool isSelected = (entity == selectedEntity);
            StyleColor itemBg = isSelected ? ui.GetTheme().accentPrimary : ui.GetTheme().buttonBackground;

            ui.DrawRectFilled(x + 5.0f, currentY, width - 10.0f, 22.0f, itemBg);
            ui.DrawText(x + 15.0f, currentY + 3.0f, tag, ui.GetTheme().textColor);

            currentY += 26.0f;
            if (currentY + 26.0f > y + height) break;
        }
    }
};

} // namespace NativeEngine::UI::Widgets
