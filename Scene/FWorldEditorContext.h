#pragma once

#include <d3d11.h>
#include "Core/Asset/FAssetHandle.h"
#include "Core/Base/TObjectRef.h"
#include "Core/Channel/FMessageChannel.h"
#include "Core/Channel/FStateChannel.h"
#include "Render/Panel/FEditorInfo.h"
#include "Serialize/FEditorSettings.h"

class AActor;
class FAssetRegistry;
class UActorComponent;
class USceneComponent;
class UWorld;

struct FWorldEditorSharedState {
    FEditorSettings EditorSettings{};
    size_t ModeIndex{ 0 };
};

class FWorldEditorContext {
public:
    void SetWorld(UWorld* InWorld);
    void InitializeChannels(FAssetRegistry& AssetRegistry, ID3D11Device* Device);
    void Dispatch();

    FMessageChannel::FSender GetEditorToWorldSender();
    FMessageChannel::FSender GetWorldToEditorSender();

    FEditorSettings GetEditorSettings() const;
    void SetEditorSettings(const FEditorSettings& Settings);
    void SetMoveSensitivity(float Value);
    void SetRotationSensitivity(float Value);
    void SetGridSize(float Value);
    void SetGridSnapEnabled(bool Enabled);
    void SetGridVisible(bool Visible);
    void SetAxisVisible(bool Visible);

    const size_t GetRenderModeState() const noexcept;
    void SetRenderModeState(const size_t State);

    void SetSelectedActor(AActor* Actor);
    void SetSelectedComponent(UActorComponent* Component);
    void ClearSelection();

    AActor* GetSelectedActor() const noexcept;
    UActorComponent* GetSelectedComponent() const noexcept;
    USceneComponent* GetSelectedTransformTarget() const noexcept;

    UWorld* GetWorld() const;

    // 프리뷰 대상을 바꾸면 Viewer 창을 띄워달라는 요청도 같이 세운다.
    void SetPreviewMesh(const FAssetHandle& Handle);
    FAssetHandle GetPreviewMesh() const noexcept;
    FAssetHandle ConsumePreviewMesh() noexcept;
    // 요청을 한 번만 처리하도록 읽으면서 내린다.
    bool ConsumePreviewOpenRequest() noexcept;
private:
    UWorld* World = nullptr;
    TObjectRef<AActor> SelectedActor;
    TObjectRef<UActorComponent> SelectedComponent;
    FStateChannel<FWorldEditorSharedState> SharedState{ std::in_place };
    FMessageChannel EditorToWorld{ 64 };
    FMessageChannel WorldToEditor{ 64 };

    FAssetHandle PreviewMesh{};
    bool bPreviewOpenRequested = false;
};
