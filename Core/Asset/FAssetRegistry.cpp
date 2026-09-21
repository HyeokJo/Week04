#include "PCH.h"
#include "FAssetRegistry.h"

#include "UFreeTypeFont.h"
#include "UMesh.h"
#include "USurfaceOpaque.h"
#include "UTexture.h"
#include "FObjImporter.h"
#include "Render/Pipeline/UPipeline.h"
#include "Core/Console/Console.h"
#include "Serialize/FObjSerializer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace {
constexpr const char* DefaultStaticMeshMaterialAssetPath = "/Game/System/Material/Default.mtl";
constexpr const char* DefaultStaticMeshPipelineAssetPath = "/Game/Pipeline/Base";
constexpr const char* DefaultCheckerboardTexturePath = "/Game/Texture/checkerboard.png";
constexpr const char* GizmoPipelineAssetPath = "/Game/Pipeline/Gizmo.json";

FString GetLowercaseExtension(const std::filesystem::path& FilePath) {
    FString Extension = FilePath.extension().generic_string().c_str();
    std::ranges::transform(Extension, Extension.begin(), [](unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    return Extension;
}
}

bool FAssetRegistry::Initialize(ID3D11Device* Device, uint32 MaxMaterialCount) {
    if (Device == nullptr || !MaterialBuffer.Initialize(Device, MaxMaterialCount)) {
        return false;
	}

	this->Device = Device;

	if (!DiscoverAssets(std::filesystem::current_path() / "Content")) {
		return false;
	}

	if (!LoadAssetsOfType(Device, EAssetType::Texture) ||
		!LoadAssetsOfType(Device, EAssetType::Font) ||
		!LoadAssetsOfType(Device, EAssetType::Pipeline) ||
		!LoadAssetsOfType(Device, EAssetType::Material) ||
		!LoadAssetsOfType(Device, EAssetType::Mesh)) {
		return false;
	}

	return EnsureSystemAssets();
}

bool FAssetRegistry::DiscoverAssets(const std::filesystem::path& Directory) {
    std::error_code ErrorCode{};
    const std::filesystem::path AbsoluteDirectory = std::filesystem::absolute(Directory, ErrorCode).lexically_normal();

    if (ErrorCode || !std::filesystem::is_directory(AbsoluteDirectory, ErrorCode)) {
        return false;
    }

    ContentRoot = AbsoluteDirectory;

    std::filesystem::recursive_directory_iterator Iterator(ContentRoot, std::filesystem::directory_options::skip_permission_denied, ErrorCode);
    const std::filesystem::recursive_directory_iterator End{};

    bool bDiscoveredAll = true;

    while (!ErrorCode && Iterator != End) {
        const std::filesystem::directory_entry Entry = *Iterator;

        if (Entry.is_regular_file(ErrorCode)) {
            bDiscoveredAll = DiscoverAssetFile(Entry.path()) && bDiscoveredAll;
        }

        Iterator.increment(ErrorCode);

        if (ErrorCode) {
            ErrorCode.clear();
        }
    }

	return bDiscoveredAll;
}

bool FAssetRegistry::LoadAssetsOfType(ID3D11Device* Device, EAssetType AssetType) {
	if (Device == nullptr) {
		return false;
	}

	bool bLoadedAll = true;

	for (FAssetEntry& Entry : Assets) {
		if (Entry.AssetType != AssetType || Entry.Asset != nullptr) {
			continue;
		}

		bool bLoaded = false;

		if (AssetType == EAssetType::Texture) {
			bLoaded = LoadTexture(Entry, Device);
		}
		else if (AssetType == EAssetType::Font) {
			bLoaded = LoadFont(Entry, Device);
		}
		else if (AssetType == EAssetType::Pipeline) {
			bLoaded = LoadPipeline(Entry, Device);
		}
		else if (AssetType == EAssetType::Material) {
			bLoaded = LoadMaterial(Entry, Device);
		}
		else if (AssetType == EAssetType::Mesh) {
			bLoaded = LoadMesh(Entry, Device);
		}

		bLoadedAll = bLoadedAll && bLoaded;
	}

	return bLoadedAll;
}

FAssetHandle FAssetRegistry::FindAsset(const FAssetPath& AssetPath) const {
    const auto It = PathToHandle.find(AssetPath);

    if (It == PathToHandle.end()) {
        return {};
    }

    return It->second;
}

FAssetHandle FAssetRegistry::FindAsset(const FGuid& PersistentGuid) const {
    const auto It = GuidToHandle.find(PersistentGuid);
    return It != GuidToHandle.end() ? It->second : FAssetHandle{};
}

const FAssetPath* FAssetRegistry::GetAssetPath(FAssetHandle Handle) const {
    const FAssetEntry* Entry = FindEntry(Handle);
    return Entry != nullptr && Entry->AssetPath ? &Entry->AssetPath : nullptr;
}

const FGuid* FAssetRegistry::GetAssetGuid(FAssetHandle Handle) const {
    const FAssetEntry* Entry = FindEntry(Handle);
    return Entry != nullptr && Entry->PersistentGuid.IsValid() ? &Entry->PersistentGuid : nullptr;
}

bool FAssetRegistry::RemoveAsset(FAssetHandle Handle) {
    FAssetEntry* Entry = FindEntry(Handle);

    if (Entry == nullptr) {
        return false;
    }

    if (Entry->Asset != nullptr && Entry->Asset->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo())) {
        MaterialBuffer.UnregisterMaterial(static_cast<UMaterial*>(Entry->Asset.get()));
    }

    RemoveHandleMappings(Handle);
    Entry->Asset.reset();
    Entry->AssetPath = {};
    Entry->PhysicalPath.clear();
    Entry->SidecarPath.clear();
    Entry->PersistentGuid = {};
    Entry->AssetType = EAssetType::END;
    Entry->Handle = FAssetHandle{ Handle.ID, Handle.Generation + 1 };
    FreeHandles.push_back(Entry->Handle);

    return true;
}

