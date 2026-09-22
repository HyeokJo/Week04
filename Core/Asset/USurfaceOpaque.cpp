#include "PCH.h"
#include "USurfaceOpaque.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace {
	struct FSurfaceOpaqueGroupGPUData {
		FVector4 DiffuseAndOpacity{ 1.0f, 1.0f, 1.0f, 1.0f };
		FVector4 AmbientAndShininess{};
		FVector4 Specular{};
		FVector4 Emissive{};
		FVector4 Reserved0{};
		FVector4 Reserved1{};
		FVector4 Reserved2{};
		FVector4 Reserved3{};
	};

	static_assert(sizeof(FSurfaceOpaqueGroupGPUData) == MATERIAL_GPU_STRIDE);

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

	bool LoadTextureMap(FMaterialTextureMap& OutTextureMap, std::istringstream& Stream, const std::filesystem::path& MtlPath, const USurfaceOpaque::FTextureResolver& TextureResolver) {
		const std::filesystem::path TextureReference = GetTextureReference(Stream);

		if (TextureReference.empty()) {
			return false;
		}

		const std::filesystem::path TexturePath = (MtlPath.parent_path() / TextureReference).lexically_normal();
		OutTextureMap.SourcePath = TextureReference.generic_string().c_str();
		OutTextureMap.Texture = TextureResolver(TexturePath);
		return static_cast<bool>(OutTextureMap.Texture);
	}
}

void USurfaceOpaque::Reset() {
	Groups.clear();
	MarkGPUDataDirty();
}

bool USurfaceOpaque::Initialize(ID3D11Device* Device, const std::filesystem::path& MtlPath, const FTextureResolver& TextureResolver) {
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
		else if (Command == "Tf") {
			ParseVector3(Stream, CurrentGroup.TransmissionFilter);
		}
		else if (Command == "Ns") {
			Stream >> CurrentGroup.Shininess;
		}
		else if (Command == "Ni") {
			Stream >> CurrentGroup.RefractionIndex;
		}
		else if (Command == "d") {
			std::string Token{};
			Stream >> Token;

			if (Token == "-halo") {
				CurrentGroup.bDissolveHalo = true;
				Stream >> CurrentGroup.Opacity;
			}
			else if (!Token.empty()) {
				std::istringstream OpacityStream(Token);
				OpacityStream >> CurrentGroup.Opacity;
			}
		}
		else if (Command == "Tr") {
			float Transparency = 0.0f;

			if (Stream >> Transparency) {
				CurrentGroup.Opacity = 1.0f - Transparency;
			}
		}
		else if (Command == "illum") {
			Stream >> CurrentGroup.IlluminationModel;
		}
		else if (Command == "sharpness") {
			Stream >> CurrentGroup.Sharpness;
		}
		else if (Command == "map_Ka") {
			LoadTextureMap(CurrentGroup.AmbientTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "map_Kd") {
			LoadTextureMap(CurrentGroup.DiffuseTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "map_Ks") {
			LoadTextureMap(CurrentGroup.SpecularTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "map_Ke") {
			LoadTextureMap(CurrentGroup.EmissiveTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "map_Tf") {
			LoadTextureMap(CurrentGroup.TransmissionTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "map_Ns") {
			LoadTextureMap(CurrentGroup.ShininessTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "map_d") {
			LoadTextureMap(CurrentGroup.OpacityTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "map_Bump" || Command == "map_bump" || Command == "bump") {
			LoadTextureMap(CurrentGroup.BumpTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "norm") {
			LoadTextureMap(CurrentGroup.NormalTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "disp") {
			LoadTextureMap(CurrentGroup.DisplacementTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "decal") {
			LoadTextureMap(CurrentGroup.DecalTexture, Stream, MtlPath, TextureResolver);
		}
		else if (Command == "refl") {
			LoadTextureMap(CurrentGroup.ReflectionTexture, Stream, MtlPath, TextureResolver);
		}
	}

	if (bHasCurrentGroup) {
		Groups.push_back(std::move(CurrentGroup));
	}

	if (Groups.empty()) {
		return false;
	}

	MarkGPUDataDirty();
	return true;
}

void USurfaceOpaque::BuildGPUData(FMaterialGPUSlot& OutSlot) const {
	BuildGPUData(0, OutSlot);
}

void USurfaceOpaque::BuildGPUData(uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const {
	if (GroupIndex >= Groups.size()) {
		OutSlot = {};
		return;
	}

	const FMaterialGroup& Group = Groups[GroupIndex];
	FSurfaceOpaqueGroupGPUData Data{};
	Data.DiffuseAndOpacity = FVector4{ Group.Diffuse.x, Group.Diffuse.y, Group.Diffuse.z, Group.Opacity };
	Data.AmbientAndShininess = FVector4{ Group.Ambient.x, Group.Ambient.y, Group.Ambient.z, Group.Shininess };
	Data.Specular = FVector4{ Group.Specular.x, Group.Specular.y, Group.Specular.z, 0.0f };
	Data.Emissive = FVector4{ Group.Emissive.x, Group.Emissive.y, Group.Emissive.z, 0.0f };

	std::memcpy(OutSlot.Data.data(), &Data, sizeof(Data));
}

FMaterialChunkSignature USurfaceOpaque::BuildChunkSignature() const {
	return BuildChunkSignature(0);
}

FMaterialChunkSignature USurfaceOpaque::BuildChunkSignature(uint32 GroupIndex) const {
	FMaterialChunkSignatureBuilder Builder{};

	if (GroupIndex >= Groups.size()) {
		return Builder.Build();
	}

	const FMaterialGroup& Group = Groups[GroupIndex];
	Builder.AddTexture(Group.DiffuseTexture.Texture);
	Builder.AddTexture(Group.EmissiveTexture.Texture);
	Builder.AddTexture(Group.NormalTexture.Texture ? Group.NormalTexture.Texture : Group.BumpTexture.Texture);

	return Builder.Build();
}

std::optional<uint32> USurfaceOpaque::FindGroupIndex(const FString& Name) const {
	for (uint32 Index = 0; Index < Groups.size(); ++Index) {
		if (Groups[Index].Name == Name) {
			return Index;
		}
	}

	return std::nullopt;
}

const TArray<FMaterialGroup>& USurfaceOpaque::GetGroups() const {
	return Groups;
}

bool USurfaceOpaque::ModifyGroup(uint32 GroupIndex, const std::function<void(FMaterialGroup&)>& Modifier) {
	if (GroupIndex >= Groups.size() || !Modifier) {
		return false;
	}

	Modifier(Groups[GroupIndex]);
	MarkGPUDataDirty();
	return true;
}

void USurfaceOpaque::Serialize(FArchive& Ar) {
	UMaterial::Serialize(Ar);
}
