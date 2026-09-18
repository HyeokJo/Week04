#include "PCH.h"
#include "FAssetRegistry.h"

#include "UColorMaterial.h"
#include "Render/Pipeline/UPipeline.h"

#include <cctype>

namespace {
constexpr const char* DefaultStaticMeshMaterialName = "__DefaultStaticMeshMaterial";
constexpr const char* DefaultStaticMeshPipelineName = "__DefaultStaticMeshPipeline";
constexpr const char* DefaultStaticMeshMaterialMetadataPath = "./Content/Metadata/DefaultStaticMeshMaterial.meta";
constexpr const char* DefaultStaticMeshPipelineMetadataPath = "./Content/Metadata/DefaultStaticMeshPipeline.meta";
}

bool FAssetRegistry::Initialize(ID3D11Device* Device, uint32 MaxMaterialCount) {
    if (Device == nullptr || !MaterialBuffer.Initialize(Device, MaxMaterialCount)) {
        return false;
    }

    this->Device = Device;
    return DiscoverAssets(std::filesystem::current_path() / "Content");
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

    while (!ErrorCode && Iterator != End) {
        const std::filesystem::directory_entry Entry = *Iterator;

        if (Entry.is_regular_file(ErrorCode)) {
            DiscoverAssetFile(Entry.path());
        }

        Iterator.increment(ErrorCode);

        if (ErrorCode) {
            ErrorCode.clear();
        }
    }

    return true;
}

FAssetHandle FAssetRegistry::FindAsset(const FAssetPath& AssetPath) const {
    const auto It = PathToHandle.find(AssetPath);

    if (It == PathToHandle.end()) {
        return {};
    }

    return It->second;
}

UAsset* FAssetRegistry::GetUAsset(const FString& Name) {
    return ResolveAsset<UAsset>(GetAsset(Name));
}

FAssetHandle FAssetRegistry::GetAsset(const FString& Name) const {
    const auto It = LegacyNameToHandle.find(Name);

    if (It == LegacyNameToHandle.end()) {
        return {};
    }

    return It->second;
}

FAssetHandle FAssetRegistry::GetAsset(const FGuid& ID) const {
    const auto It = GuidToHandle.find(ID);

    if (It == GuidToHandle.end()) {
        return {};
    }

    return It->second;
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
    Entry->AssetType = EAssetType::END;
    Entry->Handle = FAssetHandle{ Handle.ID, Handle.Generation + 1 };
    FreeHandles.push_back(Entry->Handle);

    return true;
}

void FAssetRegistry::Reset() {
    Assets.clear();
    FreeHandles.clear();
    PathToHandle.clear();
    LegacyNameToHandle.clear();
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
    const FAssetHandle ExistingHandle = GetAsset(DefaultStaticMeshMaterialName);

    if (ResolveAsset<UMaterial>(ExistingHandle) != nullptr) {
        return ExistingHandle;
    }

    if (ExistingHandle || Device == nullptr) {
        return {};
    }

    return EmplaceAsset<UColorMaterial>(Device, DefaultStaticMeshMaterialName, DefaultStaticMeshMaterialMetadataPath);
}

FAssetHandle FAssetRegistry::EnsureDefaultStaticMeshPipeline() {
    const FAssetHandle ExistingHandle = GetAsset(DefaultStaticMeshPipelineName);

    if (ResolveAsset<UPipeline>(ExistingHandle) != nullptr) {
        return ExistingHandle;
    }

    if (ExistingHandle || Device == nullptr) {
        return {};
    }

    return EmplaceAsset<UPipeline>(Device, DefaultStaticMeshPipelineName, DefaultStaticMeshPipelineMetadataPath);
}

bool FAssetRegistry::AdoptAsset(ID3D11Device* Device, const FGuid& ID, const FString& Name, const std::filesystem::path& MetadataPath, std::unique_ptr<UObject>&& Asset) {
    if (Device == nullptr || Asset == nullptr || !ID.IsValid() || GetAsset(Name)) {
        return false;
    }

    if (!Asset->GetTypeInfo()->IsA(UAsset::StaticTypeInfo())) {
        return false;
    }

    std::unique_ptr<UAsset> TypedAsset(static_cast<UAsset*>(Asset.release()));
    const FAssetPath AssetPath = MakeLegacyAssetPath(Name);

    if (!AssetPath || PathToHandle.contains(AssetPath)) {
        return false;
    }

    TypedAsset->SetAssetName(Name);
    TypedAsset->Initialize(Device, MetadataPath);

    if (TypedAsset->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo()) && !MaterialBuffer.RegisterMaterial(static_cast<UMaterial*>(TypedAsset.get()))) {
        return false;
    }

    const FAssetHandle Handle = AllocateHandle();
    FAssetEntry Entry{};
    Entry.AssetPath = AssetPath;
    Entry.PhysicalPath = MetadataPath;
    Entry.Handle = Handle;
    Entry.Asset = std::move(TypedAsset);

    if (Handle.ID < Assets.size()) {
        Assets[Handle.ID] = std::move(Entry);
    }
    else {
        Assets.emplace_back(std::move(Entry));
    }

    PathToHandle[AssetPath] = Handle;
    LegacyNameToHandle[Name] = Handle;
    GuidToHandle[ID] = Handle;

    return true;
}

FAssetPath FAssetRegistry::MakeLegacyAssetPath(const FString& Name) {
    return FAssetPath{ FString{ "/Engine/Legacy/" } + Name };
}

bool FAssetRegistry::DiscoverAssetFile(const std::filesystem::path& FilePath) {
    const EAssetType AssetType = GetAssetType(FilePath);

    if (AssetType == EAssetType::END) {
        return true;
    }

    return RegisterDiscoveredAsset(MakeAssetPath(FilePath), FilePath, AssetType);
}

bool FAssetRegistry::RegisterDiscoveredAsset(const FAssetPath& AssetPath, const std::filesystem::path& PhysicalPath, EAssetType AssetType) {
    if (!AssetPath || AssetType == EAssetType::END || PathToHandle.contains(AssetPath)) {
        return false;
    }

    const FAssetHandle Handle = AllocateHandle();
    FAssetEntry Entry{};
    Entry.AssetPath = AssetPath;
    Entry.PhysicalPath = PhysicalPath;
    Entry.AssetType = AssetType;
    Entry.Handle = Handle;

    if (Handle.ID < Assets.size()) {
        Assets[Handle.ID] = std::move(Entry);
    }
    else {
        Assets.emplace_back(std::move(Entry));
    }

    PathToHandle[AssetPath] = Handle;
    return true;
}

FAssetPath FAssetRegistry::MakeAssetPath(const std::filesystem::path& PhysicalPath) const {
    std::error_code ErrorCode{};
    std::filesystem::path RelativePath = std::filesystem::relative(PhysicalPath, ContentRoot, ErrorCode).lexically_normal();

    if (ErrorCode || RelativePath.empty() || *RelativePath.begin() == "..") {
        return {};
    }

    RelativePath.replace_extension();
    return FAssetPath{ FString{ "/Game/" } + RelativePath.generic_string().c_str() };
}

EAssetType FAssetRegistry::GetAssetType(const std::filesystem::path& FilePath) {
    FString Extension = FilePath.extension().generic_string().c_str();

    std::ranges::transform(Extension, Extension.begin(), [](unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });

    if (Extension == ".obj") {
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

    if (Extension == ".json" && FilePath.parent_path().filename() == "Pipeline") {
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

    for (auto It = LegacyNameToHandle.begin(); It != LegacyNameToHandle.end();) {
        if (It->second == Handle) {
            It = LegacyNameToHandle.erase(It);
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
