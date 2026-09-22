#include "PCH.h"
#include "FViewerPanel.h"

#include "ImGui/imgui.h"
#include "Render/Renderer.h"
#include "Core/Asset/FAssetRegistry.h"
#include "Core/Asset/UMaterial.h"
#include "Core/Asset/UMesh.h"
#include "Core/Console/Console.h"
#include "Render/Pipeline/UPipeline.h"
#include "Scene/FWorldEditorContext.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float MinimumDistance{ 0.10f };
    constexpr float MaximumDistance{ 500.0f };
    constexpr char DefaultMeshPath[]{ "/Game/System/Mesh/Cube.bin" };
    constexpr char DefaultMaterialPath[]{ "/Game/System/Material/Green.mtl" };
    constexpr char DefaultPipelinePath[]{ "/Game/Pipeline/Base" };
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

void FViewerPanel::DrawContents() {
    if (const FAssetHandle PreviewMesh{ mEditorContext.ConsumePreviewMesh() }; PreviewMesh) {
        SetMesh(PreviewMesh);
    }

    DrawMenuBar();
    DrawProperties();
    ImGui::Separator();
    //MeshHandle = Registry != nullptr ? Registry->GetAsset(FString("ObjImport")) : FAssetHandle{};
    DrawPreview();
}

void FViewerPanel::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Import OBJ...")) {
            OPENFILENAMEA OpenFileName{};
            OpenFileName.lStructSize = sizeof(OpenFileName);
            OpenFileName.hwndOwner = mWindowHandle;
            OpenFileName.lpstrFilter = "OBJ Files(*.obj)\0*.obj\0All Files(*.*)\0*.*\0";
            OpenFileName.nMaxFile = MAX_PATH;
            OpenFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
            OpenFileName.lpstrDefExt = "obj";

            FString FilePath{ OpenFileDialog(FString{ "./Content/ModelingFiles" }, OpenFileName) };
            if (!FilePath.empty()) {
                mEditorToWorldSender.TryEmplace<FMessageImportMesh>(FString{ "ObjImport" }, std::move(FilePath), FString{});
            }
        }
        //for (const char* Name : MeshNames)
        //{
        //    if (ImGui::MenuItem(Name))
        //    {
        //        //MeshHandle = Registry != nullptr ? Registry->GetAsset(Name) : FAssetHandle{};
        //        MeshHandle = Registry != nullptr ? Registry->FindAsset(FAssetPath{ Name }) : FAssetHandle{};
        //    }
        //}
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

    //지정된게 없으면 기본 큐브로
    if (mRegistry->ResolveAsset<UMesh>(mMeshHandle) == nullptr) {
        SetMesh({});
    }
    if (mRegistry->ResolveAsset<UMaterial>(mMaterialHandle) == nullptr) {
        SetMaterial({});
    }

    const FAssetHandle PipelineHandle{ mRegistry->FindAsset(FAssetPath{ DefaultPipelinePath }) };
    UPipeline* Pipeline{ mRegistry->ResolveAsset<UPipeline>(PipelineHandle) };
    if (mMeshHandle && mMaterialHandle && Pipeline != nullptr) {
        Pipeline->SetRenderMode(ERenderMode::Lit);
        FActorProbe ActorProbe{};
        ActorProbe.MeshHandle = mMeshHandle;
        ActorProbe.MaterialHandle = mMaterialHandle;
        ActorProbe.PipelineHandle = PipelineHandle;
        Probe.ActorProbes.push_back(ActorProbe);
    }

    // 프리뷰 전용 디렉셔널 라이트
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
    Camera.Projection = FMatrix::CreatePerspectiveFieldOfView(mFieldOfView, Aspect, 0.1f, 1000.0f);
    Camera.ViewProjection = Camera.View * Camera.Projection;
    return Camera;
}

void FViewerPanel::ProcessInput() {
    const ImGuiIO& Input{ ImGui::GetIO() };
    if (ImGui::IsItemActive()) {
        const float DeltaYaw{ Input.MouseDelta.x * 0.01f };
        const float DeltaPitch{ Input.MouseDelta.y * 0.01f };
        // 월드 Z축을 기준으로 회전
        const FQuat YawRotation{ FQuat::CreateFromAxisAngle(FVector::UnitZ, DeltaYaw) };
        const FMatrix CurrentRotation{ FMatrix::CreateFromQuaternion(mOrbitRotation) };
        //현재 Orbit의 Right 축을 구한다.
        const FVector Right{ CurrentRotation.TransformDirection(FVector::UnitY) };
        //현재 Right 축을 기준으로 Pitch
        const FQuat PitchRotation{ FQuat::CreateFromAxisAngle(Right, DeltaPitch) };
        // 회전 누적
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

    // 디바이스는 생성자 시점에 없으므로 첫 렌더에서 초기화한다.
    if (!mLineRendererInitialized) {
        mLineRenderer->Initialize(InRenderer.GetDevice());
        mLineRendererInitialized = true;
    }

    // 아웃라이너에서 더블클릭한 메시를 넘겨받는다. 핸들만 복사하므로
    // 액터가 사라져도 뷰어는 영향받지 않는다.
    FRenderProbe PreviewProbe{ BuildPreviewProbe() };
    FRenderSettings PreviewSettings{};
    PreviewSettings.ClearColor = FVector4{ 0.12f, 0.13f, 0.15f, 1.0f };
    InRenderer.RenderScene(mSurface, PreviewProbe, BuildPreviewCamera(), PreviewSettings);
    // RenderScene 이 서피스를 Bind/Clear 하므로 반드시 그 뒤에 그려야 남는다.
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
    // 이미지 영역을 아이템으로 선점한다. ImGui::Image 는 상호작용 아이템이 아니라서
    // 그 위에서 드래그하면 클릭이 창 배경으로 흘러가 창 자체가 움직인다.
    ImGui::InvisibleButton("##PreviewViewport", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    // 첫 프레임에는 아직 서피스가 없으므로 건너뛴다.
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
    // 카메라의 회전만 남기고 위치는 고정한다. 그래야 축이 화면 구석에 붙박이로 있으면서
    // 방향만 따라 돈다.
    View.Translation(FVector3{ 0.0f, 0.0f, 3.0f });
    const FMatrix Projection{ FMatrix::CreateOrthographic(2.5f, 2.5f, 0.1f, 10.0f) };
    constexpr float AxisSize{ 100.0f };
    // 직교 투영이라 세 축 길이가 항상 같게 보인다.
    const D3D11_VIEWPORT AxisViewport{ 5.0f, 5.0f, AxisSize, AxisSize, 0.0f, 1.0f };
    Context->RSSetViewports(1, &AxisViewport);

    mLineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 1.0f, 0.0f, 0.0f }, 1.0f, FVector4{ 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
    mLineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 1.0f, 0.0f }, 1.0f, FVector4{ 0.0f, 1.0f, 0.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
    mLineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 0.0f, 1.0f }, 1.0f, FVector4{ 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
    mLineRenderer->Render(Context, FLineViewData{ .ViewProjection = View * Projection, .ViewportSize = FVector2D{ AxisSize, AxisSize } });

    const D3D11_VIEWPORT FullViewport{ 0.0f, 0.0f, static_cast<float>(mSurfaceWidth), static_cast<float>(mSurfaceHeight), 0.0f, 1.0f };
    // 뷰포트를 되돌리지 않으면 이후 메인 렌더링이 이 100x100 안에 그려진다.
    Context->RSSetViewports(1, &FullViewport);
}
