#include "PCH.h"
#include "FViewerPanel.h"

#include "ImGui/imgui.h"
#include "Render/Renderer.h"
#include "Core/Asset/FAssetRegistry.h"
#include "Core/Asset/UMaterial.h"
#include "Core/Asset/UMesh.h"
#include "Core/Asset/USurfaceOpaque.h"
#include "Core/Console/Console.h"
#include "Render/Pipeline/UPipeline.h"
#include "Scene/FWorldEditorContext.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float MinimumDistance{ 0.10f };
    constexpr float MaximumDistance{ 1000000.0f };
    constexpr char DefaultMeshPath[]{ "/Game/System/Mesh/Cube.bin" };
    constexpr char DefaultMaterialPath[]{ "/Game/System/Material/Green.mtl" };
    constexpr char DefaultPipelinePath[]{ "/Game/Pipeline/Base" };
    constexpr char TexturedPipelinePath[]{ "/Game/Pipeline/TexturedBase.json" };
}

FViewerPanel::FViewerPanel(FAssetRegistry& InRegistry, HWND InputWindowHandle, FMessageChannel::FSender InEditorToWorldSender, FWorldEditorContext& InEditorContext, FAssetThumbnailRenderer* InThumbnailRenderer)
    : FEditorWindow("Viewer", ImGuiWindowFlags_MenuBar)
    , mRegistry(&InRegistry)
    , mEditorToWorldSender(std::move(InEditorToWorldSender))
    , mWindowHandle(InputWindowHandle)
    , mEditorContext(InEditorContext) {
    mPropertyEditor.BindThumbnailRenderer(InThumbnailRenderer);
    SetMesh({});
    SetMaterial({});
}

void FViewerPanel::SetMesh(FAssetHandle InMeshHandle) {
    mMeshHandle = mRegistry->ResolveAsset<UMesh>(InMeshHandle) != nullptr ? InMeshHandle : mRegistry->FindAsset(FAssetPath{ DefaultMeshPath });
}

void FViewerPanel::SetMaterial(FAssetHandle InMaterialHandle) {
    mMaterialHandle = mRegistry->ResolveAsset<UMaterial>(InMaterialHandle) != nullptr ? InMaterialHandle : mRegistry->FindAsset(FAssetPath{ DefaultMaterialPath });
    if (mRegistry->ResolveAsset<UMaterial>(mMaterialHandle) == nullptr) {
        mMaterialHandle = mRegistry->EnsureDefaultStaticMeshMaterial();
    }
}

bool FViewerPanel::OpenViewerFile(const std::filesystem::path& FilePath) {
    if (mRegistry == nullptr) {
        return false;
    }

    std::string Extension{ FilePath.extension().string() };
    std::ranges::transform(Extension, Extension.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
    if (Extension != ".obj" && Extension != ".bin" && Extension != ".mtl") {
        return false;
    }

    const FAssetHandle Handle{ mRegistry->LoadViewerAsset(FilePath) };
    if (Extension == ".mtl" && mRegistry->ResolveAsset<UMesh>(mMeshHandle) != nullptr && mRegistry->ResolveAsset<UMaterial>(Handle) != nullptr) {
        SetMaterial(Handle);
        return true;
    }
    if (Extension != ".mtl" && mRegistry->ResolveAsset<UMesh>(Handle) != nullptr) {
        SetMesh(Handle);
        const UMesh* Mesh{ mRegistry->ResolveAsset<UMesh>(mMeshHandle) };
        const uint32 VertexCount{ Mesh->GetVertexAttributeCount(EVertexAttribute::Position) };
        const uint32 VertexStride{ Mesh->GetVertexStride(EVertexAttribute::Position) };
        const std::byte* VertexData{ static_cast<const std::byte*>(Mesh->GetVertexData(EVertexAttribute::Position)) };
        if (VertexData != nullptr && VertexCount > 0 && VertexStride >= sizeof(FVector3)) {
            FVector3 Minimum{ *reinterpret_cast<const FVector3*>(VertexData) };
            FVector3 Maximum{ Minimum };
            for (uint32 Index{ 1 }; Index < VertexCount; ++Index) {
                const FVector3& Position{ *reinterpret_cast<const FVector3*>(VertexData + static_cast<size_t>(Index) * VertexStride) };
                Minimum.x = std::min(Minimum.x, Position.x);
                Minimum.y = std::min(Minimum.y, Position.y);
                Minimum.z = std::min(Minimum.z, Position.z);
                Maximum.x = std::max(Maximum.x, Position.x);
                Maximum.y = std::max(Maximum.y, Position.y);
                Maximum.z = std::max(Maximum.z, Position.z);
            }
            mTarget = (Minimum + Maximum) * 0.5f;
            mDistance = std::clamp((Maximum - Minimum).Length() / std::tan(mFieldOfView * 0.5f), MinimumDistance, MaximumDistance);
        }
        return true;
    }
    return false;
}

bool FViewerPanel::HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition) {
    if (!IsVisible() || ScreenPosition.x < mDropTargetMin.x || ScreenPosition.x >= mDropTargetMax.x || ScreenPosition.y < mDropTargetMin.y || ScreenPosition.y >= mDropTargetMax.y) {
        return false;
    }
    return OpenViewerFile(FilePath);
}

