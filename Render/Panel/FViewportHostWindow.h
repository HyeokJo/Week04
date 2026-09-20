#pragma once

#include <array>

#include "FEditorWindow.h"
#include "Render/EditorView/FViewportLayout.h"
#include "Render/Renderer.h"

class EditorViewport;
class FKeyboardInput;
class FMouseInput;

class FViewportHostWindow final : public FEditorWindow {
public:
    using FViewportId = ::FViewportId;
    static constexpr uint32 MaximumViewportCount = FViewportLayout::MaximumViewportCount;

    static_assert(MaximumViewportCount <= FRenderer::ViewportCount);

    explicit FViewportHostWindow(FRenderer& InRenderer);

    void PrepareFrame(ImGuiID InDockSpaceId);
    void ProcessInput(EditorViewport& Viewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime);
    bool PrepareViewportForRender(FViewportId Id);
    bool SplitViewport(FViewportId TargetViewportId, EViewportSplitDirection Direction);
    bool RemoveViewport(FViewportId ViewportId);
    uint32 GetViewportCount() const;

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
    void CycleViewportCount();
    bool DrawSplitterHandle(FViewportSplitterNode& Splitter);
    void DrawViewport(FViewportId Id, const FRect& Rect, const ImVec2& MainViewportPosition);
    void ResizeViewportSurface(FViewportId Id);
    FViewportId FindAvailableViewportId() const;

    FRenderer* Renderer = nullptr;
    FViewportLayout Layout;
    std::array<FViewportFrame, MaximumViewportCount> ViewportFrames{};
    FViewportId ActiveViewportId = 0;
    ImGuiID DockSpaceId = 0;
    bool bSplitterActive = false;
};
