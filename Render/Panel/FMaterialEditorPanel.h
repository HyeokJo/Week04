#pragma once

#include "FEditorWindow.h"
#include "FPropertyEditorContext.h"
#include "Render/FSceneRenderSurface.h"
#include "Core/Asset/FAssetHandle.h"
#include "Core/Asset/FMaterialGroup.h"
#include <functional>

class FAssetRegistry;
class FAssetThumbnailRenderer;
class FRenderer;
class USurfaceOpaque;

class FMaterialEditorPanel final : public FEditorWindow {
public:
	FMaterialEditorPanel(FAssetRegistry& InRegistry, FAssetThumbnailRenderer& InThumbnailRenderer);
	~FMaterialEditorPanel() override;

	FMaterialEditorPanel(const FMaterialEditorPanel&) = delete;
	FMaterialEditorPanel& operator=(const FMaterialEditorPanel&) = delete;
	FMaterialEditorPanel(FMaterialEditorPanel&&) = delete;
	FMaterialEditorPanel& operator=(FMaterialEditorPanel&&) = delete;

public:
	void OpenMaterial(FAssetHandle MaterialHandle);
	void RenderOffscreen(FRenderer& Renderer, FAssetRegistry& Registry) override;
	void ReleaseRenderResources() override;

private:
	void DrawContents() override;
	void DrawGroup(USurfaceOpaque& Material, uint32 GroupIndex, const FMaterialGroup& Group);
	void DrawTexture(USurfaceOpaque& Material, uint32 GroupIndex, const char* Label, FMaterialTextureMap FMaterialGroup::* Member);
	void ModifyGroup(USurfaceOpaque& Material, uint32 GroupIndex, const std::function<void(FMaterialGroup&)>& Modifier);

private:
	FAssetRegistry& mRegistry;
	FAssetThumbnailRenderer& mThumbnailRenderer;
	FPropertyEditorContext mPropertyEditor{};
	FSceneRenderSurface mPreviewSurface{};
	FAssetHandle mMaterialHandle{};
	bool mPreviewDirty{ false };
};
