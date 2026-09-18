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

    const std::filesystem::path& GetContentRoot() const {
        return ContentRoot;
    }

    const TArray<FAssetEntry>& GetAssetEntries() const {
        return Assets;
    }

    FAssetHandle FindAsset(const FAssetPath& AssetPath) const override;
    UAsset* GetUAsset(const FString& Name) override;
    FAssetHandle GetAsset(const FString& Name) const override;
    FAssetHandle GetAsset(const FGuid& ID) const override;

    bool RemoveAsset(FAssetHandle Handle);

    template<typename T, typename... TArgs>
    requires std::is_base_of_v<UAsset, T>
    FAssetHandle EmplaceAsset(ID3D11Device* Device, const FString& Name, const std::filesystem::path& MetadataPath, TArgs&&... Args) {
        return EmplaceAssetAtPath<T>(Device, MakeLegacyAssetPath(Name), Name, MetadataPath, std::forward<TArgs>(Args)...);
    }

    template<typename T, typename... TArgs>
    requires std::is_base_of_v<UAsset, T>
    FAssetHandle EmplaceAssetAtPath(ID3D11Device* Device, const FAssetPath& AssetPath, const FString& Name, const std::filesystem::path& MetadataPath, TArgs&&... Args) {
        if (Device == nullptr || !AssetPath || PathToHandle.contains(AssetPath)) {
            return {};
        }

        std::unique_ptr<T> NewAsset = std::make_unique<T>(std::forward<TArgs>(Args)...);
        NewAsset->SetAssetName(Name);
        NewAsset->Initialize(Device, MetadataPath);

        if constexpr (std::is_base_of_v<UMaterial, T>) {
            if (!MaterialBuffer.RegisterMaterial(NewAsset.get())) {
                return {};
            }
        }

        const FAssetHandle Handle = AllocateHandle();
        FAssetEntry Entry{};
        Entry.AssetPath = AssetPath;
        Entry.PhysicalPath = MetadataPath;
        Entry.Handle = Handle;
        Entry.Asset = std::move(NewAsset);

        if (Handle.ID < Assets.size()) {
            Assets[Handle.ID] = std::move(Entry);
        }
        else {
            Assets.emplace_back(std::move(Entry));
        }

        PathToHandle[AssetPath] = Handle;
        LegacyNameToHandle[Name] = Handle;
        GuidToHandle[Assets[Handle.ID].Asset->GetGuid()] = Handle;

        return Handle;
    }

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

    bool AdoptAsset(ID3D11Device* Device, const FGuid& ID, const FString& Name, const std::filesystem::path& MetadataPath, std::unique_ptr<UObject>&& Asset);


private:
    static FAssetPath MakeLegacyAssetPath(const FString& Name);

    bool DiscoverAssetFile(const std::filesystem::path& FilePath);
    bool RegisterDiscoveredAsset(const FAssetPath& AssetPath, const std::filesystem::path& PhysicalPath, EAssetType AssetType);
    FAssetPath MakeAssetPath(const std::filesystem::path& PhysicalPath) const;
    static EAssetType GetAssetType(const std::filesystem::path& FilePath);

    FAssetHandle AllocateHandle();
    FAssetEntry* FindEntry(FAssetHandle Handle);
    const FAssetEntry* FindEntry(FAssetHandle Handle) const;
    void RemoveHandleMappings(FAssetHandle Handle);

private:
    TArray<FAssetEntry> Assets{};
    TArray<FAssetHandle> FreeHandles{};

    TMap<FAssetPath, FAssetHandle> PathToHandle{};
    TMap<FString, FAssetHandle> LegacyNameToHandle{};
    TMap<FGuid, FAssetHandle> GuidToHandle{};

    FMaterialBuffer MaterialBuffer{};
    ID3D11Device* Device{ nullptr };
    std::filesystem::path ContentRoot{};
};
