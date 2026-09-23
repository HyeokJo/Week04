#include "PCH.h"
#include "doctest.h"

#include "../Scene/AActor.h"
#include "../Scene/Component/UCameraComponent.h"
#include "../Scene/Component/UBillboardComponent.h"
#include "../Scene/Component/UBoxColliderComponent.h"
#include "../Scene/Component/UMeshComponent.h"
#include "../Scene/Component/UStaticMeshComponent.h"
#include "../Scene/FWorldEditorContext.h"
#include "../Scene/Subsystem/UCameraSubsystem.h"
#include "../Scene/Subsystem/UCollisionSubsystem.h"
#include "../Scene/Subsystem/UPickingSubsystem.h"
#include "../Scene/Subsystem/URenderSubsystem.h"
#include "../Scene/UWorld.h"

#include "../Core/Asset/UMesh.h"
#include "../Core/Asset/UMaterial.h"
#include "../Core/Asset/USurfaceOpaque.h"
#include "../Core/Asset/BasicGeometry/Plane.h"
#include "../Render/Pipeline/UPipeline.h"

#include <array>
#include <cstring>
#include <fstream>

namespace {
    template<typename T>
    concept CH8MeshHasBounds = requires(const T& Mesh) {
        Mesh.GetLocalBoundingBox();
    };

    static_assert(!CH8MeshHasBounds<UMesh>);

    class UTestMeshComponent final : public UMeshComponent {
    public:
        JG_DECLARE_DERIVED_TYPEINFO(UTestMeshComponent, UMeshComponent)

        UMesh* ResolveMesh() const override {
            return Mesh;
        }

        UMesh* Mesh = nullptr;
    };

    Microsoft::WRL::ComPtr<ID3D11Device> CreateTestDevice() {
        Microsoft::WRL::ComPtr<ID3D11Device> Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
        D3D_FEATURE_LEVEL FeatureLevel{};
        const HRESULT Result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &Device,
            &FeatureLevel,
            &Context
        );
        CHECK(SUCCEEDED(Result));
        return Device;
    }

    bool MakeTriangleMesh(UMesh& Mesh, ID3D11Device* Device) {
        const std::array<uint32, 3> Indices{ 0, 1, 2 };
        const std::array<FVector3, 3> Positions{
            FVector3{ -1.0f, 0.0f, -1.0f },
            FVector3{ 1.0f, 0.0f, -1.0f },
            FVector3{ -1.0f, 0.0f, 1.0f }
        };
        return Mesh.Make(Device, Indices, MakeVertexAttribute<EVertexAttribute::Position>(Positions));
    }
}

