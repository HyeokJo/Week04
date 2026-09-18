#include "PCH.h"
#include "UTexture.h"

#include "FAssetMetadataParser.h"
#include "../../ErrorHandler.h"

#include <DirectXTex.h>

namespace {
	DXGI_FORMAT GetDXGIFormat(ETextureFormat textureFormat) {
		switch (textureFormat) {
		case ETextureFormat::UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case ETextureFormat::SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}
}

void UTexture::Initialize(ID3D11Device* device, const std::filesystem::path& metaData) {
	Initialize(device, metaData, ETextureFormat::UNORM, ETextureExtension::DDS);
}

void UTexture::Initialize(ID3D11Device* device, const std::filesystem::path& metaData, ETextureFormat textureFormat, ETextureExtension textureExtension) {
	UAsset::Initialize(device, metaData);

	FAssetMetadataParser Parser;
	ErrorHandler::Report(!Parser.Load(metaData), "[ UTexture ]", "Failed to load metadata: " + metaData.string(), ErrorHandler::EErrorLevel::Critical);

	const std::filesystem::path path = Parser.ResolvePath("FilePath");
	if (device == nullptr) {
		ErrorHandler::Report("[ UTexture ]", "Texture device is null: " + path.string(), ErrorHandler::EErrorLevel::Critical);
		return;
	}

	const DXGI_FORMAT TargetFormat = GetDXGIFormat(textureFormat);

	if (TargetFormat == DXGI_FORMAT_UNKNOWN) {
		ErrorHandler::Report("[ UTexture ]", "Unsupported texture format setting: " + path.string(), ErrorHandler::EErrorLevel::Critical);
		return;
	}

	DirectX::ScratchImage SourceImage{};
	DirectX::ScratchImage ConvertedImage{};
	DirectX::ScratchImage GeneratedMipChain{};
	DirectX::ScratchImage DDSImage{};
	DirectX::TexMetadata SourceImageMetaData{};
	HRESULT Result = S_OK;

	if (textureExtension == ETextureExtension::DDS) {
		Result = DirectX::LoadFromDDSFile(path.wstring().c_str(), DirectX::DDS_FLAGS_NONE, &SourceImageMetaData, SourceImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load DDS texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	}
	else if (textureExtension == ETextureExtension::TGA) {
		Result = DirectX::LoadFromTGAFile(path.wstring().c_str(), DirectX::TGA_FLAGS_NONE, &SourceImageMetaData, SourceImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load TGA texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	}
	else if (textureExtension == ETextureExtension::BMP || textureExtension == ETextureExtension::PNG || textureExtension == ETextureExtension::GIF || textureExtension == ETextureExtension::TIF || textureExtension == ETextureExtension::TIFF || textureExtension == ETextureExtension::JPG || textureExtension == ETextureExtension::JPEG) {
		Result = DirectX::LoadFromWICFile(path.wstring().c_str(), DirectX::WIC_FLAGS_NONE, &SourceImageMetaData, SourceImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load WIC texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	}
	else if (textureExtension == ETextureExtension::HDR) {
		Result = DirectX::LoadFromHDRFile(path.wstring().c_str(), &SourceImageMetaData, SourceImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load HDR texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	}
	else {
		ErrorHandler::Report("[ UTexture ]", "Unsupported texture extension setting: " + path.string(), ErrorHandler::EErrorLevel::Critical);
		return;
	}

	const DirectX::Image* Images = SourceImage.GetImages();
	size_t ImageCount = SourceImage.GetImageCount();
	const DirectX::TexMetadata* ImageMetaData = &SourceImageMetaData;

	if (textureExtension != ETextureExtension::DDS) {
		Result = DirectX::Convert(Images, ImageCount, *ImageMetaData, TargetFormat, DirectX::TEX_FILTER_FANT, DirectX::TEX_THRESHOLD_DEFAULT, ConvertedImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to convert image to DDS pixel format: " + path.string(), ErrorHandler::EErrorLevel::Critical);

		if (FAILED(Result)) {
			return;
		}

		Images = ConvertedImage.GetImages();
		ImageCount = ConvertedImage.GetImageCount();
		ImageMetaData = &ConvertedImage.GetMetadata();
	}

	if (ImageMetaData->mipLevels == 1) {
		Result = DirectX::GenerateMipMaps(Images, ImageCount, *ImageMetaData, DirectX::TEX_FILTER_FANT, 0, GeneratedMipChain);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to generate mip maps: " + path.string(), ErrorHandler::EErrorLevel::Critical);

		if (FAILED(Result)) {
			return;
		}

		Images = GeneratedMipChain.GetImages();
		ImageCount = GeneratedMipChain.GetImageCount();
		ImageMetaData = &GeneratedMipChain.GetMetadata();
	}

	if (textureExtension != ETextureExtension::DDS) {
		DirectX::Blob DDSData{};
		Result = DirectX::SaveToDDSMemory(Images, ImageCount, *ImageMetaData, DirectX::DDS_FLAGS_FORCE_DX10_EXT, DDSData);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to convert image to DDS: " + path.string(), ErrorHandler::EErrorLevel::Critical);

		if (FAILED(Result)) {
			return;
		}

		Result = DirectX::LoadFromDDSMemory(DDSData.GetConstBufferPointer(), DDSData.GetBufferSize(), DirectX::DDS_FLAGS_NONE, &SourceImageMetaData, DDSImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load converted DDS texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);

		if (FAILED(Result)) {
			return;
		}

		Images = DDSImage.GetImages();
		ImageCount = DDSImage.GetImageCount();
		ImageMetaData = &DDSImage.GetMetadata();
	}

	Result = DirectX::CreateShaderResourceView(device, Images, ImageCount, *ImageMetaData, ShaderResourceView.ReleaseAndGetAddressOf());
	ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to create texture shader resource view: " + path.string(), ErrorHandler::EErrorLevel::Critical);

}

void UTexture::Serialize(FArchive& Ar) {
	UAsset::Serialize(Ar);
}