FAssetHandle FAssetRegistry::ImportMesh(const std::filesystem::path& SourceObjPath, const FString& TargetVirtualFolder) {
    std::error_code ErrorCode{};
    const std::filesystem::path AbsoluteSourcePath = std::filesystem::absolute(SourceObjPath, ErrorCode).lexically_normal();
    if (ErrorCode || !std::filesystem::is_regular_file(AbsoluteSourcePath, ErrorCode) || GetLowercaseExtension(AbsoluteSourcePath) != ".obj") {
        return {};
    }

    const std::filesystem::path TargetFolder = ResolveContentFolder(TargetVirtualFolder);
    if (TargetFolder.empty()) {
        return {};
    }

    std::filesystem::create_directories(TargetFolder, ErrorCode);
    if (ErrorCode) {
        return {};
    }

    std::filesystem::path TargetBinaryPath = TargetFolder / AbsoluteSourcePath.filename();
    TargetBinaryPath.replace_extension(".bin");
    const bool bTargetExists = std::filesystem::exists(TargetBinaryPath, ErrorCode);
    if (ErrorCode) {
        return {};
    }
    if (bTargetExists) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Mesh import target already exists: %s", TargetBinaryPath.generic_string().c_str());
        return {};
    }

    FObjImporter Importer{};
    FGeometry Geometry{};
    if (!Importer.LoadObjFile(AbsoluteSourcePath.string().c_str(), Geometry) || !FObjSerializer::SaveBinary(Geometry, TargetBinaryPath.string().c_str())) {
        std::filesystem::remove(TargetBinaryPath, ErrorCode);
        return {};
    }

    if (!DiscoverAssetFile(TargetBinaryPath)) {
        std::filesystem::remove(TargetBinaryPath, ErrorCode);
        std::filesystem::remove(MakeSidecarPath(TargetBinaryPath), ErrorCode);
        return {};
    }

    const FAssetHandle Handle = FindAsset(MakeAssetPath(TargetBinaryPath));
    FAssetEntry* Entry = FindEntry(Handle);
    if (Entry == nullptr || Entry->AssetType != EAssetType::Mesh || !LoadMesh(*Entry, Device)) {
        if (Handle) {
            RemoveAsset(Handle);
        }
        std::filesystem::remove(TargetBinaryPath, ErrorCode);
        std::filesystem::remove(MakeSidecarPath(TargetBinaryPath), ErrorCode);
        return {};
    }

    return Handle;
}

void FAssetRegistry::Reset() {
    Assets.clear();
    FreeHandles.clear();
    PathToHandle.clear();
    GuidToHandle.clear();
    MaterialBuffer.Reset();
    Device = nullptr;
    ContentRoot.clear();
}

void FAssetRegistry::Finalize() {
    for (FAssetEntry& Entry : Assets) {
        if (Entry.Asset == nullptr || !Entry.Asset->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo())) {
            continue;
        }

        UMaterial* Material = static_cast<UMaterial*>(Entry.Asset.get());
        Material->Finalize(this);
        Material->MarkGPUDataDirty();
    }
}

FAssetHandle FAssetRegistry::EnsureDefaultStaticMeshMaterial() {
    const FAssetHandle DefaultMaterialHandle = FindAsset(FAssetPath{ DefaultStaticMeshMaterialAssetPath });
    if (ResolveAsset<UMaterial>(DefaultMaterialHandle) == nullptr) {
        return {};
    }

    return DefaultMaterialHandle;
}

FAssetHandle FAssetRegistry::EnsureDefaultStaticMeshPipeline() {
    const FAssetHandle DefaultPipelineHandle = FindAsset(FAssetPath{ DefaultStaticMeshPipelineAssetPath });
    if (ResolveAsset<UPipeline>(DefaultPipelineHandle) == nullptr) {
        return {};
    }

    return DefaultPipelineHandle;
}

std::filesystem::path FAssetRegistry::ResolveContentFolder(const FString& VirtualFolder) const {
    constexpr std::string_view VirtualRoot = "/Game";

    if (VirtualFolder == VirtualRoot) {
        return ContentRoot;
    }

    constexpr std::string_view VirtualPrefix = "/Game/";

    if (!VirtualFolder.starts_with(VirtualPrefix)) {
        return {};
    }

    const FString RelativeFolder = VirtualFolder.substr(VirtualPrefix.size());

    return (ContentRoot / std::filesystem::path(RelativeFolder)).lexically_normal();
}

bool FAssetRegistry::EnsureSystemAssets() {
	struct FSystemMeshDefinition {
		const char* AssetPath;
	};

	constexpr std::array SystemMeshes{
		FSystemMeshDefinition{ "/Game/System/Mesh/Capsule.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/Cone.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/Cube.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/Cylinder.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/GizmoTorus.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/Plane.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/Pyramid.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/SkyDome.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/Sphere.bin" },
		FSystemMeshDefinition{ "/Game/System/Mesh/Torus.bin" }
	};

	for (const FSystemMeshDefinition& Definition : SystemMeshes) {
		const FAssetHandle Handle = FindAsset(FAssetPath{ Definition.AssetPath });
		if (ResolveAsset<UMesh>(Handle) == nullptr) {
			return false;
		}
	}

	struct FSystemAssetDefinition {
		const char* AssetPath;
	};

	constexpr std::array SystemGizmoMaterials{
		FSystemAssetDefinition{ "/Game/System/Material/Red.mtl" },
		FSystemAssetDefinition{ "/Game/System/Material/Green.mtl" },
		FSystemAssetDefinition{ "/Game/System/Material/Blue.mtl" }
	};

	for (const FSystemAssetDefinition& Definition : SystemGizmoMaterials) {
		const FAssetHandle Handle = FindAsset(FAssetPath{ Definition.AssetPath });
		if (ResolveAsset<UMaterial>(Handle) == nullptr) {
			return false;
		}
	}

	const FAssetHandle GizmoPipelineHandle = FindAsset(FAssetPath{ GizmoPipelineAssetPath });
	if (ResolveAsset<UPipeline>(GizmoPipelineHandle) == nullptr) {
		return false;
	}
	return true;
}

