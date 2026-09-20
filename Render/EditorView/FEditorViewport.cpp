#include "PCH.h"

#include "FEditorViewport.h"

#include "EditorViewport.h"
#include "FKeyboardInput.h"
#include "FMouseInput.h"
#include "ImGui/imgui.h"
#include "Render/Renderer.h"

FEditorViewport::FEditorViewport(FViewportId InViewportId, FRenderer& InRenderer) : Renderer(&InRenderer), ViewportId(InViewportId) {
}

FViewportId FEditorViewport::GetViewportId() const {
    return ViewportId;
}

void FEditorViewport::BeginFrame() {
    DisplayRect = {};
    Width = 0;
    Height = 0;
    RenderLeft = 0.0f;
    RenderTop = 0.0f;
    bVisible = false;
    bHovered = false;
    bFocused = false;
}

bool FEditorViewport::Draw(const FRect& Rect, const ImVec2& MainViewportPosition, bool bInputBlocked) {
    if (Rect.IsEmpty()) {
        return false;
    }

    DisplayRect = Rect;
    Width = static_cast<uint32>(Rect.GetWidth());
    Height = static_cast<uint32>(Rect.GetHeight());
    RenderLeft = static_cast<float>(Rect.Min.X) - MainViewportPosition.x;
    RenderTop = static_cast<float>(Rect.Min.Y) - MainViewportPosition.y;
    bVisible = true;

    ResizeRenderSurface();

    const ImVec2 Position{ static_cast<float>(Rect.Min.X), static_cast<float>(Rect.Min.Y) };
    const ImVec2 Size{ static_cast<float>(Rect.GetWidth()), static_cast<float>(Rect.GetHeight()) };
    ImGui::SetCursorScreenPos(Position);
    ImGui::PushID(static_cast<int>(ViewportId));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool bChildVisible = ImGui::BeginChild("##SceneViewport", Size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    bool bActivated = false;

    if (bChildVisible) {
        const ImVec2 ImagePosition = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##SceneSurface", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

        if (ID3D11ShaderResourceView* ShaderResourceView = Renderer->GetSceneShaderResourceView(ViewportId)) {
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(ShaderResourceView), ImagePosition, ImVec2(ImagePosition.x + Size.x, ImagePosition.y + Size.y));
        }

        bHovered = !bInputBlocked && ImGui::IsItemHovered();
        bActivated = bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right));
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopID();
    return bActivated;
}

void FEditorViewport::SetFocused(bool bInFocused) {
    bFocused = bInFocused;
}

void FEditorViewport::ProcessInput(EditorViewport& SharedEditorViewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime, bool bInputBlocked) {
    if (bVisible) {
        ResizeRenderSurface();
    }

    const bool bBlockMouse = bInputBlocked || !bVisible || !bHovered;
    const bool bBlockKeyboard = bInputBlocked || !bVisible || !bFocused || ImGui::GetIO().WantCaptureKeyboard;

    SharedEditorViewport.ProcessInput(KeyboardInput, MouseInput, bBlockMouse);
    MouseInput.DispatchPendingWorldCommands(Width, Height, bBlockMouse);
    KeyboardInput.DispatchPendingWorldCommands(DeltaTime, bBlockKeyboard);
}

bool FEditorViewport::PrepareForRender() {
    if (!bVisible) {
        return false;
    }

    ResizeRenderSurface();
    return true;
}

void FEditorViewport::ResizeRenderSurface() {
    Renderer->ResizeSceneSurface(ViewportId, Width, Height, RenderLeft, RenderTop);
}
