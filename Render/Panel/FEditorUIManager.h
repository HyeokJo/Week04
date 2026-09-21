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
#include "FViewportHostWindow.h"


#include "Core/Channel/FStateChannel.h"
#include "../../Scene/FWorldEditorContext.h"
// Initialize 에서 Renderer.GetDevice() 를 호출하므로 완전한 정의가 필요하다.
#include "../Renderer.h"

#include "FEditorInfo.h"

class FRenderer;

class FEditorUIManager {
public:
    void Initialize(UWorld& World, FRenderer& Renderer, FAssetRegistry& AssetRegistry, FWorldEditorContext& EditorContext, HWND WindowHandle, FStateChannel<uint8>::FReadWriter GizmoSender, FStateChannel<uint8>::FReadWriter GizmoCoordinateSpaceSender) {
        AddPanel(std::make_unique<FControlPanel>(EditorContext, WindowHandle, EditorContext.GetEditorToWorldSender()));
        AddViewportHostWindow(Renderer.GetDevice(), EditorContext);
        AddWindow(std::make_unique<FPropertyPanel>(EditorContext, std::move(GizmoSender), std::move(GizmoCoordinateSpaceSender)));
        AddWindow(std::make_unique<FConsolePanel>(Console::STDOutHandle, StatDisplayChannel.GetWriter()));
        //AddWindow(std::make_unique<FStatPanel>(World, StatDisplayChannel.GetReader()));
        AddStatWindow(std::make_unique<FStatPanel>(World, StatDisplayChannel.GetReader()));
        AddWindow(std::make_unique<FAssetBrowserPanel>(AssetRegistry, EditorContext));
        AddWindow(std::make_unique<FOutlinerPanel>(World, EditorContext));
        AddViewerWindow(AssetRegistry, EditorContext, WindowHandle);
    }

    void InitializeViewer(FAssetRegistry& AssetRegistry, HWND WindowHandle, FWorldEditorContext& EditorContext) {
        AddPanel(std::make_unique<FViewerToolBar>(EditorContext));
        AddViewerWindow(AssetRegistry, EditorContext, WindowHandle);
    }

    void Tick() {
        DockSpaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        // 닫혀 있는 Viewer 는 스스로 다시 열 수 없다. DrawPanel 이 호출되지 않기 때문이다.
        // 아웃라이너의 더블클릭 요청을 여기서 받아 창을 켜고 앞으로 끌어올린다.
        if (ViewerWindow != nullptr && PreviewContext != nullptr && PreviewContext->ConsumePreviewOpenRequest()) {
            ViewerWindow->SetVisible(true);
            ImGui::SetWindowFocus(ViewerWindow->GetWindowName());
        }

        if (ViewportHostWindow != nullptr) {
            ViewportHostWindow->PrepareFrame(DockSpaceId);
        }

        if (StatWindow != nullptr)
        {
            if (StatWindow->CheckVisible())
            {
                StatWindow->SetVisible(false);
            }
            else
            {
                StatWindow->SetVisible(true);
            }
        }
        

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

    void ReleaseRenderResources() {
        for (const std::unique_ptr<IEditorPanel>& Element : Elements) {
            if (Element != nullptr) {
                Element->ReleaseRenderResources();
            }
        }
    }

    ImGuiID GetDockSpaceId() const {
        return DockSpaceId;
    }

    const std::vector<FEditorWindow*>& GetWindows() const {
        return Windows;
    }

    FViewportHostWindow* GetViewportHostWindow() const {
        return ViewportHostWindow;
    }

    bool HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition) {
        return AssetBrowserPanel != nullptr && AssetBrowserPanel->IsVisible() &&
            AssetBrowserPanel->HandleExternalFileDrop(FilePath, ScreenPosition);
    }

private:
    void AddPanel(std::unique_ptr<IEditorPanel> Panel) {
        Elements.emplace_back(std::move(Panel));
    }

    void AddWindow(std::unique_ptr<FEditorWindow> Window)
    {
        Windows.emplace_back(Window.get());
        Elements.emplace_back(std::move(Window));
    }

    void AddViewerWindow(FAssetRegistry& AssetRegistry, FWorldEditorContext& EditorContext, HWND WindowHandle) {
        std::unique_ptr<FViewerPanel> Window = std::make_unique<FViewerPanel>(AssetRegistry, WindowHandle, EditorContext.GetEditorToWorldSender(), EditorContext);
        ViewerWindow = Window.get();
        PreviewContext = &EditorContext;
        AddWindow(std::move(Window));
    }

    void AddViewportHostWindow(ID3D11Device* Device, FWorldEditorContext& EditorContext) {
        std::unique_ptr<FViewportHostWindow> Window = std::make_unique<FViewportHostWindow>(Device, EditorContext);
        ViewportHostWindow = Window.get();
        AddWindow(std::move(Window));
    }

    void AddStatWindow(std::unique_ptr<FStatPanel> Window)
    {
        StatWindow = Window.get();
        Windows.emplace_back(Window.get());
        Elements.emplace_back(std::move(Window));

    }

    std::vector<std::unique_ptr<IEditorPanel>> Elements;
    std::vector<FEditorWindow*> Windows;
    FViewportHostWindow* ViewportHostWindow = nullptr;
    FViewerPanel* ViewerWindow = nullptr;
    FWorldEditorContext* PreviewContext = nullptr;
    ImGuiID DockSpaceId = 0;
    FAssetBrowserPanel* AssetBrowserPanel = nullptr;

    //Stat 커멘드 용
    FStateChannel<FStatDisplayFlags> StatDisplayChannel{ FStatDisplayFlags{false, false, false} };
    FStatPanel* StatWindow = nullptr;
};
