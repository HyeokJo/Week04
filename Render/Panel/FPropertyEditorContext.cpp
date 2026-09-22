#include "PCH.h"
#include "FPropertyEditorContext.h"

#include "ImGui/imgui.h"
#include "Core/Asset/FAssetRegistry.h"
#include "Core/Asset/UAsset.h"
#include "Core/Base/TypeInfo.h"

#include "../../Core/Asset/UTexture.h"
#include "../../Core/Asset/UMesh.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <vector>

namespace {
    constexpr char StaticMeshAssetPayloadType[]{ "MACAW_STATIC_MESH_ASSET" };
    constexpr char MaterialAssetPayloadType[]{ "MACAW_MATERIAL_ASSET" };
    constexpr char TextureAssetPayloadType[]{ "MACAW_TEXTURE_ASSET" };

    void ApplyAssetBrowserDrop(FAssetRegistry& Registry, const FTypeInfo& AssetType, FAssetHandle CurrentHandle, const std::function<void(FAssetHandle)>& Setter) {
        const char* PayloadType{};
        if (AssetType.IsA(UMesh::StaticTypeInfo())) {
            PayloadType = StaticMeshAssetPayloadType;
        }
        else if (AssetType.IsA(UMaterial::StaticTypeInfo())) {
            PayloadType = MaterialAssetPayloadType;
        }
		else if (AssetType.IsA(UTexture::StaticTypeInfo())) {
			PayloadType = TextureAssetPayloadType;
		}

        if (PayloadType == nullptr || !ImGui::BeginDragDropTarget()) {
            return;
        }

        const ImGuiPayload* Payload{ ImGui::AcceptDragDropPayload(PayloadType) };
        if (Payload != nullptr && Payload->DataSize == sizeof(FAssetHandle)) {
            FAssetHandle DroppedHandle{};
            std::memcpy(&DroppedHandle, Payload->Data, sizeof(DroppedHandle));
            const UAsset* DroppedAsset{ Registry.ResolveAsset<UAsset>(DroppedHandle) };
            if (DroppedAsset != nullptr && DroppedAsset->GetTypeInfo()->IsA(&AssetType) && DroppedHandle != CurrentHandle) {
                Setter(DroppedHandle);
            }
        }

        ImGui::EndDragDropTarget();
    }
}

bool FPropertyEditorContext::BeginCategory(const char* Label, bool bDefaultOpen) const {
    return ImGui::CollapsingHeader(Label, bDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

void FPropertyEditorContext::DrawDisabledText(const char* Text) const {
    ImGui::TextDisabled("%s", Text);
}

void FPropertyEditorContext::DrawButton(const char* Label, const std::function<void()>& OnClicked) const {
    if (ImGui::Button(Label)) {
        OnClicked();
    }
}

void FPropertyEditorContext::DrawBool(const char* Label, bool Value, const std::function<void(bool)>& Setter) const {
    if (ImGui::Checkbox(Label, &Value)) {
        Setter(Value);
    }
}

void FPropertyEditorContext::DrawFloat(const char* Label, float Value, float Speed, float Min, float Max, const std::function<void(float)>& Setter) const {
    if (ImGui::DragFloat(Label, &Value, Speed, Min, Max)) {
        Setter(Value);
    }
}

void FPropertyEditorContext::DrawVector2(const char* Label, const FVector2& Value, float Speed, float Min, float Max, const std::function<void(const FVector2&)>& Setter) const {
	FVector2 EditedValue = Value;
	if (ImGui::DragFloat2(Label, &EditedValue.x, Speed, Min, Max)) {
		Setter(EditedValue);
	}
}

void FPropertyEditorContext::DrawVector3(const char* Label, const FVector3& Value, float Speed, float Min, float Max, const std::function<void(const FVector3&)>& Setter) const {
    FVector3 EditedValue = Value;
    if (ImGui::DragFloat3(Label, &EditedValue.x, Speed, Min, Max)) {
        Setter(EditedValue);
    }
}

void FPropertyEditorContext::DrawColor(const char* Label, const FVector4& Value, const std::function<void(const FVector4&)>& Setter) const {
    FVector4 EditedValue = Value;
    if (ImGui::ColorEdit4(Label, &EditedValue.x)) {
        Setter(EditedValue);
    }
}

void FPropertyEditorContext::DrawText(const char* Label, const FString& Value, const std::function<void(const FString&)>& Setter) const {
    std::array<char, 2048> TextBuffer{};
    const size_t CopyLength = std::min(Value.size(), TextBuffer.size() - 1);
    std::memcpy(TextBuffer.data(), Value.data(), CopyLength);
    if (ImGui::InputTextMultiline(Label, TextBuffer.data(), TextBuffer.size(), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5.0f))) {
        Setter(TextBuffer.data());
    }
}

void FPropertyEditorContext::DrawTransform(const char* Label, const FTransform& Value, const std::function<void(const FTransform&)>& Setter) {
    const ImGuiID TransformId = ImGui::GetID(Label);
    if (EditingTransformId != TransformId) {
        EditingTransformId = TransformId;
    }

    UpdateTransformFields(Value);

    bool bChanged = false;
    bChanged |= ImGui::DragFloat3("Position", &EditPosition.x, 0.1f);
	bChanged |= ImGui::DragFloat3("Rotation", &EditRotation.x, 0.5f);
	bChanged |= ImGui::DragFloat3("Scale", &EditScale.x, 0.05f, 0.001f, FLT_MAX);
	bChanged |= ImGui::Checkbox("Absolute Location", &bAbsoluteLocation);
	bChanged |= ImGui::Checkbox("Absolute Rotation", &bAbsoluteRotation);
	bChanged |= ImGui::Checkbox("Absolute Scale", &bAbsoluteScale);
    
    if (bChanged) {
        EditScale = FVector::Max(EditScale, FVector(0.001f, 0.001f, 0.001f));
        Setter(BuildDesiredTransform());
    }
}

void FPropertyEditorContext::DrawReferencePicker(const char* Label, const char* Preview, bool bNoneSelected, const std::function<void()>& ClearSelection, const std::vector<FPropertyReferenceOption>& Options) const {
    if (!ImGui::BeginCombo(Label, Preview)) {
        return;
    }
    if (ImGui::Selectable("None", bNoneSelected)) {
        ClearSelection();
    }
    for (const FPropertyReferenceOption& Option : Options) {
        ImGui::PushID(Option.Id);
        if (ImGui::Selectable(Option.Label.c_str(), Option.bSelected)) {
            Option.OnSelected();
        }
        ImGui::PopID();
    }
    ImGui::EndCombo();
}

void FPropertyEditorContext::DrawAssetPicker(const char* Label, FAssetRegistry& Registry, const FTypeInfo& AssetType, FAssetHandle CurrentHandle, const std::function<void(FAssetHandle)>& Setter) const {
    UAsset* Current = Registry.ResolveAsset<UAsset>(CurrentHandle);

    if (Current != nullptr && !Current->GetTypeInfo()->IsA(&AssetType)) {
        Current = nullptr;
    }

    const FString PreviewName = Current != nullptr ? Current->GetAssetName() : FString("None");
    const bool bSupportsThumbnail = SupportsAssetThumbnail(AssetType);

    ImGui::PushID(Label);

    if (bSupportsThumbnail) {
        ID3D11ShaderResourceView* Thumbnail = GetAssetThumbnail(Registry, CurrentHandle);

        if (Thumbnail != nullptr) {
            ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(Thumbnail)), ImVec2(48.0f, 48.0f));
        }
        else {
            ImGui::Button("##AssetThumbnail", ImVec2(48.0f, 48.0f));
        }

        ApplyAssetBrowserDrop(Registry, AssetType, CurrentHandle, Setter);
        ImGui::SameLine();
    }

    if (ImGui::BeginCombo(Label, PreviewName.c_str())) {
        ImGui::PushID("None");

        if (DrawAssetOption("None", nullptr, Current == nullptr)) {
            Setter({});
        }

        ImGui::PopID();

        std::vector<const FAssetEntry*> MatchingAssets{};
        for (const FAssetEntry& Entry : Registry.GetAssetEntries()) {
            if (Entry.Asset == nullptr || !Entry.Asset->GetTypeInfo()->IsA(&AssetType)) {
                continue;
            }

            MatchingAssets.push_back(&Entry);
        }

        std::ranges::sort(MatchingAssets, [](const FAssetEntry* Left, const FAssetEntry* Right) {
            return Left->AssetPath.Path < Right->AssetPath.Path;
        });

        for (const FAssetEntry* Entry : MatchingAssets) {
            const FString& AssetPath{ Entry->AssetPath.Path };
            const size_t NameOffset{ AssetPath.find_last_of('/') + 1 };
            const char* AssetName{ AssetPath.c_str() + NameOffset };
            ID3D11ShaderResourceView* Thumbnail{ GetAssetThumbnail(Registry, Entry->Handle) };

            ImGui::PushID(Entry->Asset.get());

            if (DrawAssetOption(AssetName, Thumbnail, Entry->Handle == CurrentHandle) && Entry->Handle != CurrentHandle) {
                Setter(Entry->Handle);
            }
            if (Entry->Handle == CurrentHandle) {
                ImGui::SetItemDefaultFocus();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", AssetPath.c_str());
            }

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    ImGui::PopID();
}

void FPropertyEditorContext::UpdateTransformFields(const FTransform& Transform) {
    EditPosition = Transform.GetPosition();
    EditRotation = Transform.GetRotation();
    EditScale = Transform.GetScale();
    bAbsoluteLocation = Transform.IsAbsoluteLocation();
    bAbsoluteRotation = Transform.IsAbsoluteRotation();
    bAbsoluteScale = Transform.IsAbsoluteScale();
}

FTransform FPropertyEditorContext::BuildDesiredTransform() const {
    FTransform Transform{ EditPosition, EditRotation, EditScale };
    Transform.SetAbsoluteLocation(bAbsoluteLocation);
    Transform.SetAbsoluteRotation(bAbsoluteRotation);
    Transform.SetAbsoluteScale(bAbsoluteScale);
    return Transform;
}

bool FPropertyEditorContext::SupportsAssetThumbnail(const FTypeInfo& AssetType) const {
    return UMesh::StaticTypeInfo()->IsA(&AssetType) || UMaterial::StaticTypeInfo()->IsA(&AssetType) || UTexture::StaticTypeInfo()->IsA(&AssetType);
}

ID3D11ShaderResourceView* FPropertyEditorContext::GetAssetThumbnail(FAssetRegistry& Registry, FAssetHandle AssetHandle) const {
    UAsset* Asset = Registry.ResolveAsset<UAsset>(AssetHandle);

    if (Asset == nullptr) {
        return nullptr;
    }

    if (Asset->GetTypeInfo()->IsA(UTexture::StaticTypeInfo())) {
        return static_cast<UTexture*>(Asset)->GetSRV();
    }

    if (ThumbnailRenderer == nullptr) {
        return nullptr;
    }

    if (Asset->GetTypeInfo()->IsA(UMesh::StaticTypeInfo()) || Asset->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo())) {
        return ThumbnailRenderer->GetThumbnail(AssetHandle);
    }

    return nullptr;
}

