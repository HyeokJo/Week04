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
	bool HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition);

private:
	void DrawContents() override;
	void DrawMenuBar();
	void DrawProperties();
	void DrawPreview();
	void ResizeSurfaceIfNeeded(ID3D11Device* Device, uint32 Width, uint32 Height);
	FRenderProbe BuildPreviewProbe();
	CameraProbe BuildPreviewCamera() const;
	void ProcessInput();
	void RenderOrientationAxis(ID3D11DeviceContext* Context);
	FMatrix MakeCameraWorldMatrix(const FVector3& Eye) const;
	FString OpenFileDialog(const FString& FilePath, const OPENFILENAMEA& OFN) const;
	bool OpenViewerFile(const std::filesystem::path& FilePath);

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
	uint32 mDesiredWidth{};
	uint32 mDesiredHeight{};
	float mDistance{ 5.0f };
	FVector3 mTarget{ 0.0f, 0.0f, 0.0f };
	float mFieldOfView{ 1.0472f };
	FQuat mOrbitRotation{};
	ImVec2 mDropTargetMin{};
	ImVec2 mDropTargetMax{};
};
