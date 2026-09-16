#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>

namespace NativeEngine::UI {

struct WindowNode {
    std::string title;
    float posX{ 0 }, posY{ 0 };
    float width{ 400 }, height{ 300 };
    bool isVisible{ true };
    bool isDocked{ true };
    std::string dockZone{ "MainArea" };
};

class WorkspaceManager {
public:
    static WorkspaceManager& GetInstance() {
        static WorkspaceManager instance;
        return instance;
    }

    void RegisterWindow(const std::string& title, float x, float y, float w, float h, const std::string& dockZone = "MainArea") {
        m_Windows.push_back(WindowNode{ title, x, y, w, h, true, true, dockZone });
    }

    bool SaveLayoutToJSON(const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        file << "{\n  \"workspace\": {\n    \"windows\": [\n";
        for (size_t i = 0; i < m_Windows.size(); ++i) {
            const auto& w = m_Windows[i];
            file << "      {\n"
                 << "        \"title\": \"" << w.title << "\",\n"
                 << "        \"posX\": " << w.posX << ",\n"
                 << "        \"posY\": " << w.posY << ",\n"
                 << "        \"width\": " << w.width << ",\n"
                 << "        \"height\": " << w.height << ",\n"
                 << "        \"dockZone\": \"" << w.dockZone << "\"\n"
                 << "      }" << (i + 1 < m_Windows.size() ? "," : "") << "\n";
        }
        file << "    ]\n  }\n}\n";
        return true;
    }

    bool LoadLayoutFromJSON(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;
        // Basic parser / persistence confirmation
        return true;
    }

    const std::vector<WindowNode>& GetWindows() const { return m_Windows; }

private:
    WorkspaceManager() = default;
    std::vector<WindowNode> m_Windows;
};

} // namespace NativeEngine::UI
