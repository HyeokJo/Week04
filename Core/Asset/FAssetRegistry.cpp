#include "PCH.h"
#include "FAssetRegistry.h"

#include "UColorMaterial.h"
#include "Render/Pipeline/UPipeline.h"

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

    if (Entry == nullptr || Entry->Asset == nullptr) {
        return false;
    }

    if (Entry->Asset->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo())) {
        MaterialBuffer.UnregisterMaterial(static_cast<UMaterial*>(Entry->Asset.get()));
    }

    RemoveHandleMappings(Handle);
    Entry->Asset.reset();
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
