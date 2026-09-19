#pragma once 
#include "FAssetHandle.h"
#include "FAssetPath.h"
#include "UAsset.h"

class IAssetQuery {
public:
	virtual ~IAssetQuery() = default;

public:
	virtual FAssetHandle FindAsset(const FAssetPath& AssetPath) const = 0;

};
