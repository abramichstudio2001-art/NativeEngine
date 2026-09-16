#pragma once

#include "engine/ecs/Registry.hpp"
#include "engine/ui/UIRenderer.hpp"
#include <string>
#include <format>

namespace NativeEngine::UI::Widgets {

class InspectorPanel {
public:
    InspectorPanel() = default;

    void Render(float x, float y, float width, float height, ECS::Registry& registry, ECS::Entity selectedEntity) {
        auto& ui = UIRenderer::GetInstance();

        ui.DrawRectFilled(x, y, width, 24.0f, ui.GetTheme().headerBackground);
        ui.DrawText(x + 10.0f, y + 4.0f, "Inspector", ui.GetTheme().textColor);

        ui.DrawRectFilled(x, y + 24.0f, width, height - 24.0f, ui.GetTheme().windowBackground);

        if (selectedEntity == ECS::NullEntity || !registry.HasComponent<ECS::TagComponent>(selectedEntity)) {
            ui.DrawText(x + 10.0f, y + 40.0f, "No entity selected", ui.GetTheme().textDisabled);
            return;
        }

        float currentY = y + 30.0f;

        // Tag Component
        auto& tagComp = registry.GetComponent<ECS::TagComponent>(selectedEntity);
        ui.DrawText(x + 10.0f, currentY, std::format("Tag: {}", tagComp.tag), ui.GetTheme().textColor);
        currentY += 25.0f;

        // Transform Component
        if (registry.HasComponent<ECS::TransformComponent>(selectedEntity)) {
            auto& trans = registry.GetComponent<ECS::TransformComponent>(selectedEntity);
            ui.DrawRectFilled(x + 5.0f, currentY, width - 10.0f, 20.0f, ui.GetTheme().headerBackground);
            ui.DrawText(x + 10.0f, currentY + 2.0f, "Transform", ui.GetTheme().accentPrimary);
            currentY += 24.0f;

            ui.DrawText(x + 15.0f, currentY, std::format("Pos: [{:.2f}, {:.2f}, {:.2f}]", trans.position.x, trans.position.y, trans.position.z), ui.GetTheme().textColor);
            currentY += 20.0f;
            ui.DrawText(x + 15.0f, currentY, std::format("Rot: [{:.2f}, {:.2f}, {:.2f}]", trans.rotation.x, trans.rotation.y, trans.rotation.z), ui.GetTheme().textColor);
            currentY += 20.0f;
            ui.DrawText(x + 15.0f, currentY, std::format("Scl: [{:.2f}, {:.2f}, {:.2f}]", trans.scale.x, trans.scale.y, trans.scale.z), ui.GetTheme().textColor);
            currentY += 25.0f;
        }

        // MeshRenderer Component
        if (registry.HasComponent<ECS::MeshRendererComponent>(selectedEntity)) {
            auto& mesh = registry.GetComponent<ECS::MeshRendererComponent>(selectedEntity);
            ui.DrawRectFilled(x + 5.0f, currentY, width - 10.0f, 20.0f, ui.GetTheme().headerBackground);
            ui.DrawText(x + 10.0f, currentY + 2.0f, "Mesh Renderer", ui.GetTheme().accentPrimary);
            currentY += 24.0f;

            ui.DrawText(x + 15.0f, currentY, std::format("Mesh: {}", mesh.meshName), ui.GetTheme().textColor);
            currentY += 20.0f;
            ui.DrawText(x + 15.0f, currentY, std::format("Material: {}", mesh.materialName), ui.GetTheme().textColor);
            currentY += 25.0f;
        }

        // Light Component
        if (registry.HasComponent<ECS::LightComponent>(selectedEntity)) {
            auto& light = registry.GetComponent<ECS::LightComponent>(selectedEntity);
            ui.DrawRectFilled(x + 5.0f, currentY, width - 10.0f, 20.0f, ui.GetTheme().headerBackground);
            ui.DrawText(x + 10.0f, currentY + 2.0f, "Light Component", ui.GetTheme().accentPrimary);
            currentY += 24.0f;

            ui.DrawText(x + 15.0f, currentY, std::format("Intensity: {:.2f}", light.intensity), ui.GetTheme().textColor);
            currentY += 20.0f;
            ui.DrawText(x + 15.0f, currentY, std::format("Color: [{:.2f}, {:.2f}, {:.2f}]", light.color.r, light.color.g, light.color.b), ui.GetTheme().textColor);
            currentY += 25.0f;
        }
    }
};

} // namespace NativeEngine::UI::Widgets
