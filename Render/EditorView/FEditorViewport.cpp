#include "PCH.h"

#include "FEditorViewport.h"

#include "EditorViewport.h"
#include "FKeyboardInput.h"
#include "FMouseInput.h"
#include "ImGui/imgui.h"
#include "Core/Base/FTransform.h"
#include "Scene/Component/UCameraComponent.h"
#include "Scene/FWorldEditorContext.h"

FEditorViewport::FEditorViewport(FViewportId InViewportId, ID3D11Device* InDevice, FWorldEditorContext& InEditorContext)
    : Device(InDevice)
    , EditorContext(&InEditorContext)
    , ViewportId(InViewportId)
    , ProjectionType(InViewportId == 0 ? EProjectionType::Perspective : EProjectionType::Orthographic) {
    if (Device != nullptr) {
        RenderSurface.InitializeOffscreen(Device, 1, 1);
    }
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

        if (ID3D11ShaderResourceView* ShaderResourceView = RenderSurface.GetShaderResourceView()) {
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

    CameraProbe Camera{};
    const bool bHasCamera = BuildCameraProbe(Camera);
    const bool bBlockMouse = bInputBlocked || !bVisible || !bHovered || !bHasCamera;

    if (bHasCamera) {
        SharedEditorViewport.PrepareInput(Camera, BuildInputViewport());
    }

    SharedEditorViewport.ProcessInput(KeyboardInput, MouseInput, bBlockMouse);
    const FViewportMouseNavigationInput MouseNavigation = MouseInput.DispatchPendingViewportCommands(static_cast<int32>(RenderLeft), static_cast<int32>(RenderTop), Width, Height, Camera.ViewProjection, bBlockMouse);

    if (bHasCamera) {
        ApplyMouseNavigation(MouseNavigation);
    }

    const bool bViewportNavigationActive = MouseInput.IsWorldDragActive(Right);
    const bool bImGuiOwnsKeyboard = ImGui::GetIO().WantCaptureKeyboard && !bViewportNavigationActive;
    const bool bBlockKeyboard = bInputBlocked || !bVisible || !bFocused || !bHasCamera || bImGuiOwnsKeyboard;
    const FViewportKeyboardNavigationInput KeyboardNavigation = KeyboardInput.ConsumeViewportNavigation(DeltaTime, bBlockKeyboard);

    if (bHasCamera) {
        ApplyKeyboardNavigation(KeyboardNavigation);
    }
}

bool FEditorViewport::PrepareForRender() {
    if (!bVisible) {
        return false;
    }

    ResizeRenderSurface();
    return RenderSurface.IsValid();
}

void FEditorViewport::ResizeRenderSurface() {
    if (Device == nullptr || Width == 0 || Height == 0) {
        return;
    }

    const D3D11_VIEWPORT& Viewport = RenderSurface.GetViewport();
    if (!RenderSurface.IsValid() || Viewport.Width != static_cast<float>(Width) || Viewport.Height != static_cast<float>(Height)) {
        RenderSurface.Resize(Device, Width, Height);
    }
}

bool FEditorViewport::BuildCameraProbe(CameraProbe& OutCamera) {
    if (!InitializeCameraFromWorldState() || Width == 0 || Height == 0) {
        return false;
    }

    const FTransform CameraTransform{ CameraPosition, CameraRotation, FVector3{ 1.0f, 1.0f, 1.0f } };
    const float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    OutCamera.View = (UCameraComponent::CameraBasis * CameraTransform.ToMatrixNoScale()).Invert();

    if (ProjectionType == EProjectionType::Perspective) {
        OutCamera.Projection = FMatrix::CreatePerspectiveFieldOfView(FieldOfView, AspectRatio, NearPlane, FarPlane);
    } else {
        OutCamera.Projection = FMatrix::CreateOrthographic(OrthographicWidth, OrthographicWidth / AspectRatio, NearPlane, FarPlane);
    }

    OutCamera.ViewProjection = OutCamera.View * OutCamera.Projection;
    return true;
}

bool FEditorViewport::InitializeCameraFromWorldState() {
    if (bCameraInitialized) {
        return true;
    }

    const FCameraSnapshot* CameraState = EditorContext != nullptr ? EditorContext->GetCameraState() : nullptr;
    if (CameraState == nullptr) {
        return false;
    }

    CameraPosition = CameraState->Position;
    CameraRotation = CameraState->RotationQuaternion;
    FieldOfView = CameraState->FOV;
    NearPlane = CameraState->NearPlane;
    FarPlane = CameraState->FarPlane;
    bCameraInitialized = true;
    return true;
}

void FEditorViewport::ApplyMouseNavigation(const FViewportMouseNavigationInput& NavigationInput) {
    if (NavigationInput.RotationDeltaX == 0.0f && NavigationInput.RotationDeltaY == 0.0f) {
        return;
    }

    const FCameraSnapshot* CameraState = EditorContext != nullptr ? EditorContext->GetCameraState() : nullptr;
    const float RotationSensitivity = (CameraState != nullptr ? CameraState->RotationSensitivity : 1.0f) * 0.001f;
    constexpr float MaximumPitch = 0.99f;

    FQuat YawDelta = FQuat::CreateFromAxisAngle(FVector3::UnitZ, NavigationInput.RotationDeltaX * RotationSensitivity);
    YawDelta.Normalize();

    FQuat YawedRotation = FQuat::Concatenate(YawDelta, CameraRotation);
    YawedRotation.Normalize();

    FTransform YawedTransform;
    YawedTransform.SetRotation(YawedRotation);
    const FMatrix YawMatrix = YawedTransform.ToMatrixWithScale();
    FVector3 Right = YawMatrix.Right();
    FVector3 Forward = YawMatrix.Forward();
    Right.Normalize();
    Forward.Normalize();

    const float ForwardUp = Forward.Dot(FVector3::UnitZ);
    float PitchAngle = NavigationInput.RotationDeltaY * RotationSensitivity;
    if ((ForwardUp > MaximumPitch && NavigationInput.RotationDeltaY > 0.0f) || (ForwardUp < -MaximumPitch && NavigationInput.RotationDeltaY < 0.0f)) {
        PitchAngle = 0.0f;
    }

    FQuat PitchDelta = FQuat::CreateFromAxisAngle(Right, PitchAngle);
    PitchDelta.Normalize();

    FQuat WorldDelta = FQuat::Concatenate(PitchDelta, YawDelta);
    WorldDelta.Normalize();
    CameraRotation = FQuat::Concatenate(WorldDelta, CameraRotation);
    CameraRotation.Normalize();
}

void FEditorViewport::ApplyKeyboardNavigation(const FViewportKeyboardNavigationInput& NavigationInput) {
    if (NavigationInput.DeltaTime <= 0.0f || (NavigationInput.ForwardAxis == 0.0f && NavigationInput.RightAxis == 0.0f)) {
        return;
    }

    const FTransform CameraTransform{ CameraPosition, CameraRotation, FVector3{ 1.0f, 1.0f, 1.0f } };
    const FMatrix CameraWorldMatrix = CameraTransform.ToMatrixNoScale();
    const FVector3 ForwardDirection = CameraWorldMatrix.Forward();
    const FVector3 RightDirection = -CameraWorldMatrix.Right();
    FVector3 MoveDirection = ForwardDirection * NavigationInput.ForwardAxis + RightDirection * NavigationInput.RightAxis;

    if (MoveDirection.LengthSquared() <= 0.0f) {
        return;
    }

    MoveDirection.Normalize();
    const FCameraSnapshot* CameraState = EditorContext != nullptr ? EditorContext->GetCameraState() : nullptr;
    const float MoveSensitivity = CameraState != nullptr ? CameraState->MoveSensitivity : 5.0f;
    CameraPosition = CameraPosition + MoveDirection * MoveSensitivity * NavigationInput.DeltaTime;
}

FSceneRenderSurface& FEditorViewport::GetRenderSurface() {
    return RenderSurface;
}

const D3D11_VIEWPORT& FEditorViewport::GetRenderViewport() const {
    return RenderSurface.GetViewport();
}

const FRenderSettings& FEditorViewport::GetRenderSettings() const {
    return RenderSettings;
}

void FEditorViewport::ReleaseRenderResources() {
    RenderSurface.Reset();
}

D3D11_VIEWPORT FEditorViewport::BuildInputViewport() const {
    return D3D11_VIEWPORT{ RenderLeft, RenderTop, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
}
