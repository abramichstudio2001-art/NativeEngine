#pragma once

#include "engine/ui/UIRenderer.hpp"
#include <string>
#include <vector>

namespace NativeEngine::IDE {

enum class ShaderNodeType {
    InputTexture,
    InputVector,
    Time,
    MathAdd,
    MathMultiply,
    NoiseGenerator,
    PBRSurfaceOutput
};

struct NodePin {
    uint32_t id;
    std::string name;
    bool isOutput{ false };
};

struct ShaderNode {
    uint32_t id;
    ShaderNodeType type;
    std::string title;
    float posX{ 100.0f }, posY{ 100.0f };
    std::vector<NodePin> pins;
};

struct NodeLink {
    uint32_t id;
    uint32_t startPinId;
    uint32_t endPinId;
};

class ShaderGraphPanel {
public:
    ShaderGraphPanel();

    void AddNode(ShaderNodeType type, float x, float y);
    void CreateLink(uint32_t startPin, uint32_t endPin);

    std::string GenerateHLSLCode();
    void Render(float x, float y, float width, float height);

    const std::vector<ShaderNode>& GetNodes() const { return m_Nodes; }

private:
    std::vector<ShaderNode> m_Nodes;
    std::vector<NodeLink> m_Links;
    uint32_t m_NextId{ 1 };
};

} // namespace NativeEngine::IDE
