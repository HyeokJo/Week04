#pragma once

#include <d3d11.h>
#include <memory>

#include "../Core/Asset/FAssetRegistry.h"
#include "../Core/Base/FRenderProbe.h"

#include "../FSceneRenderSurface.h"

class FRenderer;
class UMesh;

class FAssetThumbnailRenderer {
	struct FThumbnail {
		std::unique_ptr<FSceneRenderSurface> Surface{};
	};

public:
	FAssetThumbnailRenderer() = default;
	~FAssetThumbnailRenderer() = default;

	FAssetThumbnailRenderer(const FAssetThumbnailRenderer&) = delete;
	FAssetThumbnailRenderer& operator=(const FAssetThumbnailRenderer&) = delete;

	FAssetThumbnailRenderer(FAssetThumbnailRenderer&&) = delete;
	FAssetThumbnailRenderer& operator=(FAssetThumbnailRenderer&&) = delete;

public:
	void Create(FRenderer* InRenderer, FAssetRegistry* InAssetRegistry);
	void RenderThumbnail(FAssetHandle AssetHandle);

	ID3D11ShaderResourceView* GetThumbnail(FAssetHandle AssetHandle) const;

	void Terminate();

private:
	void RenderThumbnail(const FAssetEntry& Entry);

	FMatrix BuildMeshTransform(const UMesh& Mesh) const;
	CameraProbe BuildCamera() const;
	FMatrix MakeCameraWorldMatrix(const FVector& Eye, const FVector& Target) const;

	const FAssetEntry* FindAssetEntry(FAssetHandle AssetHandle) const;

	static uint64 MakeThumbnailKey(FAssetHandle AssetHandle);

private:
	FRenderer* Renderer{ nullptr };
	FAssetRegistry* AssetRegistry{ nullptr };

	TMap<uint64, FThumbnail> Thumbnails{};
};