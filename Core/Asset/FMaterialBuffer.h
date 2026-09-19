#pragma once

#include "FMaterialGPUData.h"

#include <d3d11.h>
#include <wrl/client.h>

class UMaterial;

struct FMaterialBufferEntry {
	UMaterial* Material{ nullptr };
	uint32 GroupIndex{ 0 };
};

class FMaterialBuffer {
public:
    FMaterialBuffer() = default;
    ~FMaterialBuffer() = default;

    FMaterialBuffer(const FMaterialBuffer&) = delete;
    FMaterialBuffer& operator=(const FMaterialBuffer&) = delete;

    FMaterialBuffer(FMaterialBuffer&&) = delete;
    FMaterialBuffer& operator=(FMaterialBuffer&&) = delete;

public:
    bool Initialize(ID3D11Device* Device, uint32 MaxMaterialCount);

    bool RegisterMaterial(UMaterial* Material);
    void UnregisterMaterial(UMaterial* Material);

    void Flush(ID3D11DeviceContext* DeviceContext);

    ID3D11Buffer* GetBuffer() const { return Buffer.Get(); }
    ID3D11ShaderResourceView* const* GetSRV() const { return SRV.GetAddressOf(); }

    uint32 GetMaxMaterialCount() const { return MaxMaterialCount; }
    void Reset()
    {
        MaxMaterialCount = 0;
        Slots.clear();
        Materials.clear();
        FreeIndices.clear();
        Buffer.Reset();
        SRV.Reset();
    }

private:
    uint32 AllocateSlot();
    void ReleaseSlot(uint32 Index);

private:
    uint32 MaxMaterialCount{ 0 };

    TArray<FMaterialGPUSlot> Slots{};
    TArray<FMaterialBufferEntry> Materials{};
    TArray<uint32> FreeIndices{};

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer{};
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV{};
};
