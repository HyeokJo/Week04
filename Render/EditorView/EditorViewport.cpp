#include "PCH.h"

#include "EditorViewport.h"

#include <ranges>
#include <utility>

#include "../../FMouseInput.h"

#include "../../Scene/Component/UCollisionComponent.h"
#include "../../Scene/Component/UMeshComponent.h"

void EditorViewport::Initialize(ID3D11Device* Device, FAssetRegistry& AssetRegistry, FWorldEditorContext& InEditorContext) {
	LineRenderer->Initialize(Device);
	TransformGizmo.Initialize(Device, AssetRegistry, InEditorContext);
	EditorContext = &InEditorContext;
}

void EditorViewport::PrepareInput(const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport) {
	TransformGizmo.Update(Camera, Viewport);
}

void EditorViewport::ProcessInput(FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, bool bMouseCapturedByUI) {
	TransformGizmo.ProcessInput(KeyboardInput, MouseInput, bMouseCapturedByUI);
}

void EditorViewport::RenderInProbe(FRenderProbe& Probe, const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport) {
	TransformGizmo.Update(Camera, Viewport);
	TransformGizmo.Render(Probe);
}

void EditorViewport::RenderGrid(const FVector3& CameraPosition, ELineDepthMode DepthMode) {
	const float GridInterval = EditorContext != nullptr ? EditorContext->GetEditorSettings().GridSize : 1.0f;
	if (GridInterval <= 0.0f) {
		return;
	}

	int GridSize = (static_cast<int>(300 / GridInterval));
	float LineLength = static_cast<float>(GridSize) * GridInterval;

	float SnappedX = std::floor(CameraPosition.x / GridInterval) * GridInterval;
	float SnappedY = std::floor(CameraPosition.y / GridInterval) * GridInterval;

	for (auto x : std::views::iota(-GridSize, GridSize + 1)) {
		float LineX = SnappedX + static_cast<float>(x) * GridInterval;

		LineRenderer->AddLine(
			FVector3{ LineX, SnappedY - LineLength, 0.f },
			FVector3{ LineX, SnappedY + LineLength, 0.f },
			FVector4{ 0.5f, 0.5f, 0.5f, 1.0f },
			1.0f,
			DepthMode
		);
	}

	for (auto y : std::views::iota(-GridSize, GridSize + 1)) {
		float LineY = SnappedY + static_cast<float>(y) * GridInterval;

		LineRenderer->AddLine(
			FVector3{ SnappedX - LineLength, LineY, 0.f },
			FVector3{ SnappedX + LineLength, LineY, 0.f },
			FVector4{ 0.5f, 0.5f, 0.5f, 1.0f },
			1.0f,
			DepthMode
		);
	}
}

void EditorViewport::RenderAxis(ELineDepthMode DepthMode) {
	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 1.0f, 0.0f, 0.0f }, 1000.0f, FVector4{ 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f, DepthMode);
	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ -1.0f, 0.0f, 0.0f }, 1000.0f, FVector4{ 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f, DepthMode);

	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 1.0f, 0.0f }, 1000.0f, FVector4{ 0.0f, 1.0f, 0.0f, 1.0f }, 3.0f, DepthMode);
	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, -1.0f, 0.0f }, 1000.0f, FVector4{ 0.0f, 1.0f, 0.0f, 1.0f }, 3.0f, DepthMode);

	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 0.0f, 1.0f }, 1000.0f, FVector4{ 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f, DepthMode);
	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 0.0f, -1.0f }, 1000.0f, FVector4{ 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f, DepthMode);
}

void EditorViewport::RenderBounds(ELineDepthMode DepthMode) {
	if (EditorContext == nullptr) return;

	const UActorComponent* SelectedComponent = EditorContext->GetSelectedComponent();
	if (SelectedComponent == nullptr) return;

	if (SelectedComponent->GetTypeInfo()->IsA<UCollisionComponent>()) {
		const auto* CollisionComponent = static_cast<const UCollisionComponent*>(SelectedComponent);
		CollisionComponent->DrawEditorBounds(*LineRenderer, DepthMode);	
	}
	else if (SelectedComponent->GetTypeInfo()->IsA<UMeshComponent>()) {
		const auto* MeshComponent = static_cast<const UMeshComponent*>(SelectedComponent);
		
		auto& BB = MeshComponent->GetPickingBox(); 
		DirectX::BoundingOrientedBox WorldBB{};
		BB.Transform(WorldBB, MeshComponent->GetComponentToWorld().ToSimpleMath());


		std::array<DirectX::XMFLOAT3, DirectX::BoundingOrientedBox::CORNER_COUNT> Corners{};
		WorldBB.GetCorners(Corners.data());

		const FVector4 LineColor = FVector4{ 0.0f, 0.0f, 1.0f, 1.0f };
		const float Thickness = 1.0f;
		const auto AddEdge = [this, &Corners, LineColor, Thickness, DepthMode](size_t Start, size_t End) {
			LineRenderer->AddLine(FVector3{ Corners[Start] }, FVector3{ Corners[End] }, LineColor, Thickness, DepthMode);
			};

		AddEdge(0, 1);
		AddEdge(1, 2);
		AddEdge(2, 3);
		AddEdge(3, 0);
		AddEdge(4, 5);
		AddEdge(5, 6);
		AddEdge(6, 7);
		AddEdge(7, 4);
		AddEdge(0, 4);
		AddEdge(1, 5);
		AddEdge(2, 6);
		AddEdge(3, 7);


		DirectX::XMFLOAT3 Min = Corners[0];
		DirectX::XMFLOAT3 Max = Corners[0];

		for (const auto& Corner : Corners)
		{
			Min.x = std::min(Min.x, Corner.x);
			Min.y = std::min(Min.y, Corner.y);
			Min.z = std::min(Min.z, Corner.z);

			Max.x = std::max(Max.x, Corner.x);
			Max.y = std::max(Max.y, Corner.y);
			Max.z = std::max(Max.z, Corner.z);
		}

		std::array<DirectX::XMFLOAT3, 8> AABBCorners =
		{
			DirectX::XMFLOAT3{ Min.x, Min.y, Min.z },
			DirectX::XMFLOAT3{ Max.x, Min.y, Min.z },
			DirectX::XMFLOAT3{ Max.x, Max.y, Min.z },
			DirectX::XMFLOAT3{ Min.x, Max.y, Min.z },

			DirectX::XMFLOAT3{ Min.x, Min.y, Max.z },
			DirectX::XMFLOAT3{ Max.x, Min.y, Max.z },
			DirectX::XMFLOAT3{ Max.x, Max.y, Max.z },
			DirectX::XMFLOAT3{ Min.x, Max.y, Max.z }
		};

		const FVector4 AABBColor =
			FVector4{ 1.0f, 0.0f, 0.0f, 1.0f };

		const auto AddAABBEdge =
			[this, &AABBCorners, AABBColor, Thickness, DepthMode]
			(size_t Start, size_t End)
			{
				LineRenderer->AddLine(
					FVector3{ AABBCorners[Start] },
					FVector3{ AABBCorners[End] },
					AABBColor,
					Thickness,
					DepthMode
				);
			};

		AddAABBEdge(0, 1);
		AddAABBEdge(1, 2);
		AddAABBEdge(2, 3);
		AddAABBEdge(3, 0);

		AddAABBEdge(4, 5);
		AddAABBEdge(5, 6);
		AddAABBEdge(6, 7);
		AddAABBEdge(7, 4);

		AddAABBEdge(0, 4);
		AddAABBEdge(1, 5);
		AddAABBEdge(2, 6);
		AddAABBEdge(3, 7);
	}
}

void EditorViewport::RenderOrientationAxis(ID3D11DeviceContext* Context, const CameraProbe& Probe) {
	FMatrix view = Probe.View;
	view.Translation(FVector3{ 0.0f, 0.0f, 3.0f });

	FMatrix proj = FMatrix::CreateOrthographic(2.5f, 2.5f, 0.1f, 10.f);

	Context->RSSetViewports(1, &OrientationAxisViewport);

	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 1.0f, 0.0f, 0.0f }, 1.0f, FVector4{ 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 1.0f, 0.0f }, 1.0f, FVector4{ 0.0f, 1.0f, 0.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 0.0f, 1.0f }, 1.0f, FVector4{ 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);

	LineRenderer->Render(Context, FLineViewData{
		.ViewProjection = view * proj,
		.ViewportSize = FVector2D{ OrientationAxisViewport.Width, OrientationAxisViewport.Height }
	});
}

void EditorViewport::RenderSceneGuides(ID3D11DeviceContext* Context, const CameraProbe& Camera, const FVector3& CameraPosition, const D3D11_VIEWPORT& Viewport)
{
	const ELineDepthMode DepthMode = ELineDepthMode::DepthTested;

	RenderGrid(CameraPosition, DepthMode);
	RenderAxis(DepthMode);
	RenderBounds(DepthMode);
	LineRenderer->Render(Context,FLineViewData{.ViewProjection = Camera.ViewProjection,
			.ViewportSize = FVector2D{
				Viewport.Width,
				Viewport.Height
			}
		}
	);
}
