#include "PCH.h"
#include "UMaterial.h"

void UMaterial::BuildGPUData(uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const {
	if (GroupIndex != 0) {
		OutSlot = {};
		return;
	}

	BuildGPUData(OutSlot);
}

FMaterialChunkSignature UMaterial::BuildChunkSignature() const {
	return {};
}

FMaterialChunkSignature UMaterial::BuildChunkSignature(uint32 GroupIndex) const {
	return GroupIndex == 0 ? BuildChunkSignature() : FMaterialChunkSignature{};
}

void UMaterial::Finalize(IAssetQuery* Query) {
}

std::optional<uint32> UMaterial::FindGroupIndex(const FString& Name) const {
	return std::nullopt;
}

void UMaterial::Serialize(FArchive& Ar) {
	UAsset::Serialize(Ar);
}
