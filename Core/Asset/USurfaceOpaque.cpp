#include "PCH.h"
#include "USurfaceOpaque.h"

#include <cstring>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace {
	struct FSurfaceOpaqueGroupGPUData {
		FVector4 DiffuseAndOpacity{ 1.0f, 1.0f, 1.0f, 1.0f };
		FVector4 AmbientAndShininess{};
		FVector4 SpecularAndRefractionIndex{};
		FVector4 EmissiveAndSharpness{};
		FVector4 TransmissionFilter{};
		int32 IlluminationModel{};
		uint32 DissolveHalo{};
		float Padding0{};
		float Padding1{};
		FVector4 Reserved1{};
		FVector4 Reserved2{};
	};

	static_assert(sizeof(FSurfaceOpaqueGroupGPUData) == MATERIAL_GPU_STRIDE);
	static_assert(offsetof(FSurfaceOpaqueGroupGPUData, IlluminationModel) == 80);
	static_assert(offsetof(FSurfaceOpaqueGroupGPUData, DissolveHalo) == 84);

	bool ParseVector3(std::istringstream& Stream, FVector3& OutValue) {
		float X{ 0.0f };
		float Y{ 0.0f };
		float Z{ 0.0f };

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
		const std::filesystem::path TextureReference{ GetTextureReference(Stream) };

		if (TextureReference.empty()) {
			return false;
		}

		const std::filesystem::path TexturePath{ (MtlPath.parent_path() / TextureReference).lexically_normal() };
		OutTextureMap.SourcePath = TextureReference.generic_string().c_str();
		OutTextureMap.Texture = TextureResolver(TexturePath);
		return static_cast<bool>(OutTextureMap.Texture);
	}
}

void USurfaceOpaque::Reset() {
	mGroups.clear();
	MarkGPUDataDirty();
}

bool USurfaceOpaque::Initialize(ID3D11Device* Device, const std::filesystem::path& MtlPath, const FTextureResolver& TextureResolver) {
	if (!TextureResolver || !UAsset::Initialize(Device, MtlPath)) {
		return false;
	}

	Reset();

	std::ifstream File{ MtlPath };

	if (!File.is_open()) {
		return false;
	}

	FMaterialGroup CurrentGroup{};
	bool HasCurrentGroup{ false };
	std::string RawLine{};

	while (std::getline(File, RawLine)) {
		if (!RawLine.empty() && RawLine.back() == '\r') {
			RawLine.pop_back();
		}

		std::istringstream Stream{ RawLine };
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

			if (HasCurrentGroup) {
				mGroups.push_back(std::move(CurrentGroup));
			}

			CurrentGroup = {};
			CurrentGroup.Name = Name.c_str();
			HasCurrentGroup = true;
			continue;
		}

		if (!HasCurrentGroup) {
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
				std::istringstream OpacityStream{ Token };
				OpacityStream >> CurrentGroup.Opacity;
			}
		}
		else if (Command == "Tr") {
			float Transparency{ 0.0f };

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

	if (HasCurrentGroup) {
		mGroups.push_back(std::move(CurrentGroup));
	}

	if (mGroups.empty()) {
		return false;
	}

	MarkGPUDataDirty();
	return true;
}

void USurfaceOpaque::BuildGPUData(FMaterialGPUSlot& OutSlot) const {
	BuildGPUData(0, OutSlot);
}

void USurfaceOpaque::BuildGPUData(uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const {
	if (GroupIndex >= mGroups.size()) {
		OutSlot = {};
		return;
	}

	const FMaterialGroup& Group{ mGroups[GroupIndex] };
	FSurfaceOpaqueGroupGPUData Data{};
	Data.DiffuseAndOpacity = FVector4{ Group.Diffuse.x, Group.Diffuse.y, Group.Diffuse.z, Group.Opacity };
	Data.AmbientAndShininess = FVector4{ Group.Ambient.x, Group.Ambient.y, Group.Ambient.z, Group.Shininess };
	Data.SpecularAndRefractionIndex = FVector4{ Group.Specular.x, Group.Specular.y, Group.Specular.z, Group.RefractionIndex };
	Data.EmissiveAndSharpness = FVector4{ Group.Emissive.x, Group.Emissive.y, Group.Emissive.z, Group.Sharpness };
	Data.TransmissionFilter = FVector4{ Group.TransmissionFilter.x, Group.TransmissionFilter.y, Group.TransmissionFilter.z, 0.0f };
	Data.IlluminationModel = Group.IlluminationModel;
	Data.DissolveHalo = Group.bDissolveHalo ? 1u : 0u;

	std::memcpy(OutSlot.Data.data(), &Data, sizeof(Data));
}

FMaterialChunkSignature USurfaceOpaque::BuildChunkSignature() const {
	return BuildChunkSignature(0);
}

FMaterialChunkSignature USurfaceOpaque::BuildChunkSignature(uint32 GroupIndex) const {
	FMaterialChunkSignatureBuilder Builder{};

	if (GroupIndex >= mGroups.size()) {
		return Builder.Build();
	}

	const FMaterialGroup& Group{ mGroups[GroupIndex] };
	Builder.AddTexture(Group.AmbientTexture.Texture);
	Builder.AddTexture(Group.DiffuseTexture.Texture);
	Builder.AddTexture(Group.SpecularTexture.Texture);
	Builder.AddTexture(Group.EmissiveTexture.Texture);
	Builder.AddTexture(Group.TransmissionTexture.Texture);
	Builder.AddTexture(Group.ShininessTexture.Texture);
	Builder.AddTexture(Group.OpacityTexture.Texture);
	Builder.AddTexture(Group.BumpTexture.Texture);
	Builder.AddTexture(Group.NormalTexture.Texture);
	Builder.AddTexture(Group.DisplacementTexture.Texture);
	Builder.AddTexture(Group.DecalTexture.Texture);
	Builder.AddTexture(Group.ReflectionTexture.Texture);

	return Builder.Build();
}

std::optional<uint32> USurfaceOpaque::FindGroupIndex(const FString& Name) const {
	for (uint32 Index{}; Index < mGroups.size(); ++Index) {
		if (mGroups[Index].Name == Name) {
			return Index;
		}
	}

	return std::nullopt;
}

const TArray<FMaterialGroup>& USurfaceOpaque::GetGroups() const {
	return mGroups;
}

bool USurfaceOpaque::ModifyGroup(uint32 GroupIndex, const std::function<void(FMaterialGroup&)>& Modifier) {
	if (GroupIndex >= mGroups.size() || !Modifier) {
		return false;
	}

	Modifier(mGroups[GroupIndex]);
	MarkGPUDataDirty();
	return true;
}

void USurfaceOpaque::Serialize(FArchive& Ar) {
	UMaterial::Serialize(Ar);
}

uint32 USurfaceOpaque::GetGPUDataCount() const {
	return mGroups.empty() ? 1u : static_cast<uint32>(mGroups.size());
}
