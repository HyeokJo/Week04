#pragma once

#include "FAssetHandle.h"
#include "FAssetPath.h"
#include "UAsset.h"

#include <filesystem>
#include <memory>

enum class EAssetType : uint8 {
    Texture,
    Pipeline,
    Material,
    Mesh,
    Font,
    END
};

struct FTextureMetadata {
    bool mMakeDDS{ true };
    bool mGenerateMipMap{ true };
};

struct FMeshMetadata {
    bool mFlipUV{ false };
};

struct FAssetEntry {
    FAssetPath AssetPath{};
    std::filesystem::path PhysicalPath{};
    std::filesystem::path SidecarPath{};
    FGuid PersistentGuid{};
    EAssetType AssetType{ EAssetType::END };
    FAssetHandle Handle{};
    std::unique_ptr<UAsset> Asset{};
    FTextureMetadata mTextureMetadata{};
    FMeshMetadata mMeshMetadata{};
};
