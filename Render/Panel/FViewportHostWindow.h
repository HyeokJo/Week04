#pragma once

#include <array>

#include "FEditorWindow.h"
#include "Render/EditorView/SSplitter.h"
#include "Render/Renderer.h"

class EditorViewport;
class FKeyboardInput;
class FMouseInput;

class FViewportHostWindow final : public FEditorWindow {
public:
    using FViewportId = FRenderer::FViewportId;
    static constexpr uint32 ViewportCount = FRenderer::ViewportCount;

    explicit FViewportHostWindow(FRenderer& InRenderer);

    void PrepareFrame(ImGuiID InDockSpaceId);
    void ProcessInput(EditorViewport& Viewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime);
    bool PrepareViewportForRender(FViewportId Id);

private:
    struct FViewportFrame {
        ImVec2 Position{};
        ImVec2 Size{};
        uint32 Width = 0;
        uint32 Height = 0;
        float RenderLeft = 0.0f;
        float RenderTop = 0.0f;
        bool bVisible = false;
        bool bHovered = false;
        bool bFocused = false;
    };

    void DrawContents() override;
    void PushWindowStyle() override;
    bool DrawSplitterHandle(const char* Id, SSplitter& Splitter, ImGuiMouseCursor Cursor);
    void DrawViewport(FViewportId Id, const ImVec2& MainViewportPosition);
    void ResizeViewportSurface(FViewportId Id);

    FRenderer* Renderer = nullptr;
    std::array<SWindow, ViewportCount> ViewportRegions{};
    std::array<FViewportFrame, ViewportCount> ViewportFrames{};
    SSplitterH RootSplit{};
    SSplitterV TopSplit{};
    SSplitterV BottomSplit{};
    FViewportId ActiveViewportId = 0;
    ImGuiID DockSpaceId = 0;
    bool bSplitterActive = false;
};
