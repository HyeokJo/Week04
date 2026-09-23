#pragma once

#include "FAssetHandle.h"

#include <array>
#include <functional>

inline constexpr uint8 MAX_MATERIAL_TEXTURE_FIELDS = 12;

struct FMaterialChunkSignature {
	std::array<FAssetHandle, MAX_MATERIAL_TEXTURE_FIELDS> TextureHandles{};
	uint8 TextureFieldCount{ 0 };

	bool IsValid() const;
	FAssetHandle GetTextureHandle(uint8 TextureFieldIndex) const;
	size_t GetHash() const noexcept;

	bool operator==(const FMaterialChunkSignature& Other) const = default;
	bool operator!=(const FMaterialChunkSignature& Other) const = default;
};

class FMaterialChunkSignatureBuilder {
public:
	bool AddTexture(FAssetHandle TextureHandle);

	FMaterialChunkSignature Build() const;
	void Reset();

private:
	FMaterialChunkSignature mSignature{};
};

namespace std {
	template<>
	struct hash<FMaterialChunkSignature> {
		size_t operator()(const FMaterialChunkSignature& Signature) const noexcept;
	};
}
