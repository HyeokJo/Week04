#pragma once

#include "FMaterialChunkSignature.h"
#include "FMaterialGPUData.h"
#include "UAsset.h"
#include "Common.h"

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
	JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(UMaterial, UAsset);

	virtual void BuildGPUData(FMaterialGPUSlot& OutSlot) const = 0;
	virtual void BuildGPUData(uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const;
	virtual FMaterialChunkSignature BuildChunkSignature() const;
	virtual FMaterialChunkSignature BuildChunkSignature(uint32 GroupIndex) const;
	virtual void Finalize(IAssetQuery* Query);

	uint32 GetGPUIndex() const { return GPUIndices.empty() ? UINT32_MAX : GPUIndices[0]; }
	uint32 GetGPUIndex(uint32 GroupIndex) const { return GroupIndex < GPUIndices.size() ? GPUIndices[GroupIndex] : UINT32_MAX; }
	std::span<const uint32> GetMaterialIndices() const { return GPUIndices; }
	virtual uint32 GetGPUDataCount() const { return 1; }
	virtual std::optional<uint32> FindGroupIndex(const FString& Name) const;

	void MarkGPUDataDirty() { bGPUDataDirty = true; }

protected:
	void Serialize(FArchive& Ar) override;

private:
	friend class FMaterialBuffer;

	TArray<uint32> GPUIndices{};
	bool bGPUDataDirty{ true };
};
