#include "PCH.h"
#include "FWorldEditorContext.h"

#include "AActor.h"
#include "Component/UActorComponent.h"
#include "Component/USceneComponent.h"
#include "Core/Asset/FAssetRegistry.h"
#include "UWorld.h"

#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

namespace
{
    bool ReWriteObjFilePath(const std::filesystem::path& MetaPath, const FString& NewObjFilePath)
    {
        std::ifstream InputStream(MetaPath, std::ios::binary);
        if (!InputStream.is_open())
        {
            return false;
        }

        rapidjson::IStreamWrapper InStreamWrapper(InputStream);

        rapidjson::Document Document;
        Document.ParseStream<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(InStreamWrapper);

        //읽기 닫기
        InputStream.close();

        if (Document.HasParseError() || !Document.IsObject()) return false;

        rapidjson::Document::AllocatorType& Allocator = Document.GetAllocator();

        if (Document.HasMember("FilePath"))
        {
            Document["FilePath"].SetString(NewObjFilePath.c_str(), Allocator);
        }
        else
        {
            Document.AddMember("FilePath", rapidjson::Value(NewObjFilePath.c_str(), Allocator), Allocator);
        }

        std::ofstream OutputStream(MetaPath);
        if (!OutputStream.is_open()) return false;

        rapidjson::OStreamWrapper OutStreamWrapper(OutputStream);
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> Writer(OutStreamWrapper);
        Document.Accept(Writer);

        return true;
    }
}

void FWorldEditorContext::SetWorld(UWorld* InWorld) {
    World = InWorld;
}

void FWorldEditorContext::InitializeChannels(FAssetRegistry& AssetRegistry, ID3D11Device* Device) {
    if (World == nullptr) return;

    EditorToWorld.TryBind<FMessageSpawnComponent>([this, &AssetRegistry](const FMessageSpawnComponent& Message) {
        World->HandleSpawnComponent(Message, AssetRegistry);
    });
    EditorToWorld.TryBind<FMessageSaveScene>([this, &AssetRegistry](const FMessageSaveScene& Message) {
        World->SaveScene(Message.SceneName, &AssetRegistry);
    });
    EditorToWorld.TryBind<FMessageLoadScene>([this, &AssetRegistry, Device](const FMessageLoadScene& Message) {
        World->LoadScene(std::filesystem::path(Message.FilePath.c_str()), Device, &AssetRegistry);
    });
	EditorToWorld.TryBind<FMessageImportMesh>([this, &AssetRegistry, Device](const FMessageImportMesh& Message)
											  {
												  //OBJ 로드에 사용하는 MonkeyMesh.meta 파일 내부의 로드할 Obj 파일 경로를 수정합니다.
												  //meta 파일 생성 혹은 meta 파일 로드 시스템이 개선되면 수정이 필요합니다.
												  if (ReWriteObjFilePath(Message.MetaPath, Message.FilePath))
												  {
													  //AssetRegistry.EmplaceAsset<UMesh>(Device, Message.AssetName, std::filesystem::path{ Message.MetaPath });
												  }
											  });
}

void FWorldEditorContext::Dispatch() {
    EditorToWorld.Dispatch();
    WorldToEditor.Dispatch();
}

FMessageChannel::FSender FWorldEditorContext::GetEditorToWorldSender() { return EditorToWorld.GetSender(); }
FMessageChannel::FSender FWorldEditorContext::GetWorldToEditorSender() { return WorldToEditor.GetSender(); }

const FCameraSnapshot* FWorldEditorContext::GetCameraState() const noexcept {
    const auto Reader = SharedState.GetReader();
    return Reader.Peek().Camera ? &*Reader.Peek().Camera : nullptr;
}

void FWorldEditorContext::PublishCameraState(const FCameraSnapshot& State) {
    SharedState.GetWriter().Modify([&State](FWorldEditorSharedState& Shared) { Shared.Camera = State; });
}

const size_t FWorldEditorContext::GetRenderModeState() const noexcept
{
    return SharedState.GetReader().Peek().ModeIndex;
}

void FWorldEditorContext::SetRenderModeState(const size_t State)
{
    SharedState.GetWriter().Modify([&State](FWorldEditorSharedState& Shared) {Shared.ModeIndex = State;});
}

void FWorldEditorContext::SetSelectedActor(AActor* Actor) {
    if (Actor == nullptr) {
        ClearSelection();
        return;
    }

    SelectedActor.Set(Actor);
    SelectedComponent.Set(Actor->GetRootComponent());
}

void FWorldEditorContext::SetSelectedComponent(UActorComponent* Component) {
    if (Component == nullptr || Component->GetOwner() == nullptr) {
        ClearSelection();
        return;
    }

    SelectedActor.Set(Component->GetOwner());
    SelectedComponent.Set(Component);
}

void FWorldEditorContext::ClearSelection() {
    SelectedComponent.Reset();
    SelectedActor.Reset();
}

AActor* FWorldEditorContext::GetSelectedActor() const noexcept { return SelectedActor.Get(); }
UActorComponent* FWorldEditorContext::GetSelectedComponent() const noexcept { return SelectedComponent.Get(); }

USceneComponent* FWorldEditorContext::GetSelectedTransformTarget() const noexcept {
    UActorComponent* Component = SelectedComponent.Get();
    if (Component != nullptr && Component->GetTypeInfo()->IsA(USceneComponent::StaticTypeInfo())) {
        return static_cast<USceneComponent*>(Component);
    }

    AActor* Actor = SelectedActor.Get();
    return Actor != nullptr ? Actor->GetRootComponent() : nullptr;
}
