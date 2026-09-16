#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <cstdint>

namespace NativeEngine::ECS {

struct Vector3 {
    float x{ 0.0f }, y{ 0.0f }, z{ 0.0f };
};

struct Quaternion {
    float x{ 0.0f }, y{ 0.0f }, z{ 0.0f }, w{ 1.0f };
};

struct Color {
    float r{ 1.0f }, g{ 1.0f }, b{ 1.0f }, a{ 1.0f };
};

struct TagComponent {
    std::string tag{ "Entity" };
};

struct TransformComponent {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation{ 0.0f, 0.0f, 0.0f }; // Euler angles in degrees
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

struct MeshRendererComponent {
    std::string meshName{ "Cube" };
    std::string materialName{ "DefaultMaterial" };
    bool castShadows{ true };
    bool receiveShadows{ true };
};

enum class LightType {
    Directional,
    Point,
    Spot
};

struct LightComponent {
    LightType type{ LightType::Directional };
    Color color{ 1.0f, 1.0f, 1.0f, 1.0f };
    float intensity{ 1.0f };
    float range{ 10.0f };
    float spotAngle{ 45.0f };
};

struct CameraComponent {
    float fov{ 60.0f };
    float nearClip{ 0.1f };
    float farClip{ 1000.0f };
    bool isPrimary{ true };
};

struct MaterialComponent {
    Color albedo{ 1.0f, 1.0f, 1.0f, 1.0f };
    float metallic{ 0.0f };
    float roughness{ 0.5f };
    float normalScale{ 1.0f };
    std::string shaderPath{ "shaders/PBRSurface.hlsl" };
};

struct HierarchyComponent {
    uint32_t parentEntity{ 0 };
    std::vector<uint32_t> children;
};

struct ScriptComponent {
    std::string scriptClassName{ "PlayerController" };
    bool enabled{ true };
};

} // namespace NativeEngine::ECS
