#pragma once

#include "FAssetHandle.h"

struct FMaterialTextureMap {
	FString SourcePath{};
	FAssetHandle Texture{};
};

struct FMaterialGroup {
	FString Name{};

	FVector3 Ambient{ 0.0f, 0.0f, 0.0f };
	FVector3 Diffuse{ 1.0f, 1.0f, 1.0f };
	FVector3 Specular{ 0.0f, 0.0f, 0.0f };
	FVector3 Emissive{ 0.0f, 0.0f, 0.0f };
	FVector3 TransmissionFilter{ 1.0f, 1.0f, 1.0f };

	float Shininess{ 0.0f };
	float RefractionIndex{ 1.0f };
	float Opacity{ 1.0f };
	float Sharpness{ 60.0f };
	int32 IlluminationModel{ 2 };
	bool bDissolveHalo{ false };

	FMaterialTextureMap AmbientTexture{};
	FMaterialTextureMap DiffuseTexture{};
	FMaterialTextureMap SpecularTexture{};
	FMaterialTextureMap EmissiveTexture{};
	FMaterialTextureMap TransmissionTexture{};
	FMaterialTextureMap ShininessTexture{};
	FMaterialTextureMap OpacityTexture{};
	FMaterialTextureMap BumpTexture{};
	FMaterialTextureMap NormalTexture{};
	FMaterialTextureMap DisplacementTexture{};
	FMaterialTextureMap DecalTexture{};
	FMaterialTextureMap ReflectionTexture{};
};
