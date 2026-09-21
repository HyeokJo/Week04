#include "PCH.h"

#include "Render/Panel/FAssetBrowserPanel.h"

#include "Core/Asset/FAssetRegistry.h"
#include "Core/Asset/UTexture.h"
#include "Core/Console/Console.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string_view>

namespace {
    constexpr float FolderPaneWidth = 190.0f;
    constexpr float ThumbnailSize = 96.0f;
    constexpr float TileWidth = ThumbnailSize + 18.0f;

    FString OpenFileDialog(const FString& FilePath, const OPENFILENAMEA& OFN)
    {
        char FileName[MAX_PATH] = { 0 };

        OPENFILENAMEA OpenFileName = OFN;

        OpenFileName.lpstrFile = FileName;

        std::string InitialDirectoryPath = std::filesystem::absolute(FilePath.c_str()).string();

        if (!std::filesystem::exists(InitialDirectoryPath))
        {
            std::filesystem::create_directories(InitialDirectoryPath);
        }

        OpenFileName.lpstrInitialDir = InitialDirectoryPath.c_str();

        if (GetOpenFileNameA(&OpenFileName))
        {
            return FString(FileName);
        }

        return "";
    }


}

FAssetBrowserPanel::FAssetBrowserPanel(FAssetRegistry& InAssetRegistry)
    : FEditorWindow("Content Browser###AssetBrowserPanel")
    , AssetRegistry(&InAssetRegistry) {
}

void FAssetBrowserPanel::BeginExternalDropFrame() {
    bDropTargetActive = false;
}

bool FAssetBrowserPanel::HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition) {
    if (!bDropTargetActive || AssetRegistry == nullptr ||
        ScreenPosition.x < DropTargetMin.x || ScreenPosition.x >= DropTargetMax.x ||
        ScreenPosition.y < DropTargetMin.y || ScreenPosition.y >= DropTargetMax.y) {
        return false;
    }

    FString Extension = FilePath.extension().generic_string().c_str();
    std::ranges::transform(Extension, Extension.begin(), [](unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });

    if (Extension != ".obj") {
        Console::AddLog(
            Console::STDOutHandle,
            ELogLevel::Warning,
            ELogCategory::Etc,
            "Only OBJ files can be dropped into the Asset Browser: %s",
            FilePath.generic_string().c_str());
        return true;
    }

    const FAssetHandle ImportedHandle = AssetRegistry->ImportMesh(FilePath, SelectedFolder);
    if (ImportedHandle) {
        SelectedAsset = ImportedHandle;
        Console::AddLog(
            Console::STDOutHandle,
            ELogLevel::Log,
            ELogCategory::Etc,
            "Imported dropped OBJ into %s: %s",
            SelectedFolder.c_str(),
            FilePath.generic_string().c_str());
    }
    else {
        Console::AddLog(
            Console::STDOutHandle,
            ELogLevel::Error,
            ELogCategory::Etc,
            "Failed to import dropped OBJ into %s: %s",
            SelectedFolder.c_str(),
            FilePath.generic_string().c_str());
    }

    return true;
}

void FAssetBrowserPanel::DrawContents() {
    const ImVec2 WindowPosition = ImGui::GetWindowPos();
    const ImVec2 WindowSize = ImGui::GetWindowSize();
    DropTargetMin = WindowPosition;
    DropTargetMax = ImVec2(WindowPosition.x + WindowSize.x, WindowPosition.y + WindowSize.y);
    bDropTargetActive = true;

    ImGui::TextDisabled("Content");
    ImGui::SameLine();
    ImGui::TextUnformatted(SelectedFolder.c_str());
    ImGui::SameLine();
    
    if (ImGui::Button("Import")) {

        OPENFILENAMEA OpenFileName = { 0 };

        OpenFileName.lStructSize = sizeof(OpenFileName);
        OpenFileName.hwndOwner = nullptr;
        OpenFileName.lpstrFilter = "OBJ Files(*.obj)\0*.obj\0All Files(*.*)\0*.*\0";
        OpenFileName.nMaxFile = MAX_PATH;
        OpenFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        OpenFileName.lpstrDefExt = "obj";

        FString FilePath = OpenFileDialog(FString("./Content"), OpenFileName);

        if (!FilePath.empty()) {
            const std::filesystem::path SourcePath{ FilePath };
            AssetRegistry->ImportMesh(SourcePath, SelectedFolder);
        }
    }
   
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##AssetFilter", "Search assets", AssetFilter.InputBuf, IM_ARRAYSIZE(AssetFilter.InputBuf))) {
        AssetFilter.Build();
    }
    ImGui::Separator();

    const float ContentHeight = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginChild("AssetFolders", ImVec2(FolderPaneWidth, ContentHeight), ImGuiChildFlags_Borders)) {
        DrawFolderTree("/Game", "Content");
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::BeginChild("AssetTiles", ImVec2(0.0f, ContentHeight), ImGuiChildFlags_Borders)) {
        std::vector<const FAssetEntry*> VisibleAssets{};
        for (const FAssetEntry& Entry : AssetRegistry->GetAssetEntries()) {
            if (!IsInSelectedFolder(Entry)) {
                continue;
            }

            const std::string_view AssetPath{ Entry.AssetPath.Path.data(), Entry.AssetPath.Path.size() };
            if (!AssetFilter.PassFilter(AssetPath.data(), AssetPath.data() + AssetPath.size())) {
                continue;
            }

            VisibleAssets.push_back(&Entry);
        }

        std::ranges::sort(VisibleAssets, [](const FAssetEntry* Left, const FAssetEntry* Right) {
            return Left->AssetPath.Path < Right->AssetPath.Path;
        });

        const int ColumnCount = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / TileWidth));
        if (ImGui::BeginTable("AssetGrid", ColumnCount, ImGuiTableFlags_SizingFixedFit)) {
            for (int AssetIndex = 0; AssetIndex < static_cast<int>(VisibleAssets.size()); ++AssetIndex) {
                ImGui::TableNextColumn();
                DrawAssetTile(*VisibleAssets[AssetIndex]);
            }
            ImGui::EndTable();
        }

        if (VisibleAssets.empty()) {
            ImGui::TextDisabled("No assets in this folder.");
        }
    }
    ImGui::EndChild();
}

void FAssetBrowserPanel::PushWindowStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.075f, 0.080f, 0.095f, 1.0f));
}

void FAssetBrowserPanel::PopWindowStyle() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

FString FAssetBrowserPanel::GetParentFolder(const FString& AssetPath) {
    const size_t SlashIndex = AssetPath.find_last_of('/');
    return SlashIndex == FString::npos ? FString{} : AssetPath.substr(0, SlashIndex);
}

const char* FAssetBrowserPanel::GetAssetTypeLabel(EAssetType AssetType) {
    switch (AssetType) {
    case EAssetType::Texture:  return "Texture";
    case EAssetType::Pipeline: return "Pipeline";
    case EAssetType::Material: return "Material";
    case EAssetType::Mesh:     return "Static Mesh";
    case EAssetType::Font:     return "Font";
    default:                   return "Asset";
    }
}

bool FAssetBrowserPanel::IsInSelectedFolder(const FAssetEntry& Entry) const {
    // Content 최상위는 Unreal의 All Assets 보기처럼 하위 폴더의 asset까지 모두 표시한다.
    if (SelectedFolder == "/Game") {
        return true;
    }

    return GetParentFolder(Entry.AssetPath.Path) == SelectedFolder;
}

void FAssetBrowserPanel::DrawFolderTree(const FString& FolderPath, const char* FolderName) {
    std::vector<FString> ChildFolderNames{};
    const std::string_view FolderPathView{ FolderPath.data(), FolderPath.size() };

    for (const FAssetEntry& Entry : AssetRegistry->GetAssetEntries()) {
        const std::string_view AssetPath{ Entry.AssetPath.Path.data(), Entry.AssetPath.Path.size() };
        if (!AssetPath.starts_with(FolderPathView) || AssetPath.size() <= FolderPathView.size() || AssetPath[FolderPathView.size()] != '/') {
            continue;
        }

        const std::string_view RemainingPath = AssetPath.substr(FolderPathView.size() + 1);
        const size_t SeparatorIndex = RemainingPath.find('/');
        if (SeparatorIndex == std::string_view::npos) {
            continue;
        }

        const FString ChildName{ RemainingPath.substr(0, SeparatorIndex).data(), SeparatorIndex };
        if (std::ranges::find(ChildFolderNames, ChildName) == ChildFolderNames.end()) {
            ChildFolderNames.push_back(ChildName);
        }
    }

    std::ranges::sort(ChildFolderNames);
    const bool bHasChildren = !ChildFolderNames.empty();
    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow;
    if (!bHasChildren) {
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (SelectedFolder == FolderPath) {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool bOpen = ImGui::TreeNodeEx(FolderPath.c_str(), Flags, "%s", FolderName);
    if (ImGui::IsItemClicked()) {
        SelectedFolder = FolderPath;
    }

    if (bHasChildren && bOpen) {
        for (const FString& ChildName : ChildFolderNames) {
            DrawFolderTree(FolderPath + "/" + ChildName, ChildName.c_str());
        }
        ImGui::TreePop();
    }
}

void FAssetBrowserPanel::DrawAssetTile(const FAssetEntry& Entry) {
    const FString& AssetPath = Entry.AssetPath.Path;
    const size_t NameOffset = AssetPath.find_last_of('/') + 1;
    const char* AssetName = AssetPath.c_str() + NameOffset;
    const bool bSelected = SelectedAsset == Entry.Handle;

    ImGui::PushID(static_cast<int>(Entry.Handle.ID));
    if (bSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.36f, 0.66f, 1.0f));
    }

    bool bClicked = false;
    if (Entry.AssetType == EAssetType::Texture) {
        if (const UTexture* Texture = AssetRegistry->ResolveAsset<UTexture>(Entry.Handle);
            Texture != nullptr && Texture->GetSRV() != nullptr) {
            const ImTextureID TextureId = reinterpret_cast<ImTextureID>(Texture->GetSRV());
            bClicked = ImGui::ImageButton("##Thumbnail", ImTextureRef(TextureId), ImVec2(ThumbnailSize, ThumbnailSize));
        }
    }

    if (Entry.AssetType != EAssetType::Texture || !Entry.Asset) {
        bClicked = ImGui::Button(GetAssetTypeLabel(Entry.AssetType), ImVec2(ThumbnailSize, ThumbnailSize));
    }

    if (bSelected) {
        ImGui::PopStyleColor();
    }
    if (bClicked) {
        SelectedAsset = Entry.Handle;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s\n%s", AssetPath.c_str(), GetAssetTypeLabel(Entry.AssetType));
    }

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ThumbnailSize);
    ImGui::TextUnformatted(AssetName);
    ImGui::PopTextWrapPos();
    ImGui::TextDisabled("%s", GetAssetTypeLabel(Entry.AssetType));
    ImGui::PopID();
}

