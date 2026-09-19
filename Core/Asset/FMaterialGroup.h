#pragma once

#include "FAssetHandle.h"

struct FMaterialGroup {
	FString Name{};

	FVector3 Ambient{ 0.0f, 0.0f, 0.0f };
	FVector3 Diffuse{ 1.0f, 1.0f, 1.0f };
	FVector3 Specular{ 0.0f, 0.0f, 0.0f };
	FVector3 Emissive{ 0.0f, 0.0f, 0.0f };

	float Shininess{ 0.0f };
	float Opacity{ 1.0f };

	FAssetHandle DiffuseTexture{};
	FAssetHandle NormalTexture{};
};
