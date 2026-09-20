#include "PCH.h"

#include "FViewportHostWindow.h"

#include "FKeyboardInput.h"
#include "FMouseInput.h"
#include "Render/EditorView/EditorViewport.h"

namespace {
constexpr std::array<const char*, FViewportHostWindow::MaximumViewportCount> ViewportChildIds{
    "##SceneViewport0",
    "##SceneViewport1",
    "##SceneViewport2",
    "##SceneViewport3"
};
}

FViewportHostWindow::FViewportHostWindow(FRenderer& InRenderer)
    : FEditorWindow("Viewports###SplitSceneViewport", ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
    , Renderer(&InRenderer)
    , Layout(FViewportLayout::CreateFourPane({ 0, 1, 2, 3 })) {
}

void FViewportHostWindow::PrepareFrame(ImGuiID InDockSpaceId) {
    DockSpaceId = InDockSpaceId;
    ViewportFrames.fill({});
    bSplitterActive = false;
}

void FViewportHostWindow::ProcessInput(EditorViewport& Viewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime) {
    const FViewportFrame& InputFrame = ViewportFrames[ActiveViewportId];

    if (InputFrame.bVisible) {
        ResizeViewportSurface(ActiveViewportId);
    }

    const bool bBlockMouse = bSplitterActive || !InputFrame.bVisible || !InputFrame.bHovered;
    const bool bBlockKeyboard = bSplitterActive || !InputFrame.bVisible || !InputFrame.bFocused || ImGui::GetIO().WantCaptureKeyboard;

    Viewport.ProcessInput(KeyboardInput, MouseInput, bBlockMouse);
    MouseInput.DispatchPendingWorldCommands(InputFrame.Width, InputFrame.Height, bBlockMouse);
    KeyboardInput.DispatchPendingWorldCommands(DeltaTime, bBlockKeyboard);
}

bool FViewportHostWindow::PrepareViewportForRender(FViewportId Id) {
    if (Id >= MaximumViewportCount || !ViewportFrames[Id].bVisible) {
        return false;
    }

    ResizeViewportSurface(Id);
    return true;
}

bool FViewportHostWindow::SplitViewport(FViewportId TargetViewportId, EViewportSplitDirection Direction) {
    const FViewportId NewViewportId = FindAvailableViewportId();
    if (NewViewportId >= MaximumViewportCount) {
        return false;
    }

    return Layout.SplitLeaf(TargetViewportId, NewViewportId, Direction);
}

bool FViewportHostWindow::RemoveViewport(FViewportId ViewportId) {
    if (!Layout.RemoveLeaf(ViewportId)) {
        return false;
    }

    if (ActiveViewportId == ViewportId) {
        std::vector<FViewportLeafNode*> Leaves;
        Layout.CollectLeaves(Leaves);
        ActiveViewportId = Leaves.front()->GetViewportId();
    }

    return true;
}

uint32 FViewportHostWindow::GetViewportCount() const {
    return Layout.GetLeafCount();
}

void FViewportHostWindow::DrawContents() {
    if (ImGui::Button("Cycle Viewports")) {
        CycleViewportCount();
    }

    ImGui::SameLine();
    ImGui::Text("Count: %u / %u", GetViewportCount(), MaximumViewportCount);
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

    std::vector<FViewportSplitterNode*> Splitters;
    Layout.CollectSplitters(Splitters);
    for (FViewportSplitterNode* Splitter : Splitters) {
        bSplitterActive = DrawSplitterHandle(*Splitter) || bSplitterActive;
    }

    if (bSplitterActive) {
        Layout.RefreshLayout();
    }

    std::vector<FViewportLeafNode*> Leaves;
    Layout.CollectLeaves(Leaves);
    for (FViewportLeafNode* Leaf : Leaves) {
        DrawViewport(Leaf->GetViewportId(), Leaf->GetRect(), MainViewportPosition);
    }

    const bool bHostFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    for (FViewportLeafNode* Leaf : Leaves) {
        FViewportFrame& Frame = ViewportFrames[Leaf->GetViewportId()];
        Frame.bFocused = bHostFocused && Leaf->GetViewportId() == ActiveViewportId;
    }
}

void FViewportHostWindow::PushWindowStyle() {
    if (DockSpaceId != 0) {
        ImGui::SetNextWindowDockID(DockSpaceId, ImGuiCond_FirstUseEver);
    }
}

void FViewportHostWindow::CycleViewportCount() {
    const uint32 NextViewportCount = GetViewportCount() >= MaximumViewportCount ? 1 : GetViewportCount() + 1;
    Layout = FViewportLayout::CreateFourPane({ 0, 1, 2, 3 });

    for (FViewportId Id = MaximumViewportCount; Id > NextViewportCount; --Id) {
        Layout.RemoveLeaf(Id - 1);
    }

    ActiveViewportId = 0;
    ViewportFrames.fill({});
    bSplitterActive = false;
}

bool FViewportHostWindow::DrawSplitterHandle(FViewportSplitterNode& Splitter) {
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
        const ImGuiMouseCursor Cursor = Splitter.GetDirection() == EViewportSplitDirection::Horizontal ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS;
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

void FViewportHostWindow::DrawViewport(FViewportId Id, const FRect& Rect, const ImVec2& MainViewportPosition) {
    if (Rect.IsEmpty()) {
        return;
    }

    FViewportFrame& Frame = ViewportFrames[Id];
    Frame.Position = ImVec2(static_cast<float>(Rect.Min.X), static_cast<float>(Rect.Min.Y));
    Frame.Size = ImVec2(static_cast<float>(Rect.GetWidth()), static_cast<float>(Rect.GetHeight()));
    Frame.Width = static_cast<uint32>(Rect.GetWidth());
    Frame.Height = static_cast<uint32>(Rect.GetHeight());
    Frame.RenderLeft = Frame.Position.x - MainViewportPosition.x;
    Frame.RenderTop = Frame.Position.y - MainViewportPosition.y;
    Frame.bVisible = true;

    ResizeViewportSurface(Id);

    ImGui::SetCursorScreenPos(Frame.Position);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool bChildVisible = ImGui::BeginChild(ViewportChildIds[Id], Frame.Size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (bChildVisible) {
        const ImVec2 ImagePosition = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##SceneSurface", Frame.Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

        if (ID3D11ShaderResourceView* ShaderResourceView = Renderer->GetSceneShaderResourceView(Id)) {
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(ShaderResourceView), ImagePosition, ImVec2(ImagePosition.x + Frame.Size.x, ImagePosition.y + Frame.Size.y));
        }

        Frame.bHovered = !bSplitterActive && ImGui::IsItemHovered();
        if (Frame.bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
            ActiveViewportId = Id;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void FViewportHostWindow::ResizeViewportSurface(FViewportId Id) {
    const FViewportFrame& Frame = ViewportFrames[Id];
    Renderer->ResizeSceneSurface(Id, Frame.Width, Frame.Height, Frame.RenderLeft, Frame.RenderTop);
}

FViewportHostWindow::FViewportId FViewportHostWindow::FindAvailableViewportId() const {
    for (FViewportId Id = 0; Id < MaximumViewportCount; ++Id) {
        if (Layout.FindLeaf(Id) == nullptr) {
            return Id;
        }
    }

    return MaximumViewportCount;
}
