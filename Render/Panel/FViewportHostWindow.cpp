#include "PCH.h"

#include "FViewportHostWindow.h"

#include "FKeyboardInput.h"
#include "FMouseInput.h"
#include "Render/EditorView/FEditorViewport.h"
#include "Render/EditorView/EditorViewport.h"

FViewportHostWindow::FViewportHostWindow(ID3D11Device* Device, FWorldEditorContext& EditorContext)
    : FEditorWindow("Viewports###SplitSceneViewport", ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
    , Layout(EViewportLayoutPreset::FourGrid) {
    for (FViewportId Id = 0; Id < MaximumViewportCount; ++Id) {
        Viewports[Id] = std::make_unique<FEditorViewport>(Id, Device, EditorContext);
    }
}

FViewportHostWindow::~FViewportHostWindow() = default;

void FViewportHostWindow::PrepareFrame(ImGuiID InDockSpaceId) {
    DockSpaceId = InDockSpaceId;
    bSplitterActive = false;

    for (const std::unique_ptr<FEditorViewport>& Viewport : Viewports) {
        Viewport->BeginFrame();
    }
}

void FViewportHostWindow::ProcessInput(EditorViewport& Viewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime) {
    GetViewport(ActiveViewportId)->ProcessInput(Viewport, KeyboardInput, MouseInput, DeltaTime, bSplitterActive);
}

void FViewportHostWindow::ReleaseRenderResources() {
    for (const std::unique_ptr<FEditorViewport>& Viewport : Viewports) {
        Viewport->ReleaseRenderResources();
    }
}

FEditorViewport* FViewportHostWindow::PrepareViewportForRender(FViewportId Id) {
    FEditorViewport* Viewport = GetViewport(Id);
    return Viewport != nullptr && Viewport->PrepareForRender() ? Viewport : nullptr;
}

uint32 FViewportHostWindow::GetViewportCount() const {
    return Layout.GetViewportCount();
}

void FViewportHostWindow::DrawContents() {
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::BeginCombo("Layout", Layout.GetPresetName())) {
        for (uint8 Index = 0; Index < static_cast<uint8>(EViewportLayoutPreset::Count); ++Index) {
            const EViewportLayoutPreset Preset = static_cast<EViewportLayoutPreset>(Index);
            const bool bSelected = Layout.GetPreset() == Preset;
            if (ImGui::Selectable(FViewportPresetLayout::GetPresetName(Preset), bSelected)) {
                SetViewportLayout(Preset);
            }
            if (bSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();

    const ImVec2 MainViewportPosition = ImGui::GetMainViewport()->Pos;
    const ImVec2 Origin = ImGui::GetCursorScreenPos();
    const ImVec2 AvailableSize = ImGui::GetContentRegionAvail();
    const int32 Width = static_cast<int32>(std::max(0.0f, AvailableSize.x));
    const int32 Height = static_cast<int32>(std::max(0.0f, AvailableSize.y));

    if (Width <= 0 || Height <= 0) {
        return;
    }

    const FPoint Min{ static_cast<int32>(Origin.x), static_cast<int32>(Origin.y) };
    Layout.SetRect({ Min, { Min.X + Width, Min.Y + Height } });

    std::vector<SSplitter*> Splitters;
    Layout.CollectSplitters(Splitters);
    for (SSplitter* Splitter : Splitters) {
        bSplitterActive = DrawSplitterHandle(*Splitter) || bSplitterActive;
    }

    if (bSplitterActive) {
        Layout.RefreshLayout();
    }

    for (FViewportId Id = 0; Id < GetViewportCount(); ++Id) {
        FEditorViewport* Viewport = GetViewport(Id);
        if (Viewport != nullptr && Viewport->Draw(Layout.GetViewportRect(Id), MainViewportPosition, bSplitterActive)) {
            ActiveViewportId = Viewport->GetViewportId();
        }
    }

    const bool bHostFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    for (FViewportId Id = 0; Id < GetViewportCount(); ++Id) {
        FEditorViewport* Viewport = GetViewport(Id);
        if (Viewport != nullptr) {
            Viewport->SetFocused(bHostFocused && Id == ActiveViewportId);
        }
    }
}

void FViewportHostWindow::PushWindowStyle() {
    if (DockSpaceId != 0) {
        ImGui::SetNextWindowDockID(DockSpaceId, ImGuiCond_FirstUseEver);
    }
}

void FViewportHostWindow::SetViewportLayout(EViewportLayoutPreset InPreset) {
    if (Layout.GetPreset() == InPreset) {
        return;
    }

    Layout.SetPreset(InPreset);

    ActiveViewportId = 0;
    for (const std::unique_ptr<FEditorViewport>& Viewport : Viewports) {
        Viewport->BeginFrame();
    }
    bSplitterActive = false;
}

bool FViewportHostWindow::DrawSplitterHandle(SSplitter& Splitter) {
    const FRect Rect = Splitter.GetHandleRect();
    if (Rect.IsEmpty()) {
        return false;
    }

    ImGui::SetCursorScreenPos(ImVec2(static_cast<float>(Rect.Min.X), static_cast<float>(Rect.Min.Y)));
    ImGui::PushID(&Splitter);
    ImGui::InvisibleButton("##ViewportSplitter", ImVec2(static_cast<float>(Rect.GetWidth()), static_cast<float>(Rect.GetHeight())));
    ImGui::PopID();

    const bool bHovered = ImGui::IsItemHovered();
    const bool bActive = ImGui::IsItemActive();
    if (bHovered || bActive) {
        const ImGuiMouseCursor Cursor = Rect.GetWidth() < Rect.GetHeight() ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS;
        ImGui::SetMouseCursor(Cursor);
    }

    if (bActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 MousePosition = ImGui::GetMousePos();
        Splitter.DragTo({ static_cast<int32>(MousePosition.x), static_cast<int32>(MousePosition.y) });
    }

    const FRect UpdatedRect = Splitter.GetHandleRect();
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(static_cast<float>(UpdatedRect.Min.X), static_cast<float>(UpdatedRect.Min.Y)), ImVec2(static_cast<float>(UpdatedRect.Max.X), static_cast<float>(UpdatedRect.Max.Y)), bActive ? IM_COL32(100, 150, 220, 255) : bHovered ? IM_COL32(85, 85, 85, 255) : IM_COL32(55, 55, 55, 255));
    return bActive;
}

FEditorViewport* FViewportHostWindow::GetViewport(FViewportId Id) const {
    return Id < MaximumViewportCount ? Viewports[Id].get() : nullptr;
}
