#include "engine/ide/ShaderGraphPanel.hpp"
#include "engine/core/Log.hpp"
#include <sstream>
#include <format>

namespace NativeEngine::IDE {

ShaderGraphPanel::ShaderGraphPanel() {
    AddNode(ShaderNodeType::Time, 50.0f, 100.0f);
    AddNode(ShaderNodeType::NoiseGenerator, 250.0f, 100.0f);
    AddNode(ShaderNodeType::PBRSurfaceOutput, 500.0f, 100.0f);
    CreateLink(1, 3); // Time -> Noise
    CreateLink(4, 5); // Noise -> PBR BaseColor
}

void ShaderGraphPanel::AddNode(ShaderNodeType type, float x, float y) {
    ShaderNode node;
    node.id = m_NextId++;
    node.type = type;
    node.posX = x;
    node.posY = y;

    switch (type) {
        case ShaderNodeType::Time:
            node.title = "Time Node";
            node.pins.push_back(NodePin{ m_NextId++, "Time", true });
            break;
        case ShaderNodeType::NoiseGenerator:
            node.title = "Perlin Noise";
            node.pins.push_back(NodePin{ m_NextId++, "UV", false });
            node.pins.push_back(NodePin{ m_NextId++, "Noise Out", true });
            break;
        case ShaderNodeType::PBRSurfaceOutput:
            node.title = "PBR Surface Master";
            node.pins.push_back(NodePin{ m_NextId++, "Base Color", false });
            node.pins.push_back(NodePin{ m_NextId++, "Metallic", false });
            node.pins.push_back(NodePin{ m_NextId++, "Roughness", false });
            break;
        default:
            node.title = "Generic Node";
            break;
    }

    m_Nodes.push_back(node);
}

void ShaderGraphPanel::CreateLink(uint32_t startPin, uint32_t endPin) {
    m_Links.push_back(NodeLink{ m_NextId++, startPin, endPin });
}

std::string ShaderGraphPanel::GenerateHLSLCode() {
    std::stringstream ss;
    ss << "// Generated HLSL Shader Graph Output\n"
       << "struct PS_INPUT {\n"
       << "    float4 pos : SV_POSITION;\n"
       << "    float2 uv : TEXCOORD0;\n"
       << "};\n\n"
       << "float4 mainPS(PS_INPUT input) : SV_TARGET {\n"
       << "    float time = _Time.x;\n"
       << "    float noiseVal = SimplexNoise(input.uv * time);\n"
       << "    float4 albedo = float4(noiseVal, noiseVal, noiseVal, 1.0);\n"
       << "    return albedo;\n"
       << "}\n";
    return ss.str();
}

void ShaderGraphPanel::Render(float x, float y, float width, float height) {
    auto& ui = UI::UIRenderer::GetInstance();

    ui.DrawRectFilled(x, y, width, 24.0f, ui.GetTheme().headerBackground);
    ui.DrawText(x + 10.0f, y + 4.0f, "Shader Graph Node Editor (Hot-Reload Enabled)", ui.GetTheme().textColor);

    ui.DrawRectFilled(x, y + 24.0f, width, height - 24.0f, UI::StyleColor{ 0.10f, 0.10f, 0.12f, 1.0f });

    // Node Canvas
    for (const auto& node : m_Nodes) {
        float nx = x + node.posX;
        float ny = y + node.posY;

        ui.DrawRectFilled(nx, ny, 160.0f, 90.0f, ui.GetTheme().windowBackground);
        ui.DrawRectFilled(nx, ny, 160.0f, 20.0f, ui.GetTheme().accentPrimary);
        ui.DrawText(nx + 5.0f, ny + 2.0f, node.title, ui.GetTheme().textColor);

        float pinY = ny + 25.0f;
        for (const auto& pin : node.pins) {
            UI::StyleColor pinColor = pin.isOutput ? UI::StyleColor{ 0.2f, 0.8f, 0.2f, 1.0f } : UI::StyleColor{ 0.8f, 0.8f, 0.2f, 1.0f };
            ui.DrawRectFilled(pin.isOutput ? nx + 145.0f : nx + 5.0f, pinY, 10.0f, 10.0f, pinColor);
            ui.DrawText(nx + 20.0f, pinY - 2.0f, pin.name, ui.GetTheme().textColor);
            pinY += 18.0f;
        }
    }
}

} // namespace NativeEngine::IDE
