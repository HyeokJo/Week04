#pragma once

#include "FMaterialGroup.h"
#include "UMaterial.h"

#include <functional>

class USurfaceOpaque : public UMaterial {
public:
	USurfaceOpaque() = default;
	virtual ~USurfaceOpaque() = default;

	USurfaceOpaque(const USurfaceOpaque&) = delete;
	USurfaceOpaque& operator=(const USurfaceOpaque&) = delete;

	USurfaceOpaque(USurfaceOpaque&&) noexcept = default;
	USurfaceOpaque& operator=(USurfaceOpaque&&) noexcept = default;

public:
	JG_DECLARE_DERIVED_TYPEINFO(USurfaceOpaque, UMaterial);

	using FTextureResolver = std::function<FAssetHandle(const std::filesystem::path& TexturePath)>;

	bool Initialize(ID3D11Device* Device, const std::filesystem::path& MtlPath, const FTextureResolver& TextureResolver);
	void BuildGPUData(FMaterialGPUSlot& OutSlot) const override;
	void BuildGPUData(uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const override;
	FMaterialChunkSignature BuildChunkSignature() const override;
	FMaterialChunkSignature BuildChunkSignature(uint32 GroupIndex) const override;
	uint32 GetGPUDataCount() const override { return Groups.empty() ? 1 : static_cast<uint32>(Groups.size()); }
	std::optional<uint32> FindGroupIndex(const FString& Name) const override;

	const TArray<FMaterialGroup>& GetGroups() const { return Groups; }

protected:
	void Serialize(FArchive& Ar) override;

private:
	void Reset();

	TArray<FMaterialGroup> Groups{};
};
