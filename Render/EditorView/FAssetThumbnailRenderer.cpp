#include "PCH.h"

#include "FAssetThumbnailRenderer.h"
#include "../Renderer.h"

#include "../Core/Asset/UMaterial.h"
#include "../Core/Asset/UMesh.h"

#include <algorithm>

namespace {
	constexpr uint32 ThumbnailSize = 256;

	constexpr const char* DefaultMaterialPath = "/Game/System/Material/Default.mtl";
	constexpr const char* SphereMeshPath = "/Game/System/Mesh/Sphere.bin";
	constexpr const char* StaticMeshPipelinePath = "/Game/Pipeline/Base";
	constexpr const char* MaterialPipelinePath = "/Game/Pipeline/TexturedTest.json";
}

void FAssetThumbnailRenderer::Create(FRenderer* InRenderer, FAssetRegistry* InAssetRegistry) {
	Renderer = InRenderer;
	AssetRegistry = InAssetRegistry;

	if (Renderer == nullptr || AssetRegistry == nullptr) {
		return;
	}

	for (const FAssetEntry& Entry : AssetRegistry->GetAssetEntries()) {
		if (Entry.Asset == nullptr) {
			continue;
		}

		if (Entry.AssetType != EAssetType::Mesh && Entry.AssetType != EAssetType::Material) {
			continue;
		}

		RenderThumbnail(Entry);
	}
}

void FAssetThumbnailRenderer::RenderThumbnail(FAssetHandle AssetHandle) {
	const FAssetEntry* Entry = FindAssetEntry(AssetHandle);

	if (Entry == nullptr) {
		return;
	}

	RenderThumbnail(*Entry);
}

void FAssetThumbnailRenderer::RenderThumbnail(const FAssetEntry& Entry) {
	if (Renderer == nullptr || AssetRegistry == nullptr || Entry.Asset == nullptr) {
		return;
	}

	FActorProbe ActorProbe{};

	if (Entry.AssetType == EAssetType::Mesh) {
		ActorProbe.MeshHandle = Entry.Handle;
		ActorProbe.MaterialHandle = AssetRegistry->FindAsset(FAssetPath{DefaultMaterialPath});
		ActorProbe.PipelineHandle = AssetRegistry->FindAsset(FAssetPath{StaticMeshPipelinePath});
	}
	else if (Entry.AssetType == EAssetType::Material) {
		ActorProbe.MeshHandle = AssetRegistry->FindAsset(FAssetPath{SphereMeshPath});
		ActorProbe.MaterialHandle = Entry.Handle;
		ActorProbe.PipelineHandle = AssetRegistry->FindAsset(FAssetPath{MaterialPipelinePath});
	}
	else {
		return;
	}

	UMesh* Mesh = AssetRegistry->ResolveAsset<UMesh>(ActorProbe.MeshHandle);

	UMaterial* Material = AssetRegistry->ResolveAsset<UMaterial>(ActorProbe.MaterialHandle);

	if (Mesh == nullptr || Material == nullptr || !ActorProbe.PipelineHandle) {
		return;
	}

	ActorProbe.World = BuildMeshTransform(*Mesh);

	FRenderProbe Probe{};
	Probe.ActorProbes.push_back(ActorProbe);

	FLightProbe LightProbe{};
	LightProbe.Type = ELightType::Directional;
	LightProbe.Direction = FVector{ -0.5f, -0.5f, -1.0f };
	LightProbe.Color = FVector{ 1.0f, 1.0f, 1.0f };
	LightProbe.Intensity = 1.0f;

	Probe.LightProbes.push_back(LightProbe);

	const uint64 ThumbnailKey = MakeThumbnailKey(Entry.Handle);
	FThumbnail& Thumbnail = Thumbnails[ThumbnailKey];

	if (Thumbnail.Surface == nullptr) {
		Thumbnail.Surface = std::make_unique<FSceneRenderSurface>();
	}

	Thumbnail.Surface->InitializeOffscreen(Renderer->GetDevice(), ThumbnailSize, ThumbnailSize);

	if (!Thumbnail.Surface->IsValid()) {
		Thumbnail.Surface.reset();
		return;
	}

	FRenderSettings RenderSettings{};
	RenderSettings.ClearColor = FVector4{
		0.075f,
		0.080f,
		0.095f,
		1.0f
	};
	RenderSettings.bRenderSky = false;

	Renderer->RenderScene( *Thumbnail.Surface, Probe, BuildCamera(), RenderSettings);
}

