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

	bool Initialize(ID3D11Device* Device, const std::filesystem::path& ImagePath, bool MakeDDS, ETextureFormat TextureFormat = ETextureFormat::UNORM, bool bGenerateMipMap = true);

	ID3D11ShaderResourceView* GetSRV() const { return ShaderResourceView.Get(); }
protected:
	virtual void Serialize(FArchive& Ar) override;
private:
	bool InitializeInternal(ID3D11Device* Device, const std::filesystem::path& ImagePath, ETextureFormat TextureFormat, ETextureExtension TextureExtension, bool bGenerateMipMap, bool MakeDDS);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResourceView{};
};
