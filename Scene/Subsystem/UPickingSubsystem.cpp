#include "PCH.h"

#include "UPickingSubsystem.h"

#include "Scene/Component/UMeshComponent.h"
#include "Scene/Component/UBillboardComponent.h"
#include "Scene/Component/UPrimitiveComponent.h"

#include <cmath>

void UPickingSubsystem::RegisterComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr || ContainsComponent(Component)) {
        return;
    }

    Components.emplace_back(Component);
}

void UPickingSubsystem::UnregisterComponent(UPrimitiveComponent* Component) {
    std::erase_if(Components, [Component](const TObjectRef<UPrimitiveComponent>& ComponentRef) {
        return ComponentRef.Get() == Component;
    });
}

bool UPickingSubsystem::Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld) const {
    OutComponent = nullptr;
    OutDistance = std::numeric_limits<float>::max();

    for (const TObjectRef<UPrimitiveComponent>& ComponentRef : Components) {
        UPrimitiveComponent* Component = ComponentRef.Get();
        if (Component == nullptr || !Component->IsActive() || !Component->IsVisible()) {
            continue;
        }

        if (Component->GetTypeInfo()->IsA(UBillboardComponent::StaticTypeInfo())) {
            if (CameraWorld == nullptr) {
                continue;
            }
            const UBillboardComponent* Billboard{ static_cast<const UBillboardComponent*>(Component) };
            std::array<FVector3, 4> Corners{};
            if (!Billboard->GetWorldCorners(*CameraWorld, Corners)) {
                continue;
            }
            const FVector3 Right{ Corners[2] - Corners[0] };
            const FVector3 Down{ Corners[1] - Corners[0] };
            const FVector3 Normal{ Right.Cross(Down) };
            const FVector3 Direction{ Ray.direction };
            const float Denominator{ Direction.Dot(Normal) };
            if (std::abs(Denominator) <= 0.000001f) {
                continue;
            }
            const float HitDistance{ (Corners[0] - FVector3{ Ray.position }).Dot(Normal) / Denominator };
            if (HitDistance < 0.0f || HitDistance >= OutDistance) {
                continue;
            }
            const FVector3 HitOffset{ FVector3{ Ray.position } + Direction * HitDistance - Corners[0] };
            const float Horizontal{ HitOffset.Dot(Right) / Right.LengthSquared() };
            const float Vertical{ HitOffset.Dot(Down) / Down.LengthSquared() };
            if (Horizontal >= 0.0f && Horizontal <= 1.0f && Vertical >= 0.0f && Vertical <= 1.0f) {
                OutComponent = Component;
                OutDistance = HitDistance;
            }
            continue;
        }

        DirectX::BoundingOrientedBox WorldBox;
        Component->GetPickingBox().Transform(WorldBox, Component->GetComponentToWorld().ToSimpleMath());

        float BroadPhaseDistance = 0.0f;
        if (!WorldBox.Intersects(Ray.position, Ray.direction, BroadPhaseDistance)) {
            continue;
        }

        float HitDistance = BroadPhaseDistance;
        if (Component->GetTypeInfo()->IsA(UMeshComponent::StaticTypeInfo())) {
            auto* MeshComponent = static_cast<UMeshComponent*>(Component);
            if (!MeshComponent->RaycastMesh(Ray, HitDistance)) {
                continue;
            }
        }

        if (HitDistance < OutDistance) {
            OutComponent = Component;
            OutDistance = HitDistance;
        }
    }

    return OutComponent != nullptr;
}

bool UPickingSubsystem::ContainsComponent(const UPrimitiveComponent* Component) const {
    return std::ranges::any_of(Components, [Component](const TObjectRef<UPrimitiveComponent>& ComponentRef) {
        return ComponentRef.Get() == Component;
    });
}

const TArray<TObjectRef<UPrimitiveComponent>>& UPickingSubsystem::GetRegisteredComponents() const {
    return Components;
}

void UPickingSubsystem::OnDeinitialize() {
    Components.clear();
}