ID3D11ShaderResourceView* FAssetThumbnailRenderer::GetThumbnail(FAssetHandle AssetHandle) const {
	const auto It = Thumbnails.find(MakeThumbnailKey(AssetHandle));

	if (It == Thumbnails.end() || It->second.Surface == nullptr) {
		return nullptr;
	}

	return It->second.Surface->GetShaderResourceView();
}

void FAssetThumbnailRenderer::Terminate() {
	for (auto& [Key, Thumbnail] : Thumbnails) {
		if (Thumbnail.Surface != nullptr) {
			Thumbnail.Surface->Reset();
		}
	}

	Thumbnails.clear();

	Renderer = nullptr;
	AssetRegistry = nullptr;
}

FMatrix FAssetThumbnailRenderer::BuildMeshTransform(const UMesh& Mesh) const {
	const std::span<const FVector> Positions = Mesh.GetVertexAttributeData<EVertexAttribute::Position>();

	if (Positions.empty()) {
		return FMatrix::Identity;
	}

	FVector BoundsMin = Positions.front();
	FVector BoundsMax = Positions.front();

	for (const FVector& Position : Positions) {
		BoundsMin = FVector::Min(BoundsMin, Position);
		BoundsMax = FVector::Max(BoundsMax, Position);
	}

	const FVector Center = (BoundsMin + BoundsMax) * 0.5f;
	const FVector Size = BoundsMax - BoundsMin;

	const float LargestExtent = std::max({
		Size.x,
		Size.y,
		Size.z
		});

	if (LargestExtent <= 0.0001f) {
		return FMatrix::Identity;
	}

	const float UniformScale = 1.6f / LargestExtent;

	return FMatrix::CreateTranslation(-Center) * FMatrix::CreateScale( UniformScale, UniformScale, UniformScale);
}

CameraProbe FAssetThumbnailRenderer::BuildCamera() const {
	const FVector Target{ 0.0f, 0.0f, 0.0f };
	const FVector Eye{ 3.0f, -3.0f, 2.25f };

	CameraProbe Camera{};
	Camera.View = MakeCameraWorldMatrix(Eye, Target).Invert();
	Camera.Projection = FMatrix::CreatePerspectiveFieldOfView(
		0.610865f,
		1.0f,
		0.1f,
		100.0f);

	Camera.ViewProjection = Camera.View * Camera.Projection;

	return Camera;
}

FMatrix FAssetThumbnailRenderer::MakeCameraWorldMatrix(const FVector& Eye, const FVector& Target) const {
	FVector Forward = Target - Eye;
	Forward.Normalize();

	FVector Right = FVector::UnitZ.Cross(Forward);
	Right.Normalize();

	FVector Up = Forward.Cross(Right);
	Up.Normalize();

	FMatrix Result = FMatrix::Identity;

	Result.m[0][0] = Right.x;
	Result.m[0][1] = Right.y;
	Result.m[0][2] = Right.z;

	Result.m[1][0] = Up.x;
	Result.m[1][1] = Up.y;
	Result.m[1][2] = Up.z;

	Result.m[2][0] = Forward.x;
	Result.m[2][1] = Forward.y;
	Result.m[2][2] = Forward.z;

	Result.m[3][0] = Eye.x;
	Result.m[3][1] = Eye.y;
	Result.m[3][2] = Eye.z;

	return Result;
}

const FAssetEntry* FAssetThumbnailRenderer::FindAssetEntry(FAssetHandle AssetHandle) const {
	if (AssetRegistry == nullptr) {
		return nullptr;
	}

	for (const FAssetEntry& Entry : AssetRegistry->GetAssetEntries()) {
		if (Entry.Handle == AssetHandle) {
			return &Entry;
		}
	}

	return nullptr;
}

uint64 FAssetThumbnailRenderer::MakeThumbnailKey(FAssetHandle AssetHandle) {
	return (static_cast<uint64>(AssetHandle.Generation) << 32) | static_cast<uint64>(AssetHandle.ID);
}