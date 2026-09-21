#pragma once

#include "Render/Panel/FEditorWindow.h"
#include "Core/Asset/FAssetEntry.h"

#include "ImGui/imgui.h"

class FWorldEditorContext;
class FAssetRegistry;

// Content Browser와 같이 Asset Registry가 발견한 Content asset을 탐색하는 패널입니다.
// Registry 초기화가 완료된 뒤 생성되므로, 텍스처는 이미 생성된 GPU SRV를 그대로 썸네일로 사용합니다.
class FAssetBrowserPanel : public FEditorWindow {
public:
    explicit FAssetBrowserPanel(FAssetRegistry& InAssetRegistry, FWorldEditorContext& InEditorContext);

private:
    void DrawContents() override;
    void PushWindowStyle() override;
    void PopWindowStyle() override;

    static FString GetParentFolder(const FString& AssetPath);
    static const char* GetAssetTypeLabel(EAssetType AssetType);

    bool IsInSelectedFolder(const FAssetEntry& Entry) const;
    void DrawFolderTree(const FString& FolderPath, const char* FolderName);
    void DrawAssetTile(const FAssetEntry& Entry);

    FWorldEditorContext& EditorContext;
    FAssetRegistry* AssetRegistry{ nullptr };
    FString SelectedFolder{ "/Game" };
    FAssetHandle SelectedAsset{};
    ImGuiTextFilter AssetFilter{};
};