void FViewerPanel::DrawContents() {
    mDropTargetMin = ImGui::GetWindowPos();
    const ImVec2 WindowSize{ ImGui::GetWindowSize() };
    mDropTargetMax = ImVec2{ mDropTargetMin.x + WindowSize.x, mDropTargetMin.y + WindowSize.y };
    if (const FAssetHandle PreviewMesh{ mEditorContext.ConsumePreviewMesh() }; PreviewMesh) {
        SetMesh(PreviewMesh);
    }

    DrawMenuBar();
    DrawProperties();
    ImGui::Separator();
    DrawPreview();
}

void FViewerPanel::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
#ifdef OBJ_VIEWER
        if (ImGui::MenuItem("Open Model or Material...")) {
#else
        if (ImGui::MenuItem("Import OBJ...")) {
#endif
            OPENFILENAMEA OpenFileName{};
            OpenFileName.lStructSize = sizeof(OpenFileName);
            OpenFileName.hwndOwner = mWindowHandle;
#ifdef OBJ_VIEWER
            OpenFileName.lpstrFilter = "Model and Material Files\0*.bin;*.obj;*.mtl\0All Files\0*.*\0";
#else
            OpenFileName.lpstrFilter = "OBJ Files(*.obj)\0*.obj\0All Files(*.*)\0*.*\0";
            OpenFileName.lpstrDefExt = "obj";
#endif
            OpenFileName.nMaxFile = MAX_PATH;
            OpenFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

            FString FilePath{ OpenFileDialog(FString{ "./Content/ModelingFiles" }, OpenFileName) };
            if (!FilePath.empty()) {
#ifdef OBJ_VIEWER
                OpenViewerFile(std::filesystem::path{ FilePath.c_str() });
#else
                mEditorToWorldSender.TryEmplace<FMessageImportMesh>(FString{ "ObjImport" }, std::move(FilePath), FString{});
#endif
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void FViewerPanel::DrawProperties() {
    if (mRegistry == nullptr) {
        mPropertyEditor.DrawDisabledText("Asset registry unavailable");
        return;
    }

    ImGui::TextUnformatted("Preview Assets");
    mPropertyEditor.DrawAssetPicker("StaticMesh", *mRegistry, *UMesh::StaticTypeInfo(), mMeshHandle, [this](FAssetHandle Handle) {
        SetMesh(Handle);
    });
    mPropertyEditor.DrawAssetPicker("Material", *mRegistry, *UMaterial::StaticTypeInfo(), mMaterialHandle, [this](FAssetHandle Handle) {
        SetMaterial(Handle);
    });
}

void FViewerPanel::ResizeSurfaceIfNeeded(ID3D11Device* Device, uint32 Width, uint32 Height) {
    if (Device == nullptr || Width == 0 || Height == 0) {
        return;
    }

    if (!mSurface.IsValid()) {
        mSurface.InitializeOffscreen(Device, Width, Height);
    }
    else if (Width != mSurfaceWidth || Height != mSurfaceHeight) {
        mSurface.Resize(Device, Width, Height);
    }

    mSurfaceWidth = Width;
    mSurfaceHeight = Height;
}

FMatrix FViewerPanel::MakeCameraWorldMatrix(const FVector3& Eye) const {
    FVector3 Forward{ mTarget - Eye };
    Forward.Normalize();
    FVector3 Right{ FVector3{ 0.0f, 0.0f, 1.0f }.Cross(Forward) };
    Right.Normalize();
    FVector3 Up{ Forward.Cross(Right) };
    Up.Normalize();

    FMatrix Result{ FMatrix::Identity };
    Result.m[0][0] = Right.x;
    Result.m[0][1] = Right.y;
    Result.m[0][2] = Right.z;
    Result.m[1][0] = Up.x;
    Result.m[1][1] = Up.y;
    Result.m[1][2] = Up.z;
    Result.m[2][0] = Forward.x;
    Result.m[2][1] = Forward.y;
    Result.m[2][2] = Forward.z;
    Result.m[3][0] = Eye.x;
    Result.m[3][1] = Eye.y;
    Result.m[3][2] = Eye.z;
    return Result;
}

FRenderProbe FViewerPanel::BuildPreviewProbe() {
    FRenderProbe Probe{};
    if (mRegistry == nullptr || mSurfaceWidth == 0 || mSurfaceHeight == 0) {
        return Probe;
    }

    if (mRegistry->ResolveAsset<UMesh>(mMeshHandle) == nullptr) {
        SetMesh({});
    }
    if (mRegistry->ResolveAsset<UMaterial>(mMaterialHandle) == nullptr) {
        SetMaterial({});
    }

    bool HasTexture{};
    if (const USurfaceOpaque* Material{ mRegistry->ResolveAsset<USurfaceOpaque>(mMaterialHandle) }; Material != nullptr) {
        for (uint32 GroupIndex{}; GroupIndex < Material->GetGroups().size() && !HasTexture; ++GroupIndex) {
            const FMaterialChunkSignature Signature{ Material->BuildChunkSignature(GroupIndex) };
            for (uint8 TextureIndex{}; TextureIndex < Signature.TextureFieldCount; ++TextureIndex) {
                HasTexture = HasTexture || static_cast<bool>(Signature.GetTextureHandle(TextureIndex));
            }
        }
    }
    const FAssetHandle PipelineHandle{ mRegistry->FindAsset(FAssetPath{ HasTexture ? TexturedPipelinePath : DefaultPipelinePath }) };
    UPipeline* Pipeline{ mRegistry->ResolveAsset<UPipeline>(PipelineHandle) };
    if (mMeshHandle && mMaterialHandle && Pipeline != nullptr) {
        Pipeline->SetRenderMode(ERenderMode::Lit);
        FActorProbe ActorProbe{};
        ActorProbe.MeshHandle = mMeshHandle;
        ActorProbe.MaterialHandle = mMaterialHandle;
        ActorProbe.PipelineHandle = PipelineHandle;
        Probe.ActorProbes.push_back(ActorProbe);
    }

    FLightProbe LightProbe{};
    LightProbe.Type = ELightType::Directional;
    LightProbe.Color = FVector3{ 1.0f, 1.0f, 1.0f };
    LightProbe.Intensity = 1.0f;
    FVector3 LightDirection{ -FMatrix::CreateFromQuaternion(mOrbitRotation).TransformDirection(-FVector::UnitX) - FVector::UnitZ * 0.75f };
    LightDirection.Normalize();
    LightProbe.Direction = LightDirection;
    Probe.LightProbes.push_back(LightProbe);
    return Probe;
}

CameraProbe FViewerPanel::BuildPreviewCamera() const {
    const FMatrix OrbitMatrix{ FMatrix::CreateFromQuaternion(mOrbitRotation) };
    const FVector3 Offset{ OrbitMatrix.TransformDirection(-FVector::UnitX) };
    const FVector3 Eye{ mTarget + Offset * mDistance };
    const float Aspect{ static_cast<float>(mSurfaceWidth) / static_cast<float>(mSurfaceHeight) };
    CameraProbe Camera{};
    Camera.View = MakeCameraWorldMatrix(Eye).Invert();
    Camera.Projection = FMatrix::CreatePerspectiveFieldOfView(mFieldOfView, Aspect, 0.1f, std::max(1000.0f, mDistance * 4.0f));
    Camera.ViewProjection = Camera.View * Camera.Projection;
    return Camera;
}

void FViewerPanel::ProcessInput() {
    const ImGuiIO& Input{ ImGui::GetIO() };
    if (ImGui::IsItemActive()) {
        const float DeltaYaw{ Input.MouseDelta.x * 0.01f };
        const float DeltaPitch{ Input.MouseDelta.y * 0.01f };
        const FQuat YawRotation{ FQuat::CreateFromAxisAngle(FVector::UnitZ, DeltaYaw) };
        const FMatrix CurrentRotation{ FMatrix::CreateFromQuaternion(mOrbitRotation) };
        const FVector Right{ CurrentRotation.TransformDirection(FVector::UnitY) };
        const FQuat PitchRotation{ FQuat::CreateFromAxisAngle(Right, DeltaPitch) };
        mOrbitRotation = PitchRotation * YawRotation * mOrbitRotation;
        mOrbitRotation.Normalize();
    }

    if (ImGui::IsItemHovered() && Input.MouseWheel != 0.0f) {
        mDistance *= std::pow(0.9f, Input.MouseWheel);
        mDistance = std::clamp(mDistance, MinimumDistance, MaximumDistance);
    }
}

void FViewerPanel::RenderOffscreen(FRenderer& InRenderer, FAssetRegistry&) {
    if (mDesiredWidth == 0 || mDesiredHeight == 0) {
        return;
    }

    ResizeSurfaceIfNeeded(InRenderer.GetDevice(), mDesiredWidth, mDesiredHeight);
    if (!mSurface.IsValid()) {
        return;
    }

    if (!mLineRendererInitialized) {
        mLineRenderer->Initialize(InRenderer.GetDevice());
        mLineRendererInitialized = true;
    }

    FRenderProbe PreviewProbe{ BuildPreviewProbe() };
    FRenderSettings PreviewSettings{};
    PreviewSettings.ClearColor = FVector4{ 0.12f, 0.13f, 0.15f, 1.0f };
    InRenderer.RenderScene(mSurface, PreviewProbe, BuildPreviewCamera(), PreviewSettings);
    RenderOrientationAxis(InRenderer.GetDeviceContext());
}

void FViewerPanel::ReleaseRenderResources() {
    mSurface.Reset();
    mLineRenderer->Reset();
    mLineRendererInitialized = false;
}

void FViewerPanel::DrawPreview() {
    const ImVec2 Available{ ImGui::GetContentRegionAvail() };
    mDesiredWidth = static_cast<uint32>(std::max(1.0f, Available.x));
    mDesiredHeight = static_cast<uint32>(std::max(1.0f, Available.y));
    const ImVec2 Size{ static_cast<float>(mDesiredWidth), static_cast<float>(mDesiredHeight) };
    const ImVec2 TopLeft{ ImGui::GetCursorScreenPos() };
    const ImGuiViewport* Viewport{ ImGui::GetWindowViewport() };
    ImGui::InvisibleButton("##PreviewViewport", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    if (ID3D11ShaderResourceView* PreviewSRV{ mSurface.GetShaderResourceView() }; PreviewSRV != nullptr) {
        ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(PreviewSRV), TopLeft, ImVec2{ TopLeft.x + Size.x, TopLeft.y + Size.y });
    }

    ProcessInput();

    uint32 VertexCount{};
    size_t TriangleCount{};
    if (const UMesh* Mesh{ mRegistry != nullptr ? mRegistry->ResolveAsset<UMesh>(mMeshHandle) : nullptr }; Mesh != nullptr) {
        VertexCount = Mesh->GetVertexAttributeCount(EVertexAttribute::Position);
        TriangleCount = Mesh->GetIndices().size() / 3;
    }

    constexpr float OverlayMargin{ 12.0f };
    ImGui::SetNextWindowViewport(Viewport->ID);
    ImGui::SetNextWindowPos(ImVec2{ TopLeft.x + Size.x - OverlayMargin, TopLeft.y + OverlayMargin }, ImGuiCond_Always, ImVec2{ 1.0f, 0.0f });
    ImGui::SetNextWindowBgAlpha(0.4f);
    const ImGuiWindowFlags OverlayFlags{ ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize };
    if (ImGui::Begin("Mesh Statistics##Viewer", nullptr, OverlayFlags)) {
        ImGui::Text("Vertices: %u", VertexCount);
        ImGui::Text("Triangles: %zu", TriangleCount);
    }
    ImGui::End();
}

FString FViewerPanel::OpenFileDialog(const FString& FilePath, const OPENFILENAMEA& OFN) const {
    char FileName[MAX_PATH]{};
    OPENFILENAMEA OpenFileName{ OFN };
    OpenFileName.lpstrFile = FileName;
    const std::string InitialDirectoryPath{ std::filesystem::absolute(FilePath.c_str()).string() };
    if (!std::filesystem::exists(InitialDirectoryPath)) {
        std::filesystem::create_directories(InitialDirectoryPath);
    }
    OpenFileName.lpstrInitialDir = InitialDirectoryPath.c_str();
    return GetOpenFileNameA(&OpenFileName) ? FString{ FileName } : FString{};
}

void FViewerPanel::RenderOrientationAxis(ID3D11DeviceContext* Context) {
    if (Context == nullptr || !mLineRendererInitialized) {
        return;
    }

    FMatrix View{ BuildPreviewCamera().View };
    View.Translation(FVector3{ 0.0f, 0.0f, 3.0f });
    const FMatrix Projection{ FMatrix::CreateOrthographic(2.5f, 2.5f, 0.1f, 10.0f) };
    constexpr float AxisSize{ 100.0f };
    const D3D11_VIEWPORT AxisViewport{ 5.0f, 5.0f, AxisSize, AxisSize, 0.0f, 1.0f };
    Context->RSSetViewports(1, &AxisViewport);

    mLineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 1.0f, 0.0f, 0.0f }, 1.0f, FVector4{ 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
    mLineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 1.0f, 0.0f }, 1.0f, FVector4{ 0.0f, 1.0f, 0.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
    mLineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 0.0f, 1.0f }, 1.0f, FVector4{ 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
    mLineRenderer->Render(Context, FLineViewData{ .ViewProjection = View * Projection, .ViewportSize = FVector2D{ AxisSize, AxisSize } });

    const D3D11_VIEWPORT FullViewport{ 0.0f, 0.0f, static_cast<float>(mSurfaceWidth), static_cast<float>(mSurfaceHeight), 0.0f, 1.0f };
    Context->RSSetViewports(1, &FullViewport);
}
