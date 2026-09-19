#include "PCH.h"
#include "UMaterial.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace {
	struct FMaterialGroupGPUData {
		FVector4 DiffuseAndOpacity{ 1.0f, 1.0f, 1.0f, 1.0f };
		FVector4 AmbientAndShininess{};
		FVector4 Specular{};
		FVector4 Emissive{};
		FVector4 Reserved0{};
		FVector4 Reserved1{};
		FVector4 Reserved2{};
		FVector4 Reserved3{};
	};

	static_assert(sizeof(FMaterialGroupGPUData) == MATERIAL_GPU_STRIDE);

	bool ParseVector3(std::istringstream& Stream, FVector3& OutValue) {
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;

		if (!(Stream >> X >> Y >> Z)) {
			return false;
		}

		OutValue = FVector3{ X, Y, Z };
		return true;
	}

	std::filesystem::path GetTextureReference(std::istringstream& Stream) {
		std::string Token{};
		std::string TextureReference{};

		while (Stream >> Token) {
			TextureReference = Token;
		}

		return TextureReference;
	}
}

void UMaterial::Reset() {
	Groups.clear();
	GPUIndices.clear();
	bGPUDataDirty = true;
}

bool UMaterial::Initialize(ID3D11Device* Device, const std::filesystem::path& MtlPath, const FTextureResolver& TextureResolver) {
	if (!TextureResolver || !UAsset::Initialize(Device, MtlPath)) {
		return false;
	}

	Reset();

	std::ifstream File(MtlPath);
	if (!File.is_open()) {
		return false;
	}

	FMaterialGroup CurrentGroup{};
	bool bHasCurrentGroup = false;
	std::string RawLine{};

	while (std::getline(File, RawLine)) {
		if (!RawLine.empty() && RawLine.back() == '\r') {
			RawLine.pop_back();
		}

		std::istringstream Stream(RawLine);
		std::string Command{};
		Stream >> Command;

		if (Command.empty() || Command[0] == '#') {
			continue;
		}

		if (Command == "newmtl") {
			std::string Name{};
			Stream >> Name;

			if (Name.empty()) {
				continue;
			}

			if (bHasCurrentGroup) {
				Groups.push_back(std::move(CurrentGroup));
			}

			CurrentGroup = {};
			CurrentGroup.Name = Name.c_str();
			bHasCurrentGroup = true;
			continue;
		}

		if (!bHasCurrentGroup) {
			continue;
		}

		if (Command == "Ka") {
			ParseVector3(Stream, CurrentGroup.Ambient);
		}
		else if (Command == "Kd") {
			ParseVector3(Stream, CurrentGroup.Diffuse);
		}
		else if (Command == "Ks") {
			ParseVector3(Stream, CurrentGroup.Specular);
		}
		else if (Command == "Ke") {
			ParseVector3(Stream, CurrentGroup.Emissive);
		}
		else if (Command == "Ns") {
			Stream >> CurrentGroup.Shininess;
		}
		else if (Command == "d") {
			Stream >> CurrentGroup.Opacity;
		}
		else if (Command == "Tr") {
			float Transparency = 0.0f;

			if (Stream >> Transparency) {
				CurrentGroup.Opacity = 1.0f - Transparency;
			}
		}
		else if (Command == "map_Kd" || Command == "map_Bump" || Command == "bump" || Command == "norm") {
			const std::filesystem::path TextureReference = GetTextureReference(Stream);

			if (TextureReference.empty()) {
				return false;
			}

			const std::filesystem::path TexturePath = (MtlPath.parent_path() / TextureReference).lexically_normal();
			const FAssetHandle TextureHandle = TextureResolver(TexturePath);

			if (!TextureHandle) {
				return false;
			}

			if (Command == "map_Kd") {
				CurrentGroup.DiffuseTexture = TextureHandle;
			}
			else {
				CurrentGroup.NormalTexture = TextureHandle;
			}
		}
	}

	if (bHasCurrentGroup) {
		Groups.push_back(std::move(CurrentGroup));
	}

	bGPUDataDirty = !Groups.empty();
	return !Groups.empty();
}

void UMaterial::BuildGPUData(FMaterialGPUSlot& OutSlot) const {
    OutSlot = {};
}

void UMaterial::BuildGPUData(uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const {
	if (Groups.empty()) {
		BuildGPUData(OutSlot);
		return;
	}

	if (GroupIndex >= Groups.size()) {
		OutSlot = {};
		return;
	}

	const FMaterialGroup& Group = Groups[GroupIndex];
	FMaterialGroupGPUData Data{};
	Data.DiffuseAndOpacity = FVector4{ Group.Diffuse.x, Group.Diffuse.y, Group.Diffuse.z, Group.Opacity };
	Data.AmbientAndShininess = FVector4{ Group.Ambient.x, Group.Ambient.y, Group.Ambient.z, Group.Shininess };
	Data.Specular = FVector4{ Group.Specular.x, Group.Specular.y, Group.Specular.z, 0.0f };
	Data.Emissive = FVector4{ Group.Emissive.x, Group.Emissive.y, Group.Emissive.z, 0.0f };

	std::memcpy(OutSlot.Data.data(), &Data, sizeof(Data));
}

FMaterialChunkSignature UMaterial::BuildChunkSignature() const {
	return BuildChunkSignature(0);
}

FMaterialChunkSignature UMaterial::BuildChunkSignature(uint32 GroupIndex) const {
	FMaterialChunkSignatureBuilder Builder{};

	if (Groups.empty() || GroupIndex >= Groups.size()) {
		return Builder.Build();
	}

	Builder.AddTexture(Groups[GroupIndex].DiffuseTexture);
	Builder.AddTexture(Groups[GroupIndex].NormalTexture);

	return Builder.Build();
}

void UMaterial::Finalize(IAssetQuery* Query) {
}

std::optional<uint32> UMaterial::FindGroupIndex(const FString& Name) const {
	for (uint32 Index = 0; Index < Groups.size(); ++Index) {
		if (Groups[Index].Name == Name) {
			return Index;
		}
	}

	return std::nullopt;
}

void UMaterial::Serialize(FArchive& Ar) {
	UAsset::Serialize(Ar);
}
