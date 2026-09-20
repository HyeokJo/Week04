#include "PCH.h"
#include "FViewerPanel.h"

#include "ImGui/imgui.h"
#include "Render/Renderer.h"
#include "Core/Asset/FAssetRegistry.h"
#include "../../Core/Console/Console.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float PreviewClearColor[4]{ 0.12f, 0.13f, 0.15f, 1.0f };

    constexpr float MinimumDistance = 0.10f;
    constexpr float MaximumDistance = 500.0f;
}

void FViewerPanel::DrawPanel()
{
    if (!ImGui::Begin("Viewer", nullptr, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return;
    }

    DrawMenuBar();
    DrawPreview();
    //MeshHandle = Registry != nullptr ? Registry->GetAsset(FString("ObjImport")) : FAssetHandle{};
    ImGui::End();
}

void FViewerPanel::DrawMenuBar()
{
    if (!ImGui::BeginMenuBar())
    {
        return;
    }

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::Button("Import"))
        {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Import Button Click");


            OPENFILENAMEA OpenFileName = { 0 };

            OpenFileName.lStructSize = sizeof(OpenFileName);
            OpenFileName.hwndOwner = WindowHandle;

            OpenFileName.lpstrFilter = "OBJ Files(*.obj)\0*.obj\0All Files(*.*)\0*.*\0";

            OpenFileName.nMaxFile = MAX_PATH;

            OpenFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
            OpenFileName.lpstrDefExt = "obj";

            FString FilePath = OpenFileDialog(FString("./Content/ModelingFiles"), OpenFileName);

            EditorToWorldSender.TryEmplace<FMessageImportMesh>(FString("ObjImport"), FString(FilePath), FString("./Content/Metadata/MonkeyMesh.meta"));

            
        }

        ImGui::EndMenu(); 
    }

    if (ImGui::BeginMenu("Asset"))
    {
        static const char* const MeshNames[]{
            "CubeMesh", "SphereMesh", "ConeMesh", "CapsuleMesh",
            "CylinderMesh", "TorusMesh", "PlaneMesh"
        };

        for (const char* Name : MeshNames)
        {
            if (ImGui::MenuItem(Name))
            {
                //MeshHandle = Registry != nullptr ? Registry->GetAsset(Name) : FAssetHandle{};
            }
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}



void FViewerPanel::ResizeSurfaceIfNeeded(ID3D11Device* Device, uint32 Width, uint32 Height)
{
    if (Device == nullptr || Width == 0 || Height == 0)
    {
        return;
    }

    if (!Surface.IsValid())
    {
        Surface.InitializeOffscreen(Device, Width, Height);
    }
    else if (Width != SurfaceWidth || Height != SurfaceHeight)
    {
        Surface.Resize(Device, Width, Height);
    }

    SurfaceWidth = Width;
    SurfaceHeight = Height;
}

FMatrix FViewerPanel::MakeCameraWorldMatrix(
    const FVector3& Eye) const
{
    FMatrix OrbitMatrix =
        FMatrix::CreateFromQuaternion(OrbitRotation);

    FVector3 Forward =
        Target - Eye;
    Forward.Normalize();

    FVector3 Right =
        OrbitMatrix.TransformDirection(FVector::UnitY);

    Right.Normalize();

    FVector3 Up =
        Forward.Cross(Right);
    Up.Normalize();

    FMatrix Result = FMatrix::Identity;

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


FRenderProbe FViewerPanel::BuildPreviewProbe() const
{
    FRenderProbe Probe{};

    if (Registry == nullptr || SurfaceWidth == 0 || SurfaceHeight == 0)
    {
        return Probe;
    }

    //지정된게 없으면 기본 큐브로
    FAssetHandle Mesh = MeshHandle;
    if (!Mesh)
    {
        //Mesh = Registry->GetAsset("CubeMesh");
    }

    if (Mesh)
    {
        FActorProbe ActorProbe{};
        ActorProbe.MeshHandle = Mesh;
        //ActorProbe.MaterialHandle = Registry->GetAsset("GreyMaterial");
        //ActorProbe.PipelineHandle = Registry->GetAsset("BasePipeline");

        Probe.ActorProbes.push_back(ActorProbe);
    }

    // 프리뷰 전용 디렉셔널 라이트
    FLightProbe LightProbe{};
    LightProbe.Type = ELightType::Directional;
    LightProbe.Color = FVector3{ 1.0f, 1.0f, 1.0f };
    LightProbe.Intensity = 1.0f;
    LightProbe.Direction = FVector3{ -0.4f, -0.6f, -0.7f };

    Probe.LightProbes.push_back(LightProbe);


    const FMatrix OrbitMatrix =  FMatrix::CreateFromQuaternion(OrbitRotation);

    const FVector3 Offset =OrbitMatrix.TransformDirection(FVector::UnitX);


    const FVector3 Eye = Target + Offset * Distance;
    const float Aspect = static_cast<float>(SurfaceWidth) / static_cast<float>(SurfaceHeight);

    Probe.MainCameraProbe.View = MakeCameraWorldMatrix(Eye).Invert();


    Probe.MainCameraProbe.Projection =
        FMatrix::CreatePerspectiveFieldOfView(FieldOfView, Aspect, 0.1f, 1000.0f);
    Probe.MainCameraProbe.ViewProjection =
        Probe.MainCameraProbe.View * Probe.MainCameraProbe.Projection;

    return Probe;
}


void FViewerPanel::ProcessInput()
{
    const ImGuiIO& IO = ImGui::GetIO();

    if (ImGui::IsItemActive())
    {
        const float DeltaYaw =
            IO.MouseDelta.x * 0.01f;

        const float DeltaPitch =
            IO.MouseDelta.y * 0.01f;


        // 월드 Z축을 기준으로 회전
        const FQuat YawRotation = FQuat::CreateFromAxisAngle(FVector::UnitZ,DeltaYaw);


        //현재 Orbit의 Right 축을 구한다.
        const FMatrix CurrentRotation = FMatrix::CreateFromQuaternion(OrbitRotation);

        const FVector Right = CurrentRotation.TransformDirection(FVector::UnitY);


        //현재 Right 축을 기준으로 Pitch
        const FQuat PitchRotation = FQuat::CreateFromAxisAngle(Right,DeltaPitch);


        // 회전 누적
        OrbitRotation = PitchRotation * YawRotation * OrbitRotation;

        OrbitRotation.Normalize();
    }

    if (ImGui::IsItemHovered() && IO.MouseWheel != 0.0f)
    {
        Distance *= std::pow(0.9f, IO.MouseWheel);

        Distance = std::clamp(
            Distance,
            MinimumDistance,
            MaximumDistance
        );
    }
}



void FViewerPanel::RenderOffscreen(FRenderer& InRenderer, FAssetRegistry& InRegistry)
{

    if (DesiredWidth == 0 || DesiredHeight == 0)
    {
        return;
    }

    ResizeSurfaceIfNeeded(InRenderer.GetDevice(), DesiredWidth, DesiredHeight);

    if (!Surface.IsValid())
    {
        return;
    }

    ID3D11DeviceContext* Context = InRenderer.GetDeviceContext();

    // 렌더 타겟을 프리뷰 서피스로 돌린다. 이후 메인 루프의
    // BeginSceneRender() 가 다시 메인 서피스를 바인드한다.
    Surface.Bind(Context);
    Surface.Clear(Context, PreviewClearColor);

    FRenderProbe PreviewProbe = BuildPreviewProbe();
    InRenderer.RenderScene(PreviewProbe);
}

void FViewerPanel::DrawPreview()
{
    const ImVec2 Available = ImGui::GetContentRegionAvail();

    DesiredWidth = static_cast<uint32>(std::max(1.0f, Available.x));
    DesiredHeight = static_cast<uint32>(std::max(1.0f, Available.y));

    const ImVec2 Size{ static_cast<float>(DesiredWidth), static_cast<float>(DesiredHeight) };
    const ImVec2 TopLeft = ImGui::GetCursorScreenPos();

    // 이미지 영역을 아이템으로 선점한다. ImGui::Image 는 상호작용 아이템이 아니라서
    // 그 위에서 드래그하면 클릭이 창 배경으로 흘러가 창 자체가 움직인다.
    ImGui::InvisibleButton("##PreviewViewport", Size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    // 첫 프레임에는 아직 서피스가 없으므로 건너뛴다.
    if (ID3D11ShaderResourceView* PreviewSRV = Surface.GetShaderResourceView())
    {
        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(PreviewSRV),
            TopLeft,
            ImVec2(TopLeft.x + Size.x, TopLeft.y + Size.y));
    }

    ProcessInput();
}


FString FViewerPanel::OpenFileDialog(const FString& FilePath, const OPENFILENAMEA& OFN)
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
