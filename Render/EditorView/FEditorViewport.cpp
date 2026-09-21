#include "PCH.h"

#include "FEditorViewport.h"

#include "EditorViewport.h"
#include "FKeyboardInput.h"
#include "FMouseInput.h"
#include "ImGui/imgui.h"
#include "Core/Base/FTransform.h"
#include "Scene/Component/UCameraComponent.h"
#include "Scene/FWorldEditorContext.h"

namespace {
    constexpr float HalfSqrtTwo = 0.70710678118f;

    const char* GetOrthographicViewName(EOrthographicView View) {
        switch (View) {
        case EOrthographicView::Front: return "Front";
        case EOrthographicView::Back: return "Back";
        case EOrthographicView::Left: return "Left";
        case EOrthographicView::Right: return "Right";
        case EOrthographicView::Top: return "Top";
        case EOrthographicView::Bottom: return "Bottom";
        default: return "Orthographic";
        }
    }

    FQuat GetOrthographicRotation(EOrthographicView View) {
        switch (View) {
        case EOrthographicView::Front: return { 0.0f, 0.0f, 1.0f, 0.0f };
        case EOrthographicView::Back: return {};
        case EOrthographicView::Left: return { 0.0f, 0.0f, -HalfSqrtTwo, HalfSqrtTwo };
        case EOrthographicView::Right: return { 0.0f, 0.0f, HalfSqrtTwo, HalfSqrtTwo };
        case EOrthographicView::Top: return { -HalfSqrtTwo, 0.0f, 0.0f, HalfSqrtTwo };
        case EOrthographicView::Bottom: return { HalfSqrtTwo, 0.0f, 0.0f, HalfSqrtTwo };
        default: return {};
        }
    }
}

