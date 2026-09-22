#include "PCH.h"
#include "FMaterialEditorPanel.h"

#include "Core/Asset/FAssetRegistry.h"
#include "Core/Asset/USurfaceOpaque.h"
#include "Core/Asset/UTexture.h"
#include "Render/EditorView/FAssetThumbnailRenderer.h"
#include "Render/Renderer.h"

#include <algorithm>
#include <array>

namespace {
	constexpr uint32 PreviewSize{ 512 };
	struct FTextureField {
		const char* Label{};
		FMaterialTextureMap FMaterialGroup::* Member{};
	};
	constexpr std::array<FTextureField, 12> TextureFields{{
		{ "Ambient", &FMaterialGroup::AmbientTexture },
		{ "Diffuse", &FMaterialGroup::DiffuseTexture },
		{ "Specular", &FMaterialGroup::SpecularTexture },
		{ "Emissive", &FMaterialGroup::EmissiveTexture },
		{ "Transmission", &FMaterialGroup::TransmissionTexture },
		{ "Shininess", &FMaterialGroup::ShininessTexture },
		{ "Opacity", &FMaterialGroup::OpacityTexture },
		{ "Bump", &FMaterialGroup::BumpTexture },
		{ "Normal", &FMaterialGroup::NormalTexture },
		{ "Displacement", &FMaterialGroup::DisplacementTexture },
		{ "Decal", &FMaterialGroup::DecalTexture },
		{ "Reflection", &FMaterialGroup::ReflectionTexture }
	}};
}

FMaterialEditorPanel::FMaterialEditorPanel(FAssetRegistry& InRegistry, FAssetThumbnailRenderer& InThumbnailRenderer)
	: FEditorWindow("Material Editor###MaterialEditor", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking)
	, mRegistry(InRegistry)
	, mThumbnailRenderer(InThumbnailRenderer) {
	SetVisible(false);
	mPropertyEditor.BindThumbnailRenderer(&InThumbnailRenderer);
}

FMaterialEditorPanel::~FMaterialEditorPanel() = default;

void FMaterialEditorPanel::OpenMaterial(FAssetHandle MaterialHandle) {
	if (mRegistry.ResolveAsset<USurfaceOpaque>(MaterialHandle) == nullptr) {
		return;
	}

	mMaterialHandle = MaterialHandle;
	mPreviewDirty = true;
	SetVisible(true);
}

void FMaterialEditorPanel::RenderOffscreen(FRenderer& Renderer, FAssetRegistry&) {
	if (!mPreviewDirty || mRegistry.ResolveAsset<USurfaceOpaque>(mMaterialHandle) == nullptr) {
		return;
	}

	if (!mPreviewSurface.IsValid()) {
		mPreviewSurface.InitializeOffscreen(Renderer.GetDevice(), PreviewSize, PreviewSize);
	}

	if (mPreviewSurface.IsValid()) {
		mThumbnailRenderer.RenderThumbnail(mMaterialHandle);
		mThumbnailRenderer.RenderMaterialPreview(mMaterialHandle, mPreviewSurface);
		mPreviewDirty = false;
	}
}

void FMaterialEditorPanel::ReleaseRenderResources() {
	mPreviewSurface.Reset();
	mPreviewDirty = true;
}