bool FAssetRegistry::DiscoverAssetFile(const std::filesystem::path& FilePath) {
    const EAssetType AssetType = GetAssetType(FilePath);

    if (AssetType == EAssetType::END) {
        return true;
    }

    const std::filesystem::path SidecarPath = MakeSidecarPath(FilePath);
    FGuid PersistentGuid{};
    if (!LoadOrCreatePersistentGuid(SidecarPath, PersistentGuid)) {
        return false;
    }

	if (AssetType == EAssetType::Pipeline && IsPipelineFamilyUnit(FilePath)) {
		const std::filesystem::path FamilyDirectory = FilePath.parent_path();
		const std::filesystem::path FirstUnitPath = FindFirstPipelineFamilyUnit(FamilyDirectory);
		if (FirstUnitPath.empty()) {
			return false;
		}

		if (FilePath.lexically_normal() != FirstUnitPath) {
			return true;
		}

		return RegisterDiscoveredAsset(MakeAssetPath(FamilyDirectory), FamilyDirectory, SidecarPath, PersistentGuid, AssetType);
	}

	return RegisterDiscoveredAsset(MakeAssetPath(FilePath), FilePath, SidecarPath, PersistentGuid, AssetType);
}

bool FAssetRegistry::LoadTexture(FAssetEntry& Entry, ID3D11Device* Device) {
	std::unique_ptr<UTexture> Texture = std::make_unique<UTexture>();
	Texture->SetAssetName(Entry.AssetPath.Path);
	Texture->Initialize(Device, Entry.PhysicalPath);

	if (Texture->GetSRV() == nullptr) {
		return false;
	}

	Entry.Asset = std::move(Texture);

	return true;
}

bool FAssetRegistry::LoadFont(FAssetEntry& Entry, ID3D11Device* Device) {
	std::unique_ptr<UFreeTypeFont> Font = std::make_unique<UFreeTypeFont>();
	Font->SetAssetName(Entry.AssetPath.Path);

	if (!Font->Initialize(Device, Entry.PhysicalPath) || Font->GetAtlasSRV() == nullptr) {
		return false;
	}

	Entry.Asset = std::move(Font);

	return true;
}

bool FAssetRegistry::LoadPipeline(FAssetEntry& Entry, ID3D11Device* Device) {
	std::unique_ptr<UPipeline> Pipeline = std::make_unique<UPipeline>();
	Pipeline->SetAssetName(Entry.AssetPath.Path);

	const bool bInitialized = Pipeline->Initialize(Device, Entry.PhysicalPath);

	if (!bInitialized) {
		return false;
	}

	Entry.Asset = std::move(Pipeline);

	return true;
}

bool FAssetRegistry::LoadMaterial(FAssetEntry& Entry, ID3D11Device* Device) {
	std::unique_ptr<USurfaceOpaque> Material = std::make_unique<USurfaceOpaque>();
	Material->SetAssetName(Entry.AssetPath.Path);

	const FAssetHandle CheckerboardHandle = FindAsset(FAssetPath{ DefaultCheckerboardTexturePath });
	if (ResolveAsset<UTexture>(CheckerboardHandle) == nullptr) {
		return false;
	}

	const bool bInitialized = Material->Initialize(Device, Entry.PhysicalPath, [this, CheckerboardHandle](const std::filesystem::path& TexturePath) {
		const FAssetHandle TextureHandle = FindAsset(MakeAssetPath(TexturePath));
		return ResolveAsset<UTexture>(TextureHandle) != nullptr ? TextureHandle : CheckerboardHandle;
	});

	if (!bInitialized || !MaterialBuffer.RegisterMaterial(Material.get())) {
		return false;
	}

	Entry.Asset = std::move(Material);

	return true;
}

