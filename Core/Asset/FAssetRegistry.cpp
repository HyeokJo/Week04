#include "PCH.h"
#include "FAssetRegistry.h"
#include "../../ErrorHandler.h"
#include "UColorMaterial.h"
#include "Render/Pipeline/UPipeline.h"

#include "FAssetRegistry.h"

#include "../../ErrorHandler.h"

namespace {
    constexpr const char* DefaultStaticMeshMaterialName = "__DefaultStaticMeshMaterial";
    constexpr const char* DefaultStaticMeshPipelineName = "__DefaultStaticMeshPipeline";
    constexpr const char* DefaultStaticMeshMaterialMetadataPath = "./Content/Metadata/DefaultStaticMeshMaterial.meta";
    constexpr const char* DefaultStaticMeshPipelineMetadataPath = "./Content/Metadata/DefaultStaticMeshPipeline.meta";
}

FAssetRegistry::FAssetRegistry() {
	AssetLoaders["Material"] = [this](const std::filesystem::path& filePath) { return LoadMaterial(filePath); };
	AssetLoaders["Mesh"] = [this](const std::filesystem::path& filePath) { return LoadMesh(filePath); };
	AssetLoaders["Texture"] = [this](const std::filesystem::path& filePath) { return LoadTexture(filePath); };
	AssetLoaders["Pipeline"] = [this](const std::filesystem::path& filePath) { return LoadPipelineState(filePath); };
}

bool FAssetRegistry::Initialize(ID3D11Device* Device, uint32 MaxMaterialCount) {
    if (Device == nullptr) {
        return false;
    }

    if (!MaterialBuffer.Initialize(Device, MaxMaterialCount)) {
        return false;
    }

    this->Device = Device;
    return true;
}



void FAssetRegistry::LoadDefaults() {
    // 기본 기하 도형들 마치 있는 것 처럼 로드할 것 
}

void FAssetRegistry::LoadIterate(const std::filesystem::path& dir) {
    // 주어진 dir 을 순회할 재귀함수. 
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (std::filesystem::is_directory(entry)) {
            LoadIterate(entry.path());
        }

        FString Type = FString{ dir.filename().generic_string() } ;
		FString Name = FString{ entry.path().stem().generic_string() };

		FString Key = Type + "." + Name;

        std::unique_ptr<UObject> newAsset{}; 
        if (auto It = AssetLoaders.find(Type); It != AssetLoaders.end()) {
			newAsset = std::move(std::invoke(It->second, entry.path()));
		}
		else {
			ErrorHandler::Report("[ AssetRegistry ]", "No loader found for asset type: " + Type, ErrorHandler::EErrorLevel::Warning);
		}



    }
}

std::unique_ptr<UObject> FAssetRegistry::LoadMaterial(const std::filesystem::path& filePath) {

    return std::unique_ptr<UObject>();
}

std::unique_ptr<UObject> FAssetRegistry::LoadMesh(const std::filesystem::path& filePath) {

    return std::unique_ptr<UObject>();
}

std::unique_ptr<UObject> FAssetRegistry::LoadTexture(const std::filesystem::path& filePath) {

    return std::unique_ptr<UObject>();
}

std::unique_ptr<UObject> FAssetRegistry::LoadPipelineState(const std::filesystem::path& filePath) {

    return std::unique_ptr<UObject>();
}