TEST_SUITE("CH6 World Subsystems") {
    TEST_CASE("World-owned subsystems track component registration and actor removal") {
        UWorld World;
        CHECK(World.GetRenderSubsystem().IsInitialized());
        CHECK(World.GetCollisionSubsystem().IsInitialized());
        CHECK(World.GetPickingSubsystem().IsInitialized());
        CHECK(World.GetCameraSubsystem().IsInitialized());
        CHECK_EQ(World.GetRenderSubsystem().GetWorld(), &World);
        CHECK_EQ(World.GetCollisionSubsystem().GetWorld(), &World);
        CHECK_EQ(World.GetPickingSubsystem().GetWorld(), &World);
        CHECK_EQ(World.GetCameraSubsystem().GetWorld(), &World);

        AActor* Actor = World.AdoptActor<AActor>();
        REQUIRE(Actor != nullptr);

        UStaticMeshComponent* Mesh = Actor->AddComponent<UStaticMeshComponent>();
        UBoxColliderComponent* Collision = Actor->AddComponent<UBoxColliderComponent>();
        UCameraComponent* Camera = Actor->AddComponent<UCameraComponent>();
        REQUIRE(Mesh != nullptr);
        REQUIRE(Collision != nullptr);
        REQUIRE(Camera != nullptr);

        CHECK(World.GetRenderSubsystem().ContainsComponent(Mesh));
        CHECK(World.GetCollisionSubsystem().ContainsComponent(Collision));
        CHECK(World.GetPickingSubsystem().ContainsComponent(Mesh));
        CHECK(World.GetPickingSubsystem().ContainsComponent(Collision));
        CHECK_EQ(World.GetCameraSubsystem().GetMainCamera(), Camera);

        Actor->SetWorld(nullptr);

        CHECK_FALSE(World.GetRenderSubsystem().ContainsComponent(Mesh));
        CHECK_FALSE(World.GetCollisionSubsystem().ContainsComponent(Collision));
        CHECK_FALSE(World.GetPickingSubsystem().ContainsComponent(Mesh));
        CHECK_EQ(World.GetCameraSubsystem().GetMainCamera(), nullptr);
    }

    TEST_CASE("Static mesh components receive fallback material and pipeline assets") {
        Microsoft::WRL::ComPtr<ID3D11Device> Device = CreateTestDevice();
        REQUIRE(Device != nullptr);

        FAssetRegistry AssetRegistry;
        REQUIRE(AssetRegistry.Initialize(Device.Get()));

        UWorld World;
        World.SetAssetRegistry(&AssetRegistry);

        AActor* Actor = World.AdoptActor<AActor>();
        REQUIRE(Actor != nullptr);

        UStaticMeshComponent* Mesh = Actor->AddComponent<UStaticMeshComponent>();
        REQUIRE(Mesh != nullptr);

        CHECK(AssetRegistry.ResolveAsset<UMaterial>(Mesh->GetMaterialHandle()) != nullptr);
        CHECK(AssetRegistry.ResolveAsset<USurfaceOpaque>(Mesh->GetMaterialHandle()) != nullptr);
        CHECK(AssetRegistry.ResolveAsset<UPipeline>(Mesh->GetPipelineHandle()) != nullptr);

        FRenderProbe Probe;
        World.GetRenderSubsystem().BuildRenderProbes(&AssetRegistry, Probe);
        REQUIRE_EQ(Probe.ActorProbes.size(), 1);
        CHECK(Probe.ActorProbes[0].MaterialHandle == Mesh->GetMaterialHandle());
        CHECK(Probe.ActorProbes[0].PipelineHandle == Mesh->GetPipelineHandle());
    }

    TEST_CASE("Opaque surfaces import common MTL properties") {
        Microsoft::WRL::ComPtr<ID3D11Device> Device = CreateTestDevice();
        REQUIRE(Device != nullptr);

        const std::filesystem::path MtlPath = std::filesystem::temp_directory_path() / "MacawSurfaceOpaqueTokenTest.mtl";
        {
            std::ofstream File(MtlPath);
            REQUIRE(File.is_open());
            File << "newmtl ComplexSurface\n";
            File << "Ka 0.1 0.2 0.3\n";
            File << "Kd 0.4 0.5 0.6\n";
            File << "Ks 0.2 0.3 0.4\n";
            File << "Ke 0.5 0.6 0.7\n";
            File << "Tf 0.7 0.8 0.9\n";
            File << "Ns 200.0\n";
            File << "Ni 1.33\n";
            File << "sharpness 72.0\n";
            File << "d -halo 0.8\n";
            File << "illum 7\n";
            File << "map_Kd base.png\n";
            File << "map_Ks specular.png\n";
            File << "map_bump bump.png\n";
            File << "norm normal.png\n";
            File << "disp displacement.png\n";
            File << "refl reflection.png\n";
        }

        USurfaceOpaque Surface{};
        uint32 NextTextureID{ 1 };
        const bool Initialized{ Surface.Initialize(Device.Get(), MtlPath, [&NextTextureID](const std::filesystem::path&) {
            return FAssetHandle{ .ID = NextTextureID++, .Generation = 1 };
        }) };
        std::error_code ErrorCode{};
        std::filesystem::remove(MtlPath, ErrorCode);
        REQUIRE(Initialized);

        const TArray<FMaterialGroup>& Groups = Surface.GetGroups();
        REQUIRE_EQ(Groups.size(), 1);
        const FMaterialGroup& Group = Groups[0];
        CHECK_EQ(Group.RefractionIndex, doctest::Approx(1.33f));
        CHECK_EQ(Group.Opacity, doctest::Approx(0.8f));
        CHECK_EQ(Group.IlluminationModel, 7);
        CHECK(Group.bDissolveHalo);
        CHECK_EQ(Group.DiffuseTexture.SourcePath, "base.png");
        CHECK(Group.SpecularTexture.Texture);
        CHECK(Group.BumpTexture.Texture);
        CHECK(Group.NormalTexture.Texture);
        CHECK(Group.DisplacementTexture.Texture);
        CHECK(Group.ReflectionTexture.Texture);

        const FMaterialChunkSignature Signature{ Surface.BuildChunkSignature() };
        REQUIRE_EQ(Signature.TextureFieldCount, 12);
        CHECK_FALSE(Signature.GetTextureHandle(0));
        CHECK(Signature.GetTextureHandle(1) == Group.DiffuseTexture.Texture);
        CHECK(Signature.GetTextureHandle(2) == Group.SpecularTexture.Texture);
        CHECK_FALSE(Signature.GetTextureHandle(3));
        CHECK(Signature.GetTextureHandle(7) == Group.BumpTexture.Texture);
        CHECK(Signature.GetTextureHandle(8) == Group.NormalTexture.Texture);
        CHECK(Signature.GetTextureHandle(9) == Group.DisplacementTexture.Texture);
        CHECK(Signature.GetTextureHandle(11) == Group.ReflectionTexture.Texture);

        FMaterialGPUSlot Slot{};
        Surface.BuildGPUData(Slot);
        std::array<float, 20> ScalarData{};
        std::memcpy(ScalarData.data(), Slot.Data.data(), sizeof(ScalarData));
        CHECK_EQ(ScalarData[0], doctest::Approx(0.4f));
        CHECK_EQ(ScalarData[1], doctest::Approx(0.5f));
        CHECK_EQ(ScalarData[2], doctest::Approx(0.6f));
        CHECK_EQ(ScalarData[3], doctest::Approx(0.8f));
        CHECK_EQ(ScalarData[4], doctest::Approx(0.1f));
        CHECK_EQ(ScalarData[5], doctest::Approx(0.2f));
        CHECK_EQ(ScalarData[6], doctest::Approx(0.3f));
        CHECK_EQ(ScalarData[7], doctest::Approx(200.0f));
        CHECK_EQ(ScalarData[8], doctest::Approx(0.2f));
        CHECK_EQ(ScalarData[9], doctest::Approx(0.3f));
        CHECK_EQ(ScalarData[10], doctest::Approx(0.4f));
        CHECK_EQ(ScalarData[11], doctest::Approx(1.33f));
        CHECK_EQ(ScalarData[12], doctest::Approx(0.5f));
        CHECK_EQ(ScalarData[13], doctest::Approx(0.6f));
        CHECK_EQ(ScalarData[14], doctest::Approx(0.7f));
        CHECK_EQ(ScalarData[15], doctest::Approx(72.0f));
        CHECK_EQ(ScalarData[16], doctest::Approx(0.7f));
        CHECK_EQ(ScalarData[17], doctest::Approx(0.8f));
        CHECK_EQ(ScalarData[18], doctest::Approx(0.9f));
        int32 IlluminationModel{};
        uint32 DissolveHalo{};
        std::memcpy(&IlluminationModel, Slot.Data.data() + 80, sizeof(IlluminationModel));
        std::memcpy(&DissolveHalo, Slot.Data.data() + 84, sizeof(DissolveHalo));
        CHECK_EQ(IlluminationModel, 7);
        CHECK_EQ(DissolveHalo, 1);

        const bool Modified{ Surface.ModifyGroup(0, [](FMaterialGroup& Target) {
            Target.AmbientTexture.Texture = FAssetHandle{ .ID = 101, .Generation = 1 };
            Target.DiffuseTexture.Texture = FAssetHandle{ .ID = 102, .Generation = 1 };
            Target.SpecularTexture.Texture = FAssetHandle{ .ID = 103, .Generation = 1 };
            Target.EmissiveTexture.Texture = FAssetHandle{ .ID = 104, .Generation = 1 };
            Target.TransmissionTexture.Texture = FAssetHandle{ .ID = 105, .Generation = 1 };
            Target.ShininessTexture.Texture = FAssetHandle{ .ID = 106, .Generation = 1 };
            Target.OpacityTexture.Texture = FAssetHandle{ .ID = 107, .Generation = 1 };
            Target.BumpTexture.Texture = FAssetHandle{ .ID = 108, .Generation = 1 };
            Target.NormalTexture.Texture = FAssetHandle{ .ID = 109, .Generation = 1 };
            Target.DisplacementTexture.Texture = FAssetHandle{ .ID = 110, .Generation = 1 };
            Target.DecalTexture.Texture = FAssetHandle{ .ID = 111, .Generation = 1 };
            Target.ReflectionTexture.Texture = FAssetHandle{ .ID = 112, .Generation = 1 };
        }) };
        REQUIRE(Modified);
        const FMaterialChunkSignature CompleteSignature{ Surface.BuildChunkSignature() };
        REQUIRE_EQ(CompleteSignature.TextureFieldCount, 12);
        for (uint8 TextureFieldIndex{}; TextureFieldIndex < CompleteSignature.TextureFieldCount; ++TextureFieldIndex) {
            CHECK_EQ(CompleteSignature.GetTextureHandle(TextureFieldIndex).ID, 101u + TextureFieldIndex);
        }
    }

    TEST_CASE("Editor context owns selection state and selected render flags") {
        UWorld World;
        FWorldEditorContext Context;
        World.SetEditorContext(&Context);

        AActor* SelectedActor = World.AdoptActor<AActor>();
        AActor* OtherActor = World.AdoptActor<AActor>();
        REQUIRE(SelectedActor != nullptr);
        REQUIRE(OtherActor != nullptr);

        UStaticMeshComponent* SelectedMesh = SelectedActor->AddComponent<UStaticMeshComponent>();
        UStaticMeshComponent* OtherMesh = OtherActor->AddComponent<UStaticMeshComponent>();
        UBoxColliderComponent* Collider = SelectedActor->AddComponent<UBoxColliderComponent>();
        REQUIRE(SelectedMesh != nullptr);
        REQUIRE(OtherMesh != nullptr);
        REQUIRE(Collider != nullptr);
        SelectedActor->SetRootComponent(SelectedMesh);
        OtherActor->SetRootComponent(OtherMesh);

        Context.SetSelectedActor(SelectedActor);
        CHECK_EQ(Context.GetSelectedActor(), SelectedActor);
        CHECK_EQ(Context.GetSelectedTransformTarget(), SelectedMesh);

        FRenderProbe Probe;
        World.GetRenderSubsystem().BuildRenderProbes(World.GetAssetRegistry(), Probe);
        REQUIRE_EQ(Probe.ActorProbes.size(), 2);
        CHECK((Probe.ActorProbes[0].Flags & static_cast<uint32>(ERenderObjectFlags::Selected)) != 0);
        CHECK((Probe.ActorProbes[1].Flags & static_cast<uint32>(ERenderObjectFlags::Selected)) == 0);

        REQUIRE(World.DestroyActor(SelectedActor));
        World.FlushPendingDestroyActors();
        CHECK_EQ(Context.GetSelectedActor(), nullptr);
    }

    TEST_CASE("Box colliders build mesh bounds and require connected mesh narrow phase") {
        Microsoft::WRL::ComPtr<ID3D11Device> Device = CreateTestDevice();
        REQUIRE(Device != nullptr);

        UMesh Mesh;
        REQUIRE(MakeTriangleMesh(Mesh, Device.Get()));

        UWorld World;
        AActor* Actor = World.AdoptActor<AActor>();
        REQUIRE(Actor != nullptr);
        UTestMeshComponent* MeshComponent = Actor->AddComponent<UTestMeshComponent>();
        UBoxColliderComponent* Collider = Actor->AddComponent<UBoxColliderComponent>();
        REQUIRE(MeshComponent != nullptr);
        REQUIRE(Collider != nullptr);
        Actor->SetRootComponent(MeshComponent);
        MeshComponent->Mesh = &Mesh;
        Collider->SetMeshComponent(MeshComponent);

        CHECK_EQ(Collider->GetMeshComponent(), MeshComponent);
        CHECK(Collider->BuildBoundsFromMesh());
        CHECK(Collider->GetExtent().x == doctest::Approx(1.0f));
        CHECK(Collider->GetExtent().z == doctest::Approx(1.0f));
        float MissDistance = 0.0f;
        CHECK_FALSE(Collider->Raycast(FRay{ FVector3{ -0.75f, 0.75f, -1.0f }.ToSimpleMath(), FVector3{ 0.0f, 0.0f, 1.0f }.ToSimpleMath() }, MissDistance));

        float Distance = 0.0f;
        CHECK(MeshComponent->RaycastMesh(FRay{ FVector3{ 0.5f, -0.5f, -1.0f }.ToSimpleMath(), FVector3{ 0.0f, 0.0f, 1.0f }.ToSimpleMath() }, Distance));

        UBoxColliderComponent* BoundsOnly = Actor->AddComponent<UBoxColliderComponent>();
        REQUIRE(BoundsOnly != nullptr);
        BoundsOnly->SetExtent(FVector3{ 1.0f, 1.0f, 1.0f });
        CHECK(BoundsOnly->Raycast(FRay{ FVector3{ 0.0f, 0.0f, -2.0f }.ToSimpleMath(), FVector3{ 0.0f, 0.0f, 1.0f }.ToSimpleMath() }, Distance));
        CHECK(Distance == doctest::Approx(1.0f));

        CHECK(World.GetCollisionSubsystem().ContainsComponent(Collider));
        REQUIRE(World.DestroyActor(Actor));
        World.FlushPendingDestroyActors();
        CHECK_FALSE(World.GetCollisionSubsystem().ContainsComponent(Collider));
    }

    TEST_CASE("Picking subsystem broad-phases primitives and narrow-phases mesh geometry") {
        Microsoft::WRL::ComPtr<ID3D11Device> Device = CreateTestDevice();
        REQUIRE(Device != nullptr);

        UMesh Mesh;
        REQUIRE(MakeTriangleMesh(Mesh, Device.Get()));

        UWorld World;
        AActor* Actor = World.AdoptActor<AActor>();
        REQUIRE(Actor != nullptr);
        UTestMeshComponent* MeshComponent = Actor->AddComponent<UTestMeshComponent>();
        REQUIRE(MeshComponent != nullptr);
        Actor->SetRootComponent(MeshComponent);
        MeshComponent->Mesh = &Mesh;
        MeshComponent->SetPickingBox(DirectX::BoundingOrientedBox{
            DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f },
            DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f },
            DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f }
        });

        UPrimitiveComponent* PickedComponent = nullptr;
        float Distance = 0.0f;
        CHECK_FALSE(World.GetPickingSubsystem().Raycast(
            FRay{ FVector3{ 0.0f, 0.5f, -2.0f }.ToSimpleMath(), FVector3{ 0.0f, 0.0f, 1.0f }.ToSimpleMath() },
            PickedComponent,
            Distance));

        CHECK(World.GetPickingSubsystem().Raycast(
            FRay{ FVector3{ 0.25f, -0.25f, -2.0f }.ToSimpleMath(), FVector3{ 0.0f, 0.0f, 1.0f }.ToSimpleMath() },
            PickedComponent,
            Distance));
        CHECK_EQ(PickedComponent, MeshComponent);
        CHECK(Distance == doctest::Approx(2.0f));
    }

    TEST_CASE("Billboard picking follows the camera-facing quad") {
        UWorld World{};
        AActor* Actor{ World.AdoptActor<AActor>() };
        REQUIRE(Actor != nullptr);
        UBillboardComponent* Billboard{ Actor->AddComponent<UBillboardComponent>() };
        REQUIRE(Billboard != nullptr);
        Actor->SetRootComponent(Billboard);
        Billboard->SetTextureHandle(FAssetHandle{ 1, 0 });
        Billboard->SetPipelineHandle(FAssetHandle{ 2, 0 });
        Billboard->SetSize(FVector2{ 2.0f, 2.0f });

        FMatrix CameraWorld{};
        UPrimitiveComponent* PickedComponent{};
        float Distance{};
        CHECK(World.GetPickingSubsystem().Raycast(FRay{ FVector3{ 0.5f, 0.5f, -2.0f }.ToSimpleMath(), FVector3{ 0.0f, 0.0f, 1.0f }.ToSimpleMath() }, PickedComponent, Distance, &CameraWorld));
        CHECK_EQ(PickedComponent, Billboard);
        CHECK(Distance == doctest::Approx(2.0f));
        CHECK_FALSE(World.GetPickingSubsystem().Raycast(FRay{ FVector3{ 1.5f, 0.0f, -2.0f }.ToSimpleMath(), FVector3{ 0.0f, 0.0f, 1.0f }.ToSimpleMath() }, PickedComponent, Distance, &CameraWorld));

        CameraWorld.m[0][0] = 0.0f;
        CameraWorld.m[0][2] = -1.0f;
        CameraWorld.m[2][0] = 1.0f;
        CameraWorld.m[2][2] = 0.0f;
        CHECK(World.GetPickingSubsystem().Raycast(FRay{ FVector3{ -2.0f, 0.5f, -0.5f }.ToSimpleMath(), FVector3{ 1.0f, 0.0f, 0.0f }.ToSimpleMath() }, PickedComponent, Distance, &CameraWorld));
        CHECK_EQ(PickedComponent, Billboard);
        CHECK(Distance == doctest::Approx(2.0f));
    }
}
