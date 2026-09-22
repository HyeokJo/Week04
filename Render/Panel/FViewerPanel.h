#pragma once

#include "PCH.h"
#include "FEditorWindow.h"
#include "FPropertyEditorContext.h"
#include "Render/FSceneRenderSurface.h"
#include "Core/Base/FRenderProbe.h"
#include "Core/Channel/FMessageChannel.h"
#include "Core/Asset/FAssetHandle.h"
#include "Render/EditorView/FLineRenderer.h"

class FWorldEditorContext;
class FRenderer;
class FAssetRegistry;
class FAssetThumbnailRenderer;

// 메인 뷰포트와 무관한 독립 공간에 모델 하나만 그려 보여주는 패널.
class FViewerPanel final : public FEditorWindow {
public:
	FViewerPanel(FAssetRegistry& InRegistry, HWND InputWindowHandle, FMessageChannel::FSender InEditorToWorldSender, FWorldEditorContext& InEditorContext, FAssetThumbnailRenderer* InThumbnailRenderer);
	~FViewerPanel() override = default;

	FViewerPanel(const FViewerPanel&) = delete;
	FViewerPanel& operator=(const FViewerPanel&) = delete;
	FViewerPanel(FViewerPanel&&) = delete;
	FViewerPanel& operator=(FViewerPanel&&) = delete;

public:
	void RenderOffscreen(FRenderer& InRenderer, FAssetRegistry& InRegistry) override;
	void ReleaseRenderResources() override;
	void SetMesh(FAssetHandle InMeshHandle);
	void SetMaterial(FAssetHandle InMaterialHandle);

	// 보여줄 메시를 바꾼다. 무효 핸들이면 CubeMesh 로 대체된다.
private:
	void DrawContents() override;
	void DrawMenuBar();
	void DrawProperties();
	void DrawPreview();
	void ResizeSurfaceIfNeeded(ID3D11Device* Device, uint32 Width, uint32 Height);
	FRenderProbe BuildPreviewProbe();
	CameraProbe BuildPreviewCamera() const;
	void ProcessInput();
	// 미리보기 서피스 좌상단에 월드 기저를 표시한다.
	void RenderOrientationAxis(ID3D11DeviceContext* Context);
	// 카메라의 월드 행렬을 직접 만든다. FTransform 을 거치지 않으므로
	// 메시 소스 기저 변환에 영향받지 않는다.
	FMatrix MakeCameraWorldMatrix(const FVector3& Eye) const;
	FString OpenFileDialog(const FString& FilePath, const OPENFILENAMEA& OFN) const;

private:
	FAssetRegistry* mRegistry{ nullptr };
	FMessageChannel::FSender mEditorToWorldSender;
	HWND mWindowHandle{ nullptr };
	FWorldEditorContext& mEditorContext;
	FPropertyEditorContext mPropertyEditor{};
	FSceneRenderSurface mSurface{};
	FAssetHandle mMeshHandle{};
	FAssetHandle mMaterialHandle{};
	std::unique_ptr<ILineRenderer> mLineRenderer{ std::make_unique<FLineRenderer>() };
	bool mLineRendererInitialized{ false };
	uint32 mSurfaceWidth{};
	uint32 mSurfaceHeight{};
	// DrawPanel 이 잰 크기. 다음 RenderOffscreen 이 이 값으로 서피스를 맞춘다
	uint32 mDesiredWidth{};
	uint32 mDesiredHeight{};
	float mDistance{ 5.0f };
	FVector3 mTarget{ 0.0f, 0.0f, 0.0f };
	float mFieldOfView{ 1.0472f }; // 60도
	FQuat mOrbitRotation{};
};