bool FAssetRegistry::LoadMesh(FAssetEntry& Entry, ID3D11Device* Device) {
	std::unique_ptr<UMesh> Mesh = std::make_unique<UMesh>();
	Mesh->SetAssetName(Entry.AssetPath.Path);

	const bool bBinaryAsset = GetLowercaseExtension(Entry.PhysicalPath) == ".bin";
	const std::filesystem::path SourceObjPath = bBinaryAsset ? std::filesystem::path{} : Entry.PhysicalPath;
	std::filesystem::path BinaryPath = Entry.PhysicalPath;
	if (!bBinaryAsset) {
		BinaryPath.replace_extension(".bin");
	}

	const bool bInitialized = Mesh->Initialize(
		Device,
		SourceObjPath,
		BinaryPath,
		[this](const std::filesystem::path& MaterialPath) {
			return FindAsset(MakeAssetPath(MaterialPath));
		},
		[this](FAssetHandle MaterialHandle, const FString& GroupName) -> std::optional<uint32> {
			const UMaterial* Material = ResolveAsset<UMaterial>(MaterialHandle);
			return Material != nullptr ? Material->FindGroupIndex(GroupName) : std::nullopt;
		});

	if (!bInitialized) {
		Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to load model: %s", Entry.PhysicalPath.generic_string().c_str());
		return false;
	}

	Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Loaded model: %s", Entry.PhysicalPath.generic_string().c_str());
	Entry.Asset = std::move(Mesh);

	return true;
}

bool FAssetRegistry::RegisterDiscoveredAsset(const FAssetPath& AssetPath, const std::filesystem::path& PhysicalPath, const std::filesystem::path& SidecarPath, const FGuid& PersistentGuid, EAssetType AssetType) {
    if (!AssetPath || !PersistentGuid.IsValid() || AssetType == EAssetType::END || PathToHandle.contains(AssetPath) || GuidToHandle.contains(PersistentGuid)) {
        return false;
    }

    const FAssetHandle Handle = AllocateHandle();
    FAssetEntry Entry{};
    Entry.AssetPath = AssetPath;
    Entry.PhysicalPath = PhysicalPath;
    Entry.SidecarPath = SidecarPath;
    Entry.PersistentGuid = PersistentGuid;
    Entry.AssetType = AssetType;
    Entry.Handle = Handle;

    if (Handle.ID < Assets.size()) {
        Assets[Handle.ID] = std::move(Entry);
    }
    else {
        Assets.emplace_back(std::move(Entry));
    }

    PathToHandle[AssetPath] = Handle;
    GuidToHandle[PersistentGuid] = Handle;
    return true;
}

std::filesystem::path FAssetRegistry::MakeSidecarPath(const std::filesystem::path& AssetPath) {
    return std::filesystem::path{ AssetPath.string() + ".meta" };
}

bool FAssetRegistry::LoadOrCreatePersistentGuid(const std::filesystem::path& SidecarPath, FGuid& OutGuid) {
    if (std::filesystem::exists(SidecarPath)) {
        std::ifstream Input(SidecarPath);
        rapidjson::Document Document{};
        rapidjson::IStreamWrapper Stream(Input);
        Document.ParseStream(Stream);

        if (!Input.good() && !Input.eof()) {
            return false;
        }

        if (!Document.IsObject() || !Document.HasMember("Guid") || !Document["Guid"].IsString()) {
            return false;
        }

        return OutGuid.Parse(Document["Guid"].GetString()) && OutGuid.IsValid();
    }

    OutGuid = FGuid::NewGuid();
    if (!OutGuid.IsValid()) {
        return false;
    }

    rapidjson::Document Document{};
    Document.SetObject();
    rapidjson::Document::AllocatorType& Allocator = Document.GetAllocator();
    const FString GuidString = OutGuid.ToString();
    Document.AddMember("Guid", rapidjson::Value(GuidString.c_str(), Allocator), Allocator);

    rapidjson::StringBuffer Buffer{};
    rapidjson::PrettyWriter<rapidjson::StringBuffer> Writer(Buffer);
    Document.Accept(Writer);

    std::ofstream Output(SidecarPath, std::ios::binary | std::ios::trunc);
    if (!Output.is_open()) {
        return false;
    }

    Output << Buffer.GetString() << "\r\n";
    return Output.good();
}

