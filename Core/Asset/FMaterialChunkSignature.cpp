#include "PCH.h"
#include "FMaterialChunkSignature.h"

bool FMaterialChunkSignature::IsValid() const {
	return TextureFieldCount <= MAX_MATERIAL_TEXTURE_FIELDS;
}

FAssetHandle FMaterialChunkSignature::GetTextureHandle(uint8 TextureFieldIndex) const {
	if (TextureFieldIndex >= TextureFieldCount) {
		return {};
	}

	return TextureHandles[TextureFieldIndex];
}

size_t FMaterialChunkSignature::GetHash() const noexcept {
	size_t Hash{ std::hash<uint8>{}(TextureFieldCount) };

	for (uint8 TextureFieldIndex{}; TextureFieldIndex < TextureFieldCount; ++TextureFieldIndex) {
		const FAssetHandle Handle{ TextureHandles[TextureFieldIndex] };
		Hash ^= std::hash<uint32>{}(Handle.ID) + static_cast<size_t>(0x9e3779b9u) + (Hash << 6) + (Hash >> 2);
		Hash ^= std::hash<uint32>{}(Handle.Generation) + static_cast<size_t>(0x9e3779b9u) + (Hash << 6) + (Hash >> 2);
	}

	return Hash;
}

bool FMaterialChunkSignatureBuilder::AddTexture(FAssetHandle TextureHandle) {
	if (mSignature.TextureFieldCount >= MAX_MATERIAL_TEXTURE_FIELDS) {
		return false;
	}

	mSignature.TextureHandles[mSignature.TextureFieldCount] = TextureHandle;
	++mSignature.TextureFieldCount;

	return true;
}

FMaterialChunkSignature FMaterialChunkSignatureBuilder::Build() const {
	return mSignature;
}

void FMaterialChunkSignatureBuilder::Reset() {
	mSignature = {};
}

size_t std::hash<FMaterialChunkSignature>::operator()(const FMaterialChunkSignature& Signature) const noexcept {
	return Signature.GetHash();
}
