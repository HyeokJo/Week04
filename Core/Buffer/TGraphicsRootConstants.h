#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>

enum class EGraphicsShaderStage : uint32 {
	None = 0,

	Vertex = 1 << 0,
	Hull = 1 << 1,
	Domain = 1 << 2,
	Geometry = 1 << 3,
	Pixel = 1 << 4,
	Compute = 1 << 5,

	Graphics = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4),
	All = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5)
};

constexpr EGraphicsShaderStage operator|(EGraphicsShaderStage Left, EGraphicsShaderStage Right) {
	return static_cast<EGraphicsShaderStage>(static_cast<uint32>(Left) | static_cast<uint32>(Right));
}

constexpr EGraphicsShaderStage operator&(EGraphicsShaderStage Left, EGraphicsShaderStage Right) {
	return static_cast<EGraphicsShaderStage>(static_cast<uint32>(Left) & static_cast<uint32>(Right));
}

constexpr EGraphicsShaderStage& operator|=(EGraphicsShaderStage& Left, EGraphicsShaderStage Right) {
	Left = Left | Right;
	return Left;
}

constexpr bool HasGraphicsShaderStage(EGraphicsShaderStage Value, EGraphicsShaderStage Stage) {
	return static_cast<uint32>(Value & Stage) != 0;
}

#define GRAPHICS_ROOT_32BIT_OFFSET(Type, Member) static_cast<uint32>(offsetof(Type, Member) / sizeof(uint32))

template<uint32 N>
concept CGraphicsRootConstantCount = N > 0 && (N % 4) == 0;

template<uint32 ConstantCount> requires CGraphicsRootConstantCount<ConstantCount>
class TGraphicsRootConstants {
private:
	static constexpr uint32 DataByteSize = ConstantCount * sizeof(uint32);

	static_assert(DataByteSize <= D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16, "Graphics root constants exceed the Direct3D 11 constant buffer limit.");

public:
	TGraphicsRootConstants() = default;
	~TGraphicsRootConstants() = default;

	TGraphicsRootConstants(const TGraphicsRootConstants&) = delete;
	TGraphicsRootConstants& operator=(const TGraphicsRootConstants&) = delete;

	TGraphicsRootConstants(TGraphicsRootConstants&&) noexcept = default;
	TGraphicsRootConstants& operator=(TGraphicsRootConstants&&) noexcept = default;

public:
	bool Initialize(ID3D11Device* Device) {
		if (!Device) {
			return false;
		}

		Reset();

		D3D11_BUFFER_DESC Description{};
		Description.ByteWidth = DataByteSize;
		Description.Usage = D3D11_USAGE_DYNAMIC;
		Description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		Description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Description.MiscFlags = 0;
		Description.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA InitialData{};
		InitialData.pSysMem = Constants.data();

		if (FAILED(Device->CreateBuffer(&Description, &InitialData, Buffer.ReleaseAndGetAddressOf()))) {
			Reset();
			return false;
		}

		bDirty = false;

		return true;
	}

	template<typename T>
	bool SetGraphicsRoot32BitConstant(const T& SrcData, uint32 DestOffsetIn32BitValues) {
		static_assert(std::is_trivially_copyable_v<T>, "Graphics root constant data must be trivially copyable.");
		static_assert(sizeof(T) == sizeof(uint32), "SetGraphicsRoot32BitConstant requires exactly one 32-bit value.");

		if (DestOffsetIn32BitValues >= ConstantCount) {
			return false;
		}

		std::memcpy(Constants.data() + DestOffsetIn32BitValues, &SrcData, sizeof(T));

		bDirty = true;

		return true;
	}

	template<typename T>
	bool SetGraphicsRoot32BitConstants(const T& SrcData, uint32 DestOffsetIn32BitValues = 0) {
		static_assert(std::is_trivially_copyable_v<T>, "Graphics root constant data must be trivially copyable.");
		static_assert(sizeof(T) % sizeof(uint32) == 0, "Graphics root constant data size must be a multiple of 32 bits.");

		constexpr uint32 Num32BitValues = static_cast<uint32>(sizeof(T) / sizeof(uint32));

		if (DestOffsetIn32BitValues > ConstantCount || Num32BitValues > ConstantCount - DestOffsetIn32BitValues) {
			return false;
		}

		std::memcpy(Constants.data() + DestOffsetIn32BitValues, &SrcData, sizeof(T));

		bDirty = true;

		return true;
	}

	bool Bind(ID3D11DeviceContext* Context, uint32 Slot, EGraphicsShaderStage ShaderStages) {
		if (!Context || !Buffer) {
			return false;
		}

		if (bDirty && !Commit(Context)) {
			return false;
		}

		ID3D11Buffer* ConstantBuffer = Buffer.Get();

		if (HasGraphicsShaderStage(ShaderStages, EGraphicsShaderStage::Vertex)) {
			Context->VSSetConstantBuffers(Slot, 1, &ConstantBuffer);
		}

		if (HasGraphicsShaderStage(ShaderStages, EGraphicsShaderStage::Hull)) {
			Context->HSSetConstantBuffers(Slot, 1, &ConstantBuffer);
		}

		if (HasGraphicsShaderStage(ShaderStages, EGraphicsShaderStage::Domain)) {
			Context->DSSetConstantBuffers(Slot, 1, &ConstantBuffer);
		}

		if (HasGraphicsShaderStage(ShaderStages, EGraphicsShaderStage::Geometry)) {
			Context->GSSetConstantBuffers(Slot, 1, &ConstantBuffer);
		}

		if (HasGraphicsShaderStage(ShaderStages, EGraphicsShaderStage::Pixel)) {
			Context->PSSetConstantBuffers(Slot, 1, &ConstantBuffer);
		}

		if (HasGraphicsShaderStage(ShaderStages, EGraphicsShaderStage::Compute)) {
			Context->CSSetConstantBuffers(Slot, 1, &ConstantBuffer);
		}

		return true;
	}

	bool Commit(ID3D11DeviceContext* Context) {
		if (!Context || !Buffer) {
			return false;
		}

		if (!bDirty) {
			return true;
		}

		D3D11_MAPPED_SUBRESOURCE MappedResource{};

		if (FAILED(Context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource))) {
			return false;
		}

		std::memcpy(MappedResource.pData, Constants.data(), DataByteSize);

		Context->Unmap(Buffer.Get(), 0);

		bDirty = false;

		return true;
	}

	void Reset() {
		Buffer.Reset();
		Constants.fill(0);

		bDirty = false;
	}

public:
	[[nodiscard]] ID3D11Buffer* GetBuffer() const { return Buffer.Get(); }

	[[nodiscard]] static constexpr uint32 GetConstantCount() { return ConstantCount; }
	[[nodiscard]] static constexpr uint32 GetByteSize() { return DataByteSize; }

	[[nodiscard]] bool IsValid() const { return Buffer != nullptr; }
	[[nodiscard]] bool IsDirty() const { return bDirty; }

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	std::array<uint32, ConstantCount> Constants{};

	bool bDirty = false;
};

/*
[32 .......... ] 
*/