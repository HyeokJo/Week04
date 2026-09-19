#include "PCH.h"
#include "UTexturedMaterial.h"

#include "UTexture.h"
#include "FAssetMetadataParser.h"
#include "../../ErrorHandler.h"

void UTexturedMaterial::Initialize(ID3D11Device* Device, const std::filesystem::path& metaData) {
	UMaterial::Initialize(Device, metaData);

	FAssetMetadataParser MetadataParser{};

	ErrorHandler::Report(not MetadataParser.Load(AssetMetaDataPath), " [ UTexturedMaterial ]", "Failed to load metadata", ErrorHandler::EErrorLevel::Critical);

	MetadataParser.TryGet("TextureName", TextureName);
	
}

FMaterialChunkSignature UTexturedMaterial::BuildChunkSignature() const {
	FMaterialChunkSignatureBuilder Builder{};
	Builder.AddTexture(TextureHandle);
	return Builder.Build();
}

void UTexturedMaterial::Finalize(IAssetQuery* Query) {
	TextureHandle = Query->FindAsset(FAssetPath{ TextureName });
	ErrorHandler::Report(!TextureHandle, "[ UTexturedMaterial ]", "Failed to resolve texture asset: " + TextureName, ErrorHandler::EErrorLevel::Critical);
}

void UTexturedMaterial::Serialize(FArchive& Ar) {
	UMaterial::Serialize(Ar);
}
