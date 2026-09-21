#pragma once

#include <d3d11.h>

#include "../../Core/Asset/FAssetRegistry.h"
#include "../../Core/Base/FRenderProbe.h"
#include "../../Core/Channel/FStateChannel.h"
#include "../../Scene/FWorldEditorContext.h"
#include "../../FMouseInput.h"

#include "ILineRenderer.h"
#include "FLineRenderer.h"
#include "FBatchLineRender.h"
#include "FTransformGizmo.h"

class EditorViewport {
	constexpr static float OrientationAxisSize = 200.0f;

public:
	EditorViewport() = default;
	~EditorViewport() = default;

	EditorViewport(const EditorViewport&) = delete;
	EditorViewport& operator=(const EditorViewport&) = delete;

	EditorViewport(EditorViewport&&) noexcept = default;
	EditorViewport& operator=(EditorViewport&&) noexcept = default;

public:
	void Initialize(ID3D11Device* Device, FAssetRegistry& AssetRegistry, FWorldEditorContext& InEditorContext);

	void PrepareInput(const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport);
	void ProcessInput(FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, bool bMouseCapturedByUI);
	void RenderInProbe(FRenderProbe& Probe, const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport);

	void RenderSceneGuides(ID3D11DeviceContext* Context, const CameraProbe& Camera, const FVector3& CameraPosition, const D3D11_VIEWPORT& Viewport);
	void RenderOrientationAxis(ID3D11DeviceContext* Context, const CameraProbe& Probe);

	FStateChannel<uint8>::FReadWriter GetGizmoMode() { return TransformGizmo.GetGizmoMode(); }
	FStateChannel<uint8>::FReadWriter GetGizmoCoordinateSpace() { return TransformGizmo.GetGizmoCoordinateSpace(); }
private:
	void RenderGrid(const FVector3& CameraPosition, ELineDepthMode DepthMode);
	void RenderAxis(ELineDepthMode DepthMode);
	void RenderBounds(ELineDepthMode DepthMode);

private:
	std::unique_ptr<ILineRenderer> LineRenderer = std::make_unique<FLineRenderer>();
	FTransformGizmo TransformGizmo{};

	D3D11_VIEWPORT OrientationAxisViewport{ 5.0f, 5.0f, OrientationAxisSize, OrientationAxisSize, 0.0f, 1.0f };

	FWorldEditorContext* EditorContext = nullptr;
};
