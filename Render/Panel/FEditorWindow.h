#pragma once

#include "IEditorPanel.h"
#include "ImGui/imgui.h"

class FEditorWindow : public IEditorPanel {
public:
    explicit FEditorWindow(const char* InWindowName, ImGuiWindowFlags InWindowFlags = ImGuiWindowFlags_None) : WindowName(InWindowName), WindowFlags(InWindowFlags) {
    }

    ~FEditorWindow() override = default;

    FEditorWindow(const FEditorWindow&) = delete;
    FEditorWindow& operator=(const FEditorWindow&) = delete;
    FEditorWindow(FEditorWindow&&) = delete;
    FEditorWindow& operator=(FEditorWindow&&) = delete;

    void DrawPanel() final {
        PushWindowStyle();

        const bool bDrawContents = ImGui::Begin(WindowName, &bVisible, WindowFlags);
        if (bDrawContents) {
            DrawContents();
        }

        ImGui::End();
        PopWindowStyle();
    }

    const char* GetWindowName() const {
        return WindowName;
    }

protected:
    virtual void DrawContents() = 0;

    virtual void PushWindowStyle() {
    }

    virtual void PopWindowStyle() {
    }

private:
    const char* WindowName = nullptr;
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
};