//FAssetHandle FAssetRegistry::EnsureDefaultStaticMeshMaterial() {
//    const FAssetHandle ExistingHandle = GetAsset(DefaultStaticMeshMaterialName);
//    if (ResolveAsset<UMaterial>(ExistingHandle) != nullptr) {
//        return ExistingHandle;
//    }
//
//    if (ExistingHandle || Device == nullptr) {
//        return {};
//    }
//
//    return EmplaceAsset<UColorMaterial>(Device, DefaultStaticMeshMaterialName, DefaultStaticMeshMaterialMetadataPath);
//}
//
//FAssetHandle FAssetRegistry::EnsureDefaultStaticMeshPipeline() {
//    const FAssetHandle ExistingHandle = GetAsset(DefaultStaticMeshPipelineName);
//    if (ResolveAsset<UPipeline>(ExistingHandle) != nullptr) {
//        return ExistingHandle;
//    }
//
//    if (ExistingHandle || Device == nullptr) {
//        return {};
//    }
//
//    return EmplaceAsset<UPipeline>(Device, DefaultStaticMeshPipelineName, DefaultStaticMeshPipelineMetadataPath);
//}
//
//FAssetHandle FAssetRegistry::AdoptAsset(ID3D11Device* Device, const FGuid& ID, const FString& Name, const std::filesystem::path& MetadataPath, std::unique_ptr<UObject>&& Asset) {
//    if (Device == nullptr || Asset == nullptr || !ID.IsValid()) {
//        return {};
//    }
//
//    if (AssetNameToHandle.contains(Name) || AssetIDToHandle.contains(ID)) {
//        return {};
//    }
//
//    if (!Asset->GetTypeInfo()->IsA(UAsset::StaticTypeInfo())) {
//        return {};
//    }
//
//    UAsset* TypedAsset = static_cast<UAsset*>(Asset.get());
//
//    TypedAsset->SetAssetName(Name);
//    TypedAsset->Initialize(Device, MetadataPath);
//
//    if (Asset->GetTypeInfo()->IsA<UMaterial>()) {
//        UMaterial* Material = static_cast<UMaterial*>(Asset.get());
//		ErrorHandler::Report(not MaterialBuffer.RegisterMaterial(Material), "FAssetRegistry::AdoptAsset", "Failed to register material in the material buffer.", ErrorHandler::EErrorLevel::Error);
//    }
//
//    const FAssetHandle Handle = AllocateHandle();
//
//    if (Handle.ID < Assets.size()) {
//        Assets[Handle.ID] = {Handle, std::move(Asset)};
//    }
//    else {
//        Assets.emplace_back(Handle, std::move(Asset));
//    }
//
//    AssetNameToHandle[Name] = Handle;
//    AssetIDToHandle[ID] = Handle;
//
//    return Handle;
//}

FAssetHandle FAssetRegistry::GetAsset(const FString& Name) const {
    const auto It = AssetNameToHandle.find(Name);

    if (It == AssetNameToHandle.end()) {
        return {};
    }

    return It->second;
}

FAssetHandle FAssetRegistry::GetAsset(const FGuid& ID) const {
    const auto It = AssetIDToHandle.find(ID);

    if (It == AssetIDToHandle.end()) {
        return {};
    }

    return It->second;
}

bool FAssetRegistry::RemoveAsset(FAssetHandle Handle) {
    if (Handle.ID >= Assets.size()) {
        return false;
    }

    auto& Entry = Assets[Handle.ID];

    if (Entry.first != Handle || Entry.second == nullptr) {
        return false;
    }

    if (Entry.second->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo())) {
        UMaterial* Material = static_cast<UMaterial*>(Entry.second.get());
        MaterialBuffer.UnregisterMaterial(Material);
    }

    RemoveHandleMappings(Handle);

    Entry.second.reset();

    Entry.first = FAssetHandle{
        Handle.ID,
        Handle.Generation + 1
    };

    FreeHandles.push_back(Entry.first);

    return true;
}

void FAssetRegistry::Finalize() {
	for (auto& [Handle, Asset] : Assets) {
		if(Asset->GetTypeInfo()->IsA<UMaterial>()) {
			auto* mat = static_cast<UMaterial*>(Asset.get());
            mat->Finalize(this); 
            mat->MarkGPUDataDirty(); 
		}
	}
}

FAssetHandle FAssetRegistry::AllocateHandle() {
    if (!FreeHandles.empty()) {
        const FAssetHandle Handle = FreeHandles.back();
        FreeHandles.pop_back();
        return Handle;
    }

    return FAssetHandle{
        static_cast<uint32>(Assets.size()),
        0
    };
}

void FAssetRegistry::RemoveHandleMappings(FAssetHandle Handle) {
    for (auto It = AssetNameToHandle.begin(); It != AssetNameToHandle.end();) {
        if (It->second == Handle) {
            It = AssetNameToHandle.erase(It);
        }
        else {
            ++It;
        }
    }

    for (auto It = AssetIDToHandle.begin(); It != AssetIDToHandle.end();) {
        if (It->second == Handle) {
            It = AssetIDToHandle.erase(It);
        }
        else {
            ++It;
        }
    }
}

