#pragma once

#include "FEditorViewportTypes.h"
#include "FViewportGeometry.h"
#include "Core/Base/FRenderProbe.h"
#include "Render/FSceneRenderSurface.h"

class EditorViewport;
class FKeyboardInput;
class FMouseInput;
class FWorldEditorContext;
struct FViewportKeyboardNavigationInput;
struct FViewportMouseNavigationInput;
struct ImVec2;

class FEditorViewport {
public:
    FEditorViewport(FViewportId InViewportId, ID3D11Device* InDevice, FWorldEditorContext& InEditorContext);

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
    bool BuildCameraProbe(CameraProbe& OutCamera);
    FSceneRenderSurface& GetRenderSurface();
    const D3D11_VIEWPORT& GetRenderViewport() const;
    const FRenderSettings& GetRenderSettings() const;
    const FVector3& GetCameraPosition() const;
    void ReleaseRenderResources();

private:
    enum class EProjectionType : uint8 {
        Perspective,
        Orthographic
    };

    void ResizeRenderSurface();
    void ApplyMouseNavigation(const FViewportMouseNavigationInput& NavigationInput);
    void ApplyKeyboardNavigation(const FViewportKeyboardNavigationInput& NavigationInput);
    D3D11_VIEWPORT BuildInputViewport() const;

    ID3D11Device* Device = nullptr;
    FWorldEditorContext* EditorContext = nullptr;
    FSceneRenderSurface RenderSurface;
    FRenderSettings RenderSettings;
    FViewportId ViewportId = 0;
    EProjectionType ProjectionType = EProjectionType::Perspective;
    FVector3 CameraPosition{ -13.567042f, -26.165287f, 31.506821f };
    FQuat CameraRotation{ -0.205543f, 0.040122f, -0.187332f, 0.959713f };
    float FieldOfView = 1.0472f;
    float OrthographicWidth = 50.0f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    FRect DisplayRect{};
    uint32 Width = 0;
    uint32 Height = 0;
    float RenderLeft = 0.0f;
    float RenderTop = 0.0f;
    bool bVisible = false;
    bool bHovered = false;
    bool bFocused = false;
};
