#include "PCH.h"
#include "UTexture.h"

#include "../../ErrorHandler.h"

#include <cctype>
#include <DirectXTex.h>

namespace {
	bool GetTextureExtension(const std::filesystem::path& Path, ETextureExtension& OutExtension) {
		FString Extension = Path.extension().generic_string().c_str();
		std::ranges::transform(Extension, Extension.begin(), [](unsigned char Character) {
			return static_cast<char>(std::tolower(Character));
		});

		if (Extension == ".dds") {
			OutExtension = ETextureExtension::DDS;
			return true;
		}

		if (Extension == ".tga") {
			OutExtension = ETextureExtension::TGA;
			return true;
		}

		if (Extension == ".bmp") {
			OutExtension = ETextureExtension::BMP;
			return true;
		}

		if (Extension == ".png") {
			OutExtension = ETextureExtension::PNG;
			return true;
		}

		if (Extension == ".gif") {
			OutExtension = ETextureExtension::GIF;
			return true;
		}

		if (Extension == ".tif") {
			OutExtension = ETextureExtension::TIF;
			return true;
		}

		if (Extension == ".tiff") {
			OutExtension = ETextureExtension::TIFF;
			return true;
		}

		if (Extension == ".jpg") {
			OutExtension = ETextureExtension::JPG;
			return true;
		}

		if (Extension == ".jpeg") {
			OutExtension = ETextureExtension::JPEG;
			return true;
		}

		if (Extension == ".hdr") {
			OutExtension = ETextureExtension::HDR;
			return true;
		}

		return false;
	}

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

bool UTexture::Initialize(ID3D11Device* Device, const std::filesystem::path& ImagePath, ETextureFormat TextureFormat, bool bGenerateMipMap) {
	ETextureExtension TextureExtension{};
	const bool bValidExtension = GetTextureExtension(ImagePath, TextureExtension);
	ErrorHandler::Report(!bValidExtension, "[ UTexture ]", "Unsupported texture extension: " + ImagePath.string(), ErrorHandler::EErrorLevel::Critical);

	if (!bValidExtension) {
		return false;
	}

	return InitializeInternal(Device, ImagePath, TextureFormat, TextureExtension, bGenerateMipMap);
}

bool UTexture::InitializeInternal(ID3D11Device* Device, const std::filesystem::path& ImagePath, ETextureFormat TextureFormat, ETextureExtension TextureExtension, bool bGenerateMipMap) {
	if (!UAsset::Initialize(Device, ImagePath)) {
		ErrorHandler::Report("[ UTexture ]", "Texture device or image path is invalid: " + ImagePath.string(), ErrorHandler::EErrorLevel::Critical);
		return false;
	}

	const std::filesystem::path& path = ImagePath;
	const DXGI_FORMAT TargetFormat = GetDXGIFormat(TextureFormat);

	if (TargetFormat == DXGI_FORMAT_UNKNOWN) {
		ErrorHandler::Report("[ UTexture ]", "Unsupported texture format setting: " + path.string(), ErrorHandler::EErrorLevel::Critical);
		return false;
	}

	DirectX::ScratchImage SourceImage{};
	DirectX::ScratchImage ConvertedImage{};
	DirectX::ScratchImage GeneratedMipChain{};
	DirectX::ScratchImage DDSImage{};
	DirectX::TexMetadata SourceImageMetaData{};
	HRESULT Result = S_OK;

	if (TextureExtension == ETextureExtension::DDS) {
		Result = DirectX::LoadFromDDSFile(path.wstring().c_str(), DirectX::DDS_FLAGS_NONE, &SourceImageMetaData, SourceImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load DDS texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	}
	else if (TextureExtension == ETextureExtension::TGA) {
		Result = DirectX::LoadFromTGAFile(path.wstring().c_str(), DirectX::TGA_FLAGS_NONE, &SourceImageMetaData, SourceImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load TGA texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	}
	else if (TextureExtension == ETextureExtension::BMP || TextureExtension == ETextureExtension::PNG || TextureExtension == ETextureExtension::GIF || TextureExtension == ETextureExtension::TIF || TextureExtension == ETextureExtension::TIFF || TextureExtension == ETextureExtension::JPG || TextureExtension == ETextureExtension::JPEG) {
		Result = DirectX::LoadFromWICFile(path.wstring().c_str(), DirectX::WIC_FLAGS_NONE, &SourceImageMetaData, SourceImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load WIC texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	}
	else if (TextureExtension == ETextureExtension::HDR) {
		Result = DirectX::LoadFromHDRFile(path.wstring().c_str(), &SourceImageMetaData, SourceImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load HDR texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	}
	else {
		ErrorHandler::Report("[ UTexture ]", "Unsupported texture extension setting: " + path.string(), ErrorHandler::EErrorLevel::Critical);
		return false;
	}

	const DirectX::Image* Images = SourceImage.GetImages();
	size_t ImageCount = SourceImage.GetImageCount();
	const DirectX::TexMetadata* ImageMetaData = &SourceImageMetaData;

	if (TextureExtension != ETextureExtension::DDS and ImageMetaData->format != TargetFormat) {
		Result = DirectX::Convert(Images, ImageCount, *ImageMetaData, TargetFormat, DirectX::TEX_FILTER_FANT, DirectX::TEX_THRESHOLD_DEFAULT, ConvertedImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to convert image to DDS pixel format: " + path.string(), ErrorHandler::EErrorLevel::Critical);

		Images = ConvertedImage.GetImages();
		ImageCount = ConvertedImage.GetImageCount();
		ImageMetaData = &ConvertedImage.GetMetadata();
	}

	if (bGenerateMipMap && ImageMetaData->mipLevels == 1) {
		Result = DirectX::GenerateMipMaps(Images, ImageCount, *ImageMetaData, DirectX::TEX_FILTER_FANT, 0, GeneratedMipChain);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to generate mip maps: " + path.string(), ErrorHandler::EErrorLevel::Critical);

		Images = GeneratedMipChain.GetImages();
		ImageCount = GeneratedMipChain.GetImageCount();
		ImageMetaData = &GeneratedMipChain.GetMetadata();
	}

	if (TextureExtension != ETextureExtension::DDS) {
		DirectX::Blob DDSData{};
		Result = DirectX::SaveToDDSMemory(Images, ImageCount, *ImageMetaData, DirectX::DDS_FLAGS_FORCE_DX10_EXT, DDSData);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to convert image to DDS: " + path.string(), ErrorHandler::EErrorLevel::Critical);

		Result = DirectX::LoadFromDDSMemory(DDSData.GetConstBufferPointer(), DDSData.GetBufferSize(), DirectX::DDS_FLAGS_NONE, &SourceImageMetaData, DDSImage);
		ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to load converted DDS texture: " + path.string(), ErrorHandler::EErrorLevel::Critical);

		Images = DDSImage.GetImages();
		ImageCount = DDSImage.GetImageCount();
		ImageMetaData = &DDSImage.GetMetadata();
	}

	Result = DirectX::CreateShaderResourceView(Device, Images, ImageCount, *ImageMetaData, ShaderResourceView.ReleaseAndGetAddressOf());
	ErrorHandler::ReportHRESULT(Result, "[ UTexture ]", "Failed to create texture shader resource view: " + path.string(), ErrorHandler::EErrorLevel::Critical);
	return SUCCEEDED(Result) && ShaderResourceView != nullptr;
}

void UTexture::Serialize(FArchive& Ar) {
	UAsset::Serialize(Ar);
}