bool FPropertyEditorContext::DrawAssetOption(const char* Label, ID3D11ShaderResourceView* Thumbnail, bool bSelected) const {
    constexpr float RowHeight = 36.0f;
    constexpr float ImageSize = 32.0f;
    constexpr float Padding = 2.0f;

    const ImVec2 RowMin = ImGui::GetCursorScreenPos();
    const ImVec2 RowSize{ ImGui::GetContentRegionAvail().x, RowHeight };
    const bool bPressed = ImGui::Selectable("##AssetOption", bSelected, ImGuiSelectableFlags_None, RowSize);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImVec2 ImageMin{ RowMin.x + Padding, RowMin.y + Padding };
    const ImVec2 ImageMax{ ImageMin.x + ImageSize, ImageMin.y + ImageSize };

    if (Thumbnail != nullptr) {
        DrawList->AddImage(reinterpret_cast<ImTextureID>(Thumbnail), ImageMin, ImageMax);
    }
    else {
        DrawList->AddRect(ImageMin, ImageMax, ImGui::GetColorU32(ImGuiCol_Border));
    }

    const float TextY = RowMin.y + (RowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    DrawList->AddText(ImVec2(ImageMax.x + 6.0f, TextY), ImGui::GetColorU32(ImGuiCol_Text), Label);

    return bPressed;
}
