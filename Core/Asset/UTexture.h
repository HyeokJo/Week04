#pragma once
#include "UAsset.h"
#include <d3d11.h>
#include <wrl/client.h>

enum class ETextureFormat : uint8 {
	UNORM,
	SRGB
};

enum class ETextureExtension : uint8 {
	DDS,
	TGA,
	BMP,
	PNG,
	GIF,
	TIF,
	TIFF,
	JPG,
	JPEG,
	HDR
};

class UTexture : public UAsset {
public:
    UTexture() {};
    ~UTexture() {};

	UTexture(const UTexture&) = delete;
	UTexture& operator=(const UTexture&) = delete;

	UTexture(UTexture&&) noexcept = default;
	UTexture& operator=(UTexture&&) noexcept = default;

public:
	JG_DECLARE_DERIVED_TYPEINFO(UTexture, UAsset);

    virtual void Initialize(ID3D11Device* device, const std::filesystem::path& metaData) override;
    void InitializeFromFile(ID3D11Device* device, const std::filesystem::path& imagePath, const std::filesystem::path& metaData = {});
    void Initialize(ID3D11Device* device, const std::filesystem::path& metaData, ETextureFormat textureFormat, ETextureExtension textureExtension, const std::filesystem::path& imagePath = {}, bool bGenerateMipMap = true);

	ID3D11ShaderResourceView* GetSRV() const { return ShaderResourceView.Get(); }
protected:
	virtual void Serialize(FArchive& Ar) override;
private:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResourceView{};
};
