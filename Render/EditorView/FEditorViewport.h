#pragma once

#include "FEditorViewportTypes.h"
#include "FViewportGeometry.h"

class EditorViewport;
class FKeyboardInput;
class FMouseInput;
class FRenderer;
struct ImVec2;

class FEditorViewport {
public:
    FEditorViewport(FViewportId InViewportId, FRenderer& InRenderer);

    FEditorViewport(const FEditorViewport&) = delete;
    FEditorViewport& operator=(const FEditorViewport&) = delete;
    FEditorViewport(FEditorViewport&&) = delete;
    FEditorViewport& operator=(FEditorViewport&&) = delete;

    FViewportId GetViewportId() const;
    void BeginFrame();
    bool Draw(const FRect& Rect, const ImVec2& MainViewportPosition, bool bInputBlocked);
    void SetFocused(bool bInFocused);
    void ProcessInput(EditorViewport& SharedEditorViewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime, bool bInputBlocked);
    bool PrepareForRender();

private:
    void ResizeRenderSurface();

    FRenderer* Renderer = nullptr;
    FViewportId ViewportId = 0;
    FRect DisplayRect{};
    uint32 Width = 0;
    uint32 Height = 0;
    float RenderLeft = 0.0f;
    float RenderTop = 0.0f;
    bool bVisible = false;
    bool bHovered = false;
    bool bFocused = false;
};
