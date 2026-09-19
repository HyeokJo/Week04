#pragma once

#include "../Base/UObject.h"
#include "FMaterialChunkSignature.h"
#include "FMaterialGroup.h"
#include "FMaterialGPUData.h"

#include "UAsset.h"
#include "Common.h"

#include <d3d11.h>
#include <functional>
#include <optional>
#include <span>

class UMaterial : public UAsset {
public:
    UMaterial() = default;
    virtual ~UMaterial() = default;

    UMaterial(const UMaterial&) = delete;
    UMaterial& operator=(const UMaterial&) = delete;

    UMaterial(UMaterial&&) noexcept = default;
    UMaterial& operator=(UMaterial&&) noexcept = default;

public:
	JG_DECLARE_DERIVED_TYPEINFO(UMaterial, UAsset);

	using FTextureResolver = std::function<FAssetHandle(const std::filesystem::path& TexturePath)>;

	bool Initialize(ID3D11Device* Device, const std::filesystem::path& MtlPath, const FTextureResolver& TextureResolver);
    virtual void BuildGPUData(FMaterialGPUSlot& OutSlot) const;
    virtual void BuildGPUData(uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const;
    virtual FMaterialChunkSignature BuildChunkSignature() const;
    virtual FMaterialChunkSignature BuildChunkSignature(uint32 GroupIndex) const;
    virtual void Finalize(IAssetQuery* Query);

    uint32 GetGPUIndex() const { return GPUIndices.empty() ? UINT32_MAX : GPUIndices[0]; }
    uint32 GetGPUIndex(uint32 GroupIndex) const { return GroupIndex < GPUIndices.size() ? GPUIndices[GroupIndex] : UINT32_MAX; }
    std::span<const uint32> GetMaterialIndices() const { return GPUIndices; }
    const TArray<FMaterialGroup>& GetGroups() const { return Groups; }
    uint32 GetGPUDataCount() const { return Groups.empty() ? 1 : static_cast<uint32>(Groups.size()); }
    std::optional<uint32> FindGroupIndex(const FString& Name) const;

    void MarkGPUDataDirty() { bGPUDataDirty = true; }

protected:
	virtual void Serialize(FArchive& Ar) override;

private:
	void Reset();

    friend class FMaterialBuffer;

    TArray<FMaterialGroup> Groups{};
    TArray<uint32> GPUIndices{};
    bool bGPUDataDirty{ true };
};
