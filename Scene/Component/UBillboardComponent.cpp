#include "PCH.h"
#include "UBillboardComponent.h"

#include "Core/Asset/FAssetRegistry.h"
#include "Core/Asset/UTexture.h"

#include "Render/Panel/FPropertyEditorContext.h"
#include "Render/Pipeline/UPipeline.h"

#include "Scene/AActor.h"
#include "Scene/UWorld.h"
#include "Scene/Subsystem/UBillboardSubsystem.h"

bool UBillboardComponent::CanRenderBillBoard() const
{
    return IsActive() && IsVisible();
}

void UBillboardComponent::Serialize(FArchive& Archive)
{
    UPrimitiveComponent::Serialize(Archive);

    FAssetRegistry* Registry = Archive.GetAssetRegistry();
    if (Archive.IsSaving() && Registry != nullptr) {
        if (const FAssetPath* AssetPath = Registry->GetAssetPath(TextureHandle)) {
            TextureAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid = Registry->GetAssetGuid(TextureHandle)) {
            TextureAssetGuid = *AssetGuid;
        }
        if (const FAssetPath* AssetPath = Registry->GetAssetPath(PipelineHandle)) {
            PipelineAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid = Registry->GetAssetGuid(PipelineHandle)) {
            PipelineAssetGuid = *AssetGuid;
        }
    }
    Archive.Serialize("TextureAssetGuid", TextureAssetGuid);
    Archive.Serialize("TextureAssetPath", TextureAssetPath.Path);
    Archive.Serialize("PipelineAssetGuid", PipelineAssetGuid);
    Archive.Serialize("PipelineAssetPath", PipelineAssetPath.Path);
    if (Archive.IsLoading()) {
        TextureHandle = Registry != nullptr ? Registry->FindAsset(TextureAssetGuid) : FAssetHandle{};
        if (!TextureHandle && Registry != nullptr) {
            TextureHandle = Registry->FindAsset(TextureAssetPath);
        }
        PipelineHandle = Registry != nullptr ? Registry->FindAsset(PipelineAssetGuid) : FAssetHandle{};
        if (!PipelineHandle && Registry != nullptr) {
            PipelineHandle = Registry->FindAsset(PipelineAssetPath);
        }
    }

    Archive.Serialize("Size", Size);
    Archive.Serialize("UVMin", UVMin);
    Archive.Serialize("UVMax", UVMax);
    Archive.Serialize("Color", Color);
}

bool UBillboardComponent::TryGetBillBoardWorld(FMatrix& OutWorld) const
{
    if (!CanRenderBillBoard())
    {
        return false;
    }

    OutWorld = GetComponentToWorld();
    return true;
}

void UBillboardComponent::SetTextureHandle(FAssetHandle InTextureHandle)
{
    TextureHandle = InTextureHandle;
    UWorld* World = GetBelongingWorld();
    FAssetRegistry* Registry = World != nullptr ? World->GetAssetRegistry() : nullptr;
    TextureAssetPath = Registry != nullptr && Registry->GetAssetPath(TextureHandle) != nullptr ? *Registry->GetAssetPath(TextureHandle) : FAssetPath{};
    TextureAssetGuid = Registry != nullptr && Registry->GetAssetGuid(TextureHandle) != nullptr ? *Registry->GetAssetGuid(TextureHandle) : FGuid{};
}

void UBillboardComponent::SetPipelineHandle(FAssetHandle InPipelineHandle)
{
    PipelineHandle = InPipelineHandle;
    UWorld* World = GetBelongingWorld();
    FAssetRegistry* Registry = World != nullptr ? World->GetAssetRegistry() : nullptr;
    PipelineAssetPath = Registry != nullptr && Registry->GetAssetPath(PipelineHandle) != nullptr ? *Registry->GetAssetPath(PipelineHandle) : FAssetPath{};
    PipelineAssetGuid = Registry != nullptr && Registry->GetAssetGuid(PipelineHandle) != nullptr ? *Registry->GetAssetGuid(PipelineHandle) : FGuid{};
}

void UBillboardComponent::SetSize(const FVector2& InSize)
{
    Size = InSize;
}

void UBillboardComponent::SetUV(const FVector2& InUVMin, const FVector2& InUVMax)
{
    UVMin = InUVMin;
    UVMax = InUVMax;
}

void UBillboardComponent::SetColor(const FVector4& InColor)
{
    Color = InColor;
}

FAssetHandle UBillboardComponent::GetTextureHandle() const
{
    return TextureHandle;
}

FAssetHandle UBillboardComponent::GetPipelineHandle() const
{
    return PipelineHandle;
}

const FVector2& UBillboardComponent::GetSize() const
{
    return Size;
}

const FVector2& UBillboardComponent::GetUVmin() const
{
    return UVMin;
}

const FVector2& UBillboardComponent::GetUVMax() const
{
    return UVMax;
}

const FVector4& UBillboardComponent::GetColor() const
{
    return Color;
}

bool UBillboardComponent::MakeBillboardRender(FBillboardProbe& OutProbe) const
{
    if (!TextureHandle || !PipelineHandle)
    {
        return false;
    }

    if (!TryGetBillBoardWorld(OutProbe.World))
    {
        return false;
    }

    OutProbe.TextureHandle = TextureHandle;
    OutProbe.PipelineHandle = PipelineHandle;
    OutProbe.Size = Size;
    OutProbe.UVMin = UVMin;
    OutProbe.UVMax = UVMax;
    OutProbe.Color = Color;

    return true;
}

void UBillboardComponent::DrawPanels(FPropertyEditorContext& Context)
{
	UPrimitiveComponent::DrawPanels(Context);
    Context.DrawColor("Color", GetColor(), [this](const FVector4& NewColor) { SetColor(NewColor);});

    AActor* Owner = GetOwner();
    UWorld* World = Owner != nullptr ? Owner->GetWorld() : nullptr;
    FAssetRegistry* Registry = World != nullptr ? World->GetAssetRegistry() : nullptr;

    Context.DrawAssetPicker("Texture", *Registry, *UTexture::StaticTypeInfo(), GetTextureHandle(), [this](FAssetHandle NewHandle) { SetTextureHandle(NewHandle); });
    Context.DrawAssetPicker("Pipeline", *Registry, *UPipeline::StaticTypeInfo(), GetPipelineHandle(), [this](FAssetHandle NewHandle) { SetPipelineHandle(NewHandle); });
}

void UBillboardComponent::OnRegister()
{
    UPrimitiveComponent::OnRegister();

    UWorld* World = GetBelongingWorld();
    if (World != nullptr)
    {
        World->GetBillboardSubsystem().RegisterComponent(this);
    }
}

void UBillboardComponent::OnUnregister()
{
    UWorld* World = GetBelongingWorld();
    if (World != nullptr)
    {
        World->GetBillboardSubsystem().UnregisterComponent(this);
    }

    UPrimitiveComponent::OnUnregister();
}
