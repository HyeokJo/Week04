#pragma once

#include <array>

#include "FEditorWindow.h"
#include "Render/EditorView/FViewportPresetLayout.h"
#include "Serialize/FEditorSettings.h"

class EditorViewport;
class FEditorViewport;
class FKeyboardInput;
class FMouseInput;
class FWorldEditorContext;
struct ID3D11Device;

class FViewportHostWindow final : public FEditorWindow {
public:
    using FViewportId = ::FViewportId;
    static constexpr uint32 MaximumViewportCount = FViewportPresetLayout::MaximumViewportCount;

    FViewportHostWindow(ID3D11Device* Device, FWorldEditorContext& EditorContext);
    ~FViewportHostWindow() override;

    void PrepareFrame(ImGuiID InDockSpaceId);
    void ProcessInput(EditorViewport& Viewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime);
    void ReleaseRenderResources() override;
    FEditorViewport* PrepareViewportForRender(FViewportId Id);
    uint32 GetViewportCount() const;

    void ApplyLayoutSettings(const FEditorSettings& Settings);
    void CaptureLayoutSettings(FEditorSettings& Settings) const;

private:
    void DrawContents() override;
    void PushWindowStyle() override;
    void SetViewportLayout(EViewportLayoutPreset InPreset);
    bool DrawSplitterHandle(SSplitter& Splitter);
    FEditorViewport* GetViewport(FViewportId Id) const;

    FViewportPresetLayout Layout;
    std::array<std::unique_ptr<FEditorViewport>, MaximumViewportCount> Viewports{};
    FViewportId ActiveViewportId = 0;
    ImGuiID DockSpaceId = 0;
    bool bSplitterActive = false;

    FEditorSettings PendingLayoutSettings{};
    bool bHasPendingLayoutSettings = true;
};
