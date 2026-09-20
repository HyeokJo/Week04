#pragma once

#include "PCH.h"
#include "FEditorWindow.h"
#include "FControlPanel.h"
#include "FPropertyPanel.h"
#include "FConsolePanel.h"
#include "FStatPanel.h"
#include "FViewerPanel.h"
#include "FAssetBrowserPanel.h"
#include "Outliner.h"
#include "FViewerToolBar.h"

#include "Core/Channel/FStateChannel.h"
#include "../../Scene/FWorldEditorContext.h"

class FEditorUIManager {
public:
    void Initialize(UWorld& World, FAssetRegistry& AssetRegistry, FWorldEditorContext& EditorContext, HWND WindowHandle, FStateChannel<uint8>::FReadWriter GizmoSender, FStateChannel<uint8>::FReadWriter GizmoCoordinateSpaceSender) {
        AddPanel(std::make_unique<FControlPanel>(EditorContext, WindowHandle, EditorContext.GetEditorToWorldSender()));
        AddWindow(std::make_unique<FPropertyPanel>(EditorContext, std::move(GizmoSender), std::move(GizmoCoordinateSpaceSender)));
        AddWindow(std::make_unique<FConsolePanel>(Console::STDOutHandle));
        AddWindow(std::make_unique<FStatPanel>(World));
        AddWindow(std::make_unique<FAssetBrowserPanel>(AssetRegistry));
        AddWindow(std::make_unique<FOutlinerPanel>(World, EditorContext));
        AddViewerWindow(AssetRegistry, WindowHandle, EditorContext);
    }

    void InitializeViewer(FAssetRegistry& AssetRegistry, HWND WindowHandle, FWorldEditorContext& EditorContext) {
        AddPanel(std::make_unique<FViewerToolBar>(EditorContext));
        AddViewerWindow(AssetRegistry, WindowHandle, EditorContext);
    }

    void Tick() {
        DockSpaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        for (const std::unique_ptr<IEditorPanel>& Element : Elements) {
            if (Element != nullptr && Element->IsVisible()) {
                Element->DrawPanel();
            }
        }
    }

    void RenderOffscreen(FRenderer& Renderer, FAssetRegistry& AssetRegistry) {
        for (const std::unique_ptr<IEditorPanel>& Element : Elements) {
            if (Element != nullptr && Element->IsVisible()) {
                Element->RenderOffscreen(Renderer, AssetRegistry);
            }
        }
    }

    ImGuiID GetDockSpaceId() const {
        return DockSpaceId;
    }

    const std::vector<FEditorWindow*>& GetWindows() const {
        return Windows;
    }

private:
    void AddPanel(std::unique_ptr<IEditorPanel> Panel) {
        Elements.emplace_back(std::move(Panel));
    }

    void AddWindow(std::unique_ptr<FEditorWindow> Window) {
        Windows.emplace_back(Window.get());
        Elements.emplace_back(std::move(Window));
    }

    void AddViewerWindow(FAssetRegistry& AssetRegistry, HWND WindowHandle, FWorldEditorContext& EditorContext) {
        AddWindow(std::make_unique<FViewerPanel>(AssetRegistry, WindowHandle, EditorContext.GetEditorToWorldSender()));
    }

    std::vector<std::unique_ptr<IEditorPanel>> Elements;
    std::vector<FEditorWindow*> Windows;
    ImGuiID DockSpaceId = 0;
};