FEditorViewport::FEditorViewport(FViewportId InViewportId, ID3D11Device* InDevice, FWorldEditorContext& InEditorContext)
    : Device(InDevice)
    , EditorContext(&InEditorContext)
    , ViewportId(InViewportId)
    , ProjectionType(InViewportId == 0 ? EProjectionType::Perspective : EProjectionType::Orthographic) {
    CameraRotation.Normalize();
    RenderSettings.bRenderSky = ProjectionType == EProjectionType::Perspective;

    if (ProjectionType == EProjectionType::Orthographic) {
        const EOrthographicView InitialViews[]{
            EOrthographicView::Front,
            EOrthographicView::Top,
            EOrthographicView::Front,
            EOrthographicView::Right
        };
        OrthographicView = InitialViews[std::min<std::size_t>(ViewportId, std::size(InitialViews) - 1)];
        ApplyOrthographicView();
    }

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

    const ImVec2 Position{ static_cast<float>(Rect.Min.X), static_cast<float>(Rect.Min.Y) };
    const ImVec2 Size{ static_cast<float>(Rect.GetWidth()), static_cast<float>(Rect.GetHeight()) };
    ImGui::SetCursorScreenPos(Position);
    ImGui::PushID(static_cast<int>(ViewportId));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool bChildVisible = ImGui::BeginChild("##SceneViewport", Size, ImGuiChildFlags_None,
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    bool bActivated = false;

    if (bChildVisible) {
        bActivated = DrawMenuBar();

        const ImVec2 ImagePosition = ImGui::GetCursorScreenPos();
        const ImVec2 ImageSize = ImGui::GetContentRegionAvail();

        if (ImageSize.x > 0.0f && ImageSize.y > 0.0f) {
            Width = static_cast<uint32>(ImageSize.x);
            Height = static_cast<uint32>(ImageSize.y);
            RenderLeft = ImagePosition.x - MainViewportPosition.x;
            RenderTop = ImagePosition.y - MainViewportPosition.y;
            DisplayRect = {
                { static_cast<int32>(ImagePosition.x), static_cast<int32>(ImagePosition.y) },
                { static_cast<int32>(ImagePosition.x + ImageSize.x), static_cast<int32>(ImagePosition.y + ImageSize.y) }
            };
            bVisible = true;
            ResizeRenderSurface();

            ImGui::InvisibleButton("##SceneSurface", ImageSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

            if (ID3D11ShaderResourceView* ShaderResourceView = RenderSurface.GetShaderResourceView()) {
                ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(ShaderResourceView), ImagePosition, ImVec2(ImagePosition.x + ImageSize.x, ImagePosition.y + ImageSize.y));
            }

            bHovered = !bInputBlocked && ImGui::IsItemHovered();
            bActivated = bActivated || (bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)));
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopID();
    return bActivated;
}

bool FEditorViewport::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
        return false;
    }

    bool bActivated = false;
    ImGui::TextUnformatted("View");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(120.0f, ImGui::GetContentRegionAvail().x));
    const bool bComboOpen = ImGui::BeginCombo("##ViewMode", GetViewModeName());
    bActivated = ImGui::IsItemActivated();
    if (bComboOpen) {
        const bool bPerspectiveSelected = ProjectionType == EProjectionType::Perspective;
        if (ImGui::Selectable("Perspective", bPerspectiveSelected)) {
            SetProjectionType(EProjectionType::Perspective);
            bActivated = true;
        }
        if (bPerspectiveSelected) {
            ImGui::SetItemDefaultFocus();
        }

        constexpr EOrthographicView Views[]{
            EOrthographicView::Front,
            EOrthographicView::Back,
            EOrthographicView::Left,
            EOrthographicView::Right,
            EOrthographicView::Top,
            EOrthographicView::Bottom
        };
        for (EOrthographicView View : Views) {
            const bool bSelected = ProjectionType == EProjectionType::Orthographic && OrthographicView == View;
            if (ImGui::Selectable(GetOrthographicViewName(View), bSelected)) {
                SetOrthographicView(View);
                bActivated = true;
            }
            if (bSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::EndMenuBar();
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
    const bool bBlockKeyboard = ProjectionType == EProjectionType::Orthographic ||
        bInputBlocked || !bVisible || !bFocused || !bHasCamera || bImGuiOwnsKeyboard;
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
    if (Width == 0 || Height == 0) {
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

void FEditorViewport::ApplyMouseNavigation(const FViewportMouseNavigationInput& NavigationInput) {
    if (ProjectionType == EProjectionType::Orthographic) {
        if (NavigationInput.WheelSteps != 0.0f) {
            OrthographicWidth *= std::pow(0.85f, NavigationInput.WheelSteps);
            OrthographicWidth = std::clamp(OrthographicWidth, 0.1f, 10000.0f);
        }

        if ((NavigationInput.DragDeltaX != 0.0f || NavigationInput.DragDeltaY != 0.0f) && Width > 0) {
            const FTransform CameraTransform{ CameraPosition, CameraRotation, FVector3{ 1.0f, 1.0f, 1.0f } };
            const FMatrix CameraMatrix = CameraTransform.ToMatrixNoScale();
            FVector3 CameraForward = CameraMatrix.Forward();
            FVector3 PlaneUp = CameraMatrix.Up();
            CameraForward.Normalize();
            PlaneUp.Normalize();

            FVector3 PlaneRight = PlaneUp.Cross(CameraForward);
            PlaneRight.Normalize();

            const float WorldUnitsPerPixel = OrthographicWidth / static_cast<float>(Width);
            const FVector3 Offset = PlaneRight * (-NavigationInput.DragDeltaX * WorldUnitsPerPixel) + PlaneUp * (-NavigationInput.DragDeltaY * WorldUnitsPerPixel);
            CameraPosition = CameraPosition + Offset;
            OrthographicTarget = OrthographicTarget + Offset;
        }
        return;
    }

    if (NavigationInput.DragDeltaX == 0.0f && NavigationInput.DragDeltaY == 0.0f) {
        return;
    }

    const FEditorSettings Settings = EditorContext != nullptr ? EditorContext->GetEditorSettings() : FEditorSettings{};
    const float RotationSensitivity = Settings.RotationSensitivity * 0.001f;
    constexpr float MaximumPitch = 0.99f;

    FQuat YawDelta = FQuat::CreateFromAxisAngle(FVector3::UnitZ, NavigationInput.DragDeltaX * RotationSensitivity);
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
    float PitchAngle = NavigationInput.DragDeltaY * RotationSensitivity;
    if ((ForwardUp > MaximumPitch && NavigationInput.DragDeltaY > 0.0f) || (ForwardUp < -MaximumPitch && NavigationInput.DragDeltaY < 0.0f)) {
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
    if (ProjectionType == EProjectionType::Orthographic || NavigationInput.DeltaTime <= 0.0f ||
        (NavigationInput.ForwardAxis == 0.0f && NavigationInput.RightAxis == 0.0f && NavigationInput.UpAxis == 0.f)) {
        return;
    }

    const FTransform CameraTransform{ CameraPosition, CameraRotation, FVector3{ 1.0f, 1.0f, 1.0f } };
    const FMatrix CameraWorldMatrix = CameraTransform.ToMatrixNoScale();
    FVector3 MoveDirection = 
        CameraWorldMatrix.Forward() * NavigationInput.ForwardAxis 
        - CameraWorldMatrix.Right() * NavigationInput.RightAxis 
        + CameraWorldMatrix.Up() * NavigationInput.UpAxis;

    if (MoveDirection.LengthSquared() <= 0.0f) {
        return;
    }

    MoveDirection.Normalize();
    const FEditorSettings Settings = EditorContext != nullptr ? EditorContext->GetEditorSettings() : FEditorSettings{};
    const float MoveSensitivity = Settings.MoveSensitivity;
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

const FVector3& FEditorViewport::GetCameraPosition() const {
    return CameraPosition;
}

void FEditorViewport::SetProjectionType(EProjectionType InProjectionType) {
    if (ProjectionType == InProjectionType) {
        return;
    }

    if (InProjectionType == EProjectionType::Perspective) {
        ProjectionType = EProjectionType::Perspective;
        CameraPosition = PerspectiveCameraPosition;
        CameraRotation = PerspectiveCameraRotation;
    } else {
        PerspectiveCameraPosition = CameraPosition;
        PerspectiveCameraRotation = CameraRotation;
        ProjectionType = EProjectionType::Orthographic;
        ApplyOrthographicView();
    }
    RenderSettings.bRenderSky = ProjectionType == EProjectionType::Perspective;
}

void FEditorViewport::SetOrthographicView(EOrthographicView InView) {
    if (ProjectionType == EProjectionType::Perspective) {
        PerspectiveCameraPosition = CameraPosition;
        PerspectiveCameraRotation = CameraRotation;
    }

    ProjectionType = EProjectionType::Orthographic;
    OrthographicView = InView;
    ApplyOrthographicView();
    RenderSettings.bRenderSky = false;
}

void FEditorViewport::SetCameraParameter(const FVector3& InPosition, const FQuat& InRotation, float InFieldOfView, float InOrthographicWidth, float InNearPlane, float InFarPlane) {
    CameraPosition = InPosition;
    CameraRotation = InRotation;
    CameraRotation.Normalize();
    FieldOfView = InFieldOfView;
    OrthographicWidth = InOrthographicWidth;
    NearPlane = InNearPlane;
    FarPlane = InFarPlane;

    if (ProjectionType == EProjectionType::Perspective) {
        PerspectiveCameraPosition = CameraPosition;
        PerspectiveCameraRotation = CameraRotation;
    } else {
        const FTransform CameraTransform{ CameraPosition, CameraRotation, FVector3{ 1.0f, 1.0f, 1.0f } };
        FVector3 CameraForward = CameraTransform.ToMatrixNoScale().Forward();
        CameraForward.Normalize();
        const float CameraDistance = (NearPlane + FarPlane) * 0.5f;
        OrthographicTarget = CameraPosition + CameraForward * CameraDistance;
    }
}

void FEditorViewport::ApplyOrthographicView() {
    CameraRotation = GetOrthographicRotation(OrthographicView);
    CameraRotation.Normalize();

    const FTransform RotationTransform{ FVector3{}, CameraRotation, FVector3{ 1.0f, 1.0f, 1.0f } };
    FVector3 CameraForward = RotationTransform.ToMatrixNoScale().Forward();
    CameraForward.Normalize();
    const float CameraDistance = (NearPlane + FarPlane) * 0.5f;
    CameraPosition = OrthographicTarget - CameraForward * CameraDistance;
}

const char* FEditorViewport::GetViewModeName() const {
    return ProjectionType == EProjectionType::Perspective ? "Perspective" : GetOrthographicViewName(OrthographicView);
}

void FEditorViewport::ReleaseRenderResources() {
    RenderSurface.Reset();
}

D3D11_VIEWPORT FEditorViewport::BuildInputViewport() const {
    return D3D11_VIEWPORT{ RenderLeft, RenderTop, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
}
