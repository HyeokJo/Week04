#pragma once

#include "STL.h"

struct FAssetPath {
    FString Path{};

    FAssetPath() = default;

    explicit FAssetPath(const FString& InPath)
        : Path(InPath) {
    }

    explicit FAssetPath(FString&& InPath)
        : Path(std::move(InPath)) {
    }

    bool IsValid() const {
        return !Path.empty();
    }

    explicit operator bool() const {
        return IsValid();
    }

    bool operator==(const FAssetPath& Other) const = default;
};

namespace std {
template<>
struct hash<FAssetPath> {
    size_t operator()(const FAssetPath& AssetPath) const noexcept {
        return hash<FString>{}(AssetPath.Path);
    }
};
}