bool FAssetRegistry::IsPipelineFamilyUnit(const std::filesystem::path& FilePath) {
	const std::filesystem::path FamilyDirectory = FilePath.parent_path();
	return FilePath.extension() == ".json" && FamilyDirectory.parent_path().filename() == "Pipeline" && FilePath.stem().generic_string().starts_with(FamilyDirectory.filename().generic_string() + "_");
}

std::filesystem::path FAssetRegistry::FindFirstPipelineFamilyUnit(const std::filesystem::path& FamilyDirectory) {
	std::error_code ErrorCode{};
	std::vector<std::filesystem::path> UnitPaths{};
	for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(FamilyDirectory, ErrorCode)) {
		if (ErrorCode) {
			return {};
		}

		if (Entry.is_regular_file(ErrorCode) && IsPipelineFamilyUnit(Entry.path())) {
			UnitPaths.emplace_back(Entry.path().lexically_normal());
		}
	}

	if (ErrorCode || UnitPaths.empty()) {
		return {};
	}

	std::ranges::sort(UnitPaths, {}, [](const std::filesystem::path& Path) {
		return Path.filename().generic_string();
	});
	return UnitPaths.front();
}

FAssetPath FAssetRegistry::MakeAssetPath(const std::filesystem::path& PhysicalPath) const {
    std::error_code ErrorCode{};
    std::filesystem::path RelativePath = std::filesystem::relative(PhysicalPath, ContentRoot, ErrorCode).lexically_normal();

    if (ErrorCode || RelativePath.empty() || *RelativePath.begin() == "..") {
        return {};
    }

    return FAssetPath{ FString{ "/Game/" } + RelativePath.generic_string().c_str() };
}

EAssetType FAssetRegistry::GetAssetType(const std::filesystem::path& FilePath) {
    const FString Extension = GetLowercaseExtension(FilePath);

    if (Extension == ".bin") {
        return EAssetType::Mesh;
    }

    if (Extension == ".mtl") {
        return EAssetType::Material;
    }

    if (Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" || Extension == ".dds" || Extension == ".tga" || Extension == ".bmp" || Extension == ".tif" || Extension == ".tiff" || Extension == ".gif" || Extension == ".hdr") {
        return EAssetType::Texture;
    }

    if (Extension == ".ttf" || Extension == ".otf") {
        return EAssetType::Font;
    }

	if (Extension == ".json" && std::ranges::any_of(FilePath.parent_path(), [](const std::filesystem::path& PathPart) {
		return PathPart == "Pipeline";
	})) {
		return EAssetType::Pipeline;
	}

    return EAssetType::END;
}

FAssetHandle FAssetRegistry::AllocateHandle() {
    if (!FreeHandles.empty()) {
        const FAssetHandle Handle = FreeHandles.back();
        FreeHandles.pop_back();
        return Handle;
    }

    return FAssetHandle{ static_cast<uint32>(Assets.size()), 0 };
}

FAssetEntry* FAssetRegistry::FindEntry(FAssetHandle Handle) {
    if (Handle.ID >= Assets.size()) {
        return nullptr;
    }

    FAssetEntry& Entry = Assets[Handle.ID];
    return Entry.Handle == Handle ? &Entry : nullptr;
}

const FAssetEntry* FAssetRegistry::FindEntry(FAssetHandle Handle) const {
    if (Handle.ID >= Assets.size()) {
        return nullptr;
    }

    const FAssetEntry& Entry = Assets[Handle.ID];
    return Entry.Handle == Handle ? &Entry : nullptr;
}

void FAssetRegistry::RemoveHandleMappings(FAssetHandle Handle) {
    for (auto It = PathToHandle.begin(); It != PathToHandle.end();) {
        if (It->second == Handle) {
            It = PathToHandle.erase(It);
        }
        else {
            ++It;
        }
    }

    for (auto It = GuidToHandle.begin(); It != GuidToHandle.end();) {
        if (It->second == Handle) {
            It = GuidToHandle.erase(It);
        }
        else {
            ++It;
        }
    }

}
