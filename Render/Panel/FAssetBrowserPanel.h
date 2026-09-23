#pragma once

#include "Render/Panel/FEditorWindow.h"
#include "Core/Asset/FAssetEntry.h"
#include "../EditorView/FAssetThumbnailRenderer.h"

#include "ImGui/imgui.h"
#include <functional>

class FWorldEditorContext;
class FAssetRegistry;

class FAssetBrowserPanel : public FEditorWindow {
public:
    explicit FAssetBrowserPanel(FAssetRegistry& InAssetRegistry, FWorldEditorContext& InEditorContext, FAssetThumbnailRenderer* InThumbnailRenderer, std::function<void(FAssetHandle)> InOpenMaterialEditor);

    void BeginExternalDropFrame();
    bool HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition);

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
    EAssetType mSelectedAssetType{ EAssetType::END };
    ImVec2 DropTargetMin{};
    ImVec2 DropTargetMax{};
    bool bDropTargetActive{ false };

	FAssetThumbnailRenderer* ThumbnailRenderer{ nullptr };
	std::function<void(FAssetHandle)> mOpenMaterialEditor{};
};