void FMaterialEditorPanel::DrawContents() {
	USurfaceOpaque* Material{ mRegistry.ResolveAsset<USurfaceOpaque>(mMaterialHandle) };
	if (Material == nullptr) {
		ImGui::TextDisabled("Material unavailable.");
		return;
	}

	const FAssetPath* AssetPath{ mRegistry.GetAssetPath(mMaterialHandle) };
	ImGui::TextUnformatted(AssetPath != nullptr ? AssetPath->Path.c_str() : Material->GetAssetName().c_str());

	const float PreviewWidth{ std::min(ImGui::GetContentRegionAvail().x, 400.0f) };
	if (ID3D11ShaderResourceView* Preview{ mPreviewSurface.GetShaderResourceView() }; Preview != nullptr) {
		ImGui::Image(ImTextureRef{ reinterpret_cast<ImTextureID>(Preview) }, ImVec2{ PreviewWidth, PreviewWidth });
	}
	else {
		ImGui::Dummy(ImVec2{ PreviewWidth, PreviewWidth });
	}

	ImGui::Separator();
	const TArray<FMaterialGroup>& Groups{ Material->GetGroups() };
	for (uint32 GroupIndex{}; GroupIndex < Groups.size(); ++GroupIndex) {
		ImGui::PushID(static_cast<int>(GroupIndex));
		const FString& GroupName{ Groups[GroupIndex].Name };
		if (ImGui::CollapsingHeader(GroupName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawGroup(*Material, GroupIndex, Groups[GroupIndex]);
		}
		ImGui::PopID();
	}
}

void FMaterialEditorPanel::DrawGroup(USurfaceOpaque& Material, uint32 GroupIndex, const FMaterialGroup& Group) {
	const auto DrawColor3{ [this, &Material, GroupIndex](const char* Label, const FVector3& Value, FVector3 FMaterialGroup::* Member) {
		const FVector4 Color{ Value.x, Value.y, Value.z, 1.0f };
		mPropertyEditor.DrawColor(Label, Color, [this, &Material, GroupIndex, Member](const FVector4& Edited) {
			ModifyGroup(Material, GroupIndex, [Member, &Edited](FMaterialGroup& Target) {
				Target.*Member = FVector3{ Edited.x, Edited.y, Edited.z };
			});
		});
	}};
	DrawColor3("Ambient", Group.Ambient, &FMaterialGroup::Ambient);
	DrawColor3("Diffuse", Group.Diffuse, &FMaterialGroup::Diffuse);
	DrawColor3("Specular", Group.Specular, &FMaterialGroup::Specular);
	DrawColor3("Emissive", Group.Emissive, &FMaterialGroup::Emissive);
	mPropertyEditor.DrawVector3("Transmission Filter", Group.TransmissionFilter, 0.01f, 0.0f, 1.0f, [this, &Material, GroupIndex](const FVector3& Value) {
		ModifyGroup(Material, GroupIndex, [&Value](FMaterialGroup& Target) { Target.TransmissionFilter = Value; });
	});
	mPropertyEditor.DrawFloat("Shininess", Group.Shininess, 1.0f, 0.0f, 1000.0f, [this, &Material, GroupIndex](float Value) {
		ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) { Target.Shininess = Value; });
	});
	mPropertyEditor.DrawFloat("Refraction Index", Group.RefractionIndex, 0.01f, 0.0f, 10.0f, [this, &Material, GroupIndex](float Value) {
		ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) { Target.RefractionIndex = Value; });
	});
	mPropertyEditor.DrawFloat("Opacity", Group.Opacity, 0.01f, 0.0f, 1.0f, [this, &Material, GroupIndex](float Value) {
		ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) { Target.Opacity = Value; });
	});
	mPropertyEditor.DrawFloat("Sharpness", Group.Sharpness, 1.0f, 0.0f, 1000.0f, [this, &Material, GroupIndex](float Value) {
		ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) { Target.Sharpness = Value; });
	});
	int IlluminationModel{ Group.IlluminationModel };
	if (ImGui::InputInt("Illumination Model", &IlluminationModel)) {
		ModifyGroup(Material, GroupIndex, [IlluminationModel](FMaterialGroup& Target) { Target.IlluminationModel = IlluminationModel; });
	}
	mPropertyEditor.DrawBool("Dissolve Halo", Group.bDissolveHalo, [this, &Material, GroupIndex](bool Value) {
		ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) { Target.bDissolveHalo = Value; });
	});
	ImGui::SeparatorText("Textures");
	for (const FTextureField& Field : TextureFields) {
		DrawTexture(Material, GroupIndex, Field.Label, Field.Member);
	}
}

void FMaterialEditorPanel::DrawTexture(USurfaceOpaque& Material, uint32 GroupIndex, const char* Label, FMaterialTextureMap FMaterialGroup::* Member) {
	const FAssetHandle CurrentHandle{ (Material.GetGroups()[GroupIndex].*Member).Texture };
	mPropertyEditor.DrawAssetPicker(Label, mRegistry, *UTexture::StaticTypeInfo(), CurrentHandle, [this, &Material, GroupIndex, Member](FAssetHandle NewHandle) {
		ModifyGroup(Material, GroupIndex, [Member, NewHandle](FMaterialGroup& Group) {
			FMaterialTextureMap& Map{ Group.*Member };
			Map.Texture = NewHandle;
			Map.SourcePath.clear();
		});
	});
}

void FMaterialEditorPanel::ModifyGroup(USurfaceOpaque& Material, uint32 GroupIndex, const std::function<void(FMaterialGroup&)>& Modifier) {
	if (Material.ModifyGroup(GroupIndex, Modifier)) {
		mPreviewDirty = true;
	}
}
