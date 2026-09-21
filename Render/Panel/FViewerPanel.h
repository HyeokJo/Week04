#pragma once

#include "PCH.h"
#include "FEditorWindow.h"
#include "Render/FSceneRenderSurface.h"
#include "Core/Base/FRenderProbe.h"
#include "Core/Channel/FMessageChannel.h"

class FRenderer;
class FAssetRegistry;
class FQuat;


// 메인 뷰포트와 무관한 독립 공간에 모델 하나만 그려 보여주는 패널.
class FViewerPanel final : public FEditorWindow {
public:
	explicit FViewerPanel(FAssetRegistry& InRegistry, HWND InputWindowHandle, FMessageChannel::FSender InEditorToWorldSender)
		: FEditorWindow("Viewer", ImGuiWindowFlags_MenuBar)
		, Registry(&InRegistry)
		, EditorToWorldSender(InEditorToWorldSender)
		, WindowHandle(InputWindowHandle) {
	}

	~FViewerPanel() override = default;

	FViewerPanel(const FViewerPanel&) = delete;
	FViewerPanel& operator=(const FViewerPanel&) = delete;

	FViewerPanel(FViewerPanel&&) = delete;
	FViewerPanel& operator=(FViewerPanel&&) = delete;

public:
	void RenderOffscreen(FRenderer& InRenderer, FAssetRegistry& InRegistry) override;
	void ReleaseRenderResources() override;

	// 보여줄 메시를 바꾼다. 무효 핸들이면 CubeMesh 로 대체된다.
	void SetMesh(const FAssetHandle& InMeshHandle) { MeshHandle = InMeshHandle; }

private:
	void DrawContents() override;
	void DrawMenuBar();
	void DrawToolBar();
	void DrawPreview();

	void ResizeSurfaceIfNeeded(ID3D11Device* Device, uint32 Width, uint32 Height);
	FRenderProbe BuildPreviewProbe() const;
	CameraProbe BuildPreviewCamera() const;
	void ProcessInput();

	// 카메라의 월드 행렬을 직접 만든다. FTransform 을 거치지 않으므로
	// 메시 소스 기저 변환에 영향받지 않는다.
	FMatrix MakeCameraWorldMatrix(const FVector3& Eye) const;

private:
	FString OpenFileDialog(const FString& FilePath, const OPENFILENAMEA& OFN);
	FAssetRegistry* Registry = nullptr;
	FMessageChannel::FSender EditorToWorldSender;

	HWND WindowHandle;
	FSceneRenderSurface Surface;
	FAssetHandle MeshHandle;

	uint32 SurfaceWidth = 0;
	uint32 SurfaceHeight = 0;

	// DrawPanel 이 잰 크기. 다음 RenderOffscreen 이 이 값으로 서피스를 맞춘다
	uint32 DesiredWidth = 0;
	uint32 DesiredHeight = 0;

	float Yaw = 0.8f;
	float Pitch = 0.5f;
	float Distance = 5.0f;
	FVector3 Target{ 0.0f, 0.0f, 0.0f };

	float FieldOfView = 1.0472f;   // 60도
	FQuat OrbitRotation{};
};
