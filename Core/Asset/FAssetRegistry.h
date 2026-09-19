#pragma once

#include "../Base/UObject.h"
#include "Common.h"
#include "FAssetEntry.h"
#include "FMaterialBuffer.h"
#include "UMaterial.h"

#include <d3d11.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

class FAssetRegistry : public IAssetQuery {
public:
    FAssetRegistry() = default;
    ~FAssetRegistry() = default;

    FAssetRegistry(const FAssetRegistry&) = delete;
    FAssetRegistry& operator=(const FAssetRegistry&) = delete;

    FAssetRegistry(FAssetRegistry&&) = delete;
    FAssetRegistry& operator=(FAssetRegistry&&) = delete;

public:
    bool Initialize(ID3D11Device* Device, uint32 MaxMaterialCount = 4096);

	bool DiscoverAssets(const std::filesystem::path& Directory);
	bool LoadAssetsOfType(ID3D11Device* Device, EAssetType AssetType);

    const std::filesystem::path& GetContentRoot() const {
        return ContentRoot;
    }

    const TArray<FAssetEntry>& GetAssetEntries() const {
        return Assets;
    }

    FAssetHandle FindAsset(const FAssetPath& AssetPath) const override;
    FAssetHandle FindAsset(const FGuid& PersistentGuid) const;
    const FAssetPath* GetAssetPath(FAssetHandle Handle) const;
    const FGuid* GetAssetGuid(FAssetHandle Handle) const;

    bool RemoveAsset(FAssetHandle Handle);

    template<typename T>
    requires std::is_base_of_v<UAsset, T>
    T* ResolveAsset(FAssetHandle Handle) {
        FAssetEntry* Entry = FindEntry(Handle);

        if (Entry == nullptr || Entry->Asset == nullptr || !Entry->Asset->GetTypeInfo()->IsA(T::StaticTypeInfo())) {
            return nullptr;
        }

        return static_cast<T*>(Entry->Asset.get());
    }

    template<typename T>
    requires std::is_base_of_v<UAsset, T>
    const T* ResolveAsset(FAssetHandle Handle) const {
        const FAssetEntry* Entry = FindEntry(Handle);

        if (Entry == nullptr || Entry->Asset == nullptr || !Entry->Asset->GetTypeInfo()->IsA(T::StaticTypeInfo())) {
            return nullptr;
        }

        return static_cast<const T*>(Entry->Asset.get());
    }

    template<typename T, typename Func>
    requires std::is_base_of_v<UAsset, T>
    void ModifyAsset(FAssetHandle Handle, Func&& Modifier) {
        T* Asset = ResolveAsset<T>(Handle);

        if (Asset == nullptr) {
            return;
        }

        std::invoke(std::forward<Func>(Modifier), *Asset);
    }

    FMaterialBuffer& GetMaterialBuffer() {
        return MaterialBuffer;
    }

    const FMaterialBuffer& GetMaterialBuffer() const {
        return MaterialBuffer;
    }

    auto GetAssetList() const {
        return Assets | std::ranges::views::filter([](const FAssetEntry& Entry) {
            return Entry.Asset != nullptr;
        }) | std::ranges::views::transform([](const FAssetEntry& Entry) -> UObject* {
            return Entry.Asset.get();
        });
    }

    void Reset();
    void Finalize();

    FAssetHandle EnsureDefaultStaticMeshMaterial();
    FAssetHandle EnsureDefaultStaticMeshPipeline();

private:
	bool EnsureSystemAssets();
	bool DiscoverAssetFile(const std::filesystem::path& FilePath);
	bool LoadTexture(FAssetEntry& Entry, ID3D11Device* Device);
	bool LoadFont(FAssetEntry& Entry, ID3D11Device* Device);
	bool LoadPipeline(FAssetEntry& Entry, ID3D11Device* Device);
	bool LoadMaterial(FAssetEntry& Entry, ID3D11Device* Device);
	bool LoadMesh(FAssetEntry& Entry, ID3D11Device* Device);
	bool RegisterDiscoveredAsset(const FAssetPath& AssetPath, const std::filesystem::path& PhysicalPath, const std::filesystem::path& SidecarPath, const FGuid& PersistentGuid, EAssetType AssetType);

    FAssetPath MakeAssetPath(const std::filesystem::path& PhysicalPath) const;
    static EAssetType GetAssetType(const std::filesystem::path& FilePath);
    static std::filesystem::path MakeSidecarPath(const std::filesystem::path& AssetPath);
    static bool LoadOrCreatePersistentGuid(const std::filesystem::path& SidecarPath, FGuid& OutGuid);
    static bool IsPipelineFamilyUnit(const std::filesystem::path& FilePath);
    static std::filesystem::path FindFirstPipelineFamilyUnit(const std::filesystem::path& FamilyDirectory);

    FAssetHandle AllocateHandle();
    FAssetEntry* FindEntry(FAssetHandle Handle);
    const FAssetEntry* FindEntry(FAssetHandle Handle) const;
    void RemoveHandleMappings(FAssetHandle Handle);

private:
    TArray<FAssetEntry> Assets{};
    TArray<FAssetHandle> FreeHandles{};

    TMap<FAssetPath, FAssetHandle> PathToHandle{};
    TMap<FGuid, FAssetHandle> GuidToHandle{};

    FMaterialBuffer MaterialBuffer{};
    ID3D11Device* Device{ nullptr };
    std::filesystem::path ContentRoot{};
};
