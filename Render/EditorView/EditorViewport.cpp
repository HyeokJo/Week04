 #include "PCH.h"

#include "EditorViewport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "../../FMouseInput.h"

#include "../../Scene/Component/UCollisionComponent.h"
#include "../../Scene/Component/UMeshComponent.h"

namespace {
bool GetVisibleGridBounds(const CameraProbe& Camera, float& MinimumX, float& MaximumX, float& MinimumY, float& MaximumY) {
	FMatrix InverseViewProjection{};
	if (!Camera.ViewProjection.TryInverse(InverseViewProjection)) {
		return false;
	}

	constexpr std::array<FVector3, 8> ClipCorners{
		FVector3{ -1.0f, -1.0f, 0.0f }, FVector3{ 1.0f, -1.0f, 0.0f }, FVector3{ 1.0f, 1.0f, 0.0f }, FVector3{ -1.0f, 1.0f, 0.0f },
		FVector3{ -1.0f, -1.0f, 1.0f }, FVector3{ 1.0f, -1.0f, 1.0f }, FVector3{ 1.0f, 1.0f, 1.0f }, FVector3{ -1.0f, 1.0f, 1.0f }
	};
	constexpr std::array<std::array<int, 2>, 12> Edges{
		std::array<int, 2>{ 0, 1 }, std::array<int, 2>{ 1, 2 }, std::array<int, 2>{ 2, 3 }, std::array<int, 2>{ 3, 0 },
		std::array<int, 2>{ 4, 5 }, std::array<int, 2>{ 5, 6 }, std::array<int, 2>{ 6, 7 }, std::array<int, 2>{ 7, 4 },
		std::array<int, 2>{ 0, 4 }, std::array<int, 2>{ 1, 5 }, std::array<int, 2>{ 2, 6 }, std::array<int, 2>{ 3, 7 }
	};
	std::array<FVector3, 8> WorldCorners{};
	for (size_t Index{ 0 }; Index < ClipCorners.size(); ++Index) {
		if (!InverseViewProjection.TransformCoord(ClipCorners[Index], WorldCorners[Index])) {
			return false;
		}
	}

	bool HasPoint{ false };
	const auto IncludePoint = [&MinimumX, &MaximumX, &MinimumY, &MaximumY, &HasPoint](const FVector3& Point) {
		if (!HasPoint) {
			MinimumX = MaximumX = Point.x;
			MinimumY = MaximumY = Point.y;
			HasPoint = true;
			return;
		}
		MinimumX = std::min(MinimumX, Point.x);
		MaximumX = std::max(MaximumX, Point.x);
		MinimumY = std::min(MinimumY, Point.y);
		MaximumY = std::max(MaximumY, Point.y);
	};
	constexpr float PlaneEpsilon{ 0.0001f };
	for (const std::array<int, 2>& Edge : Edges) {
		const FVector3& Start{ WorldCorners[Edge[0]] };
		const FVector3& End{ WorldCorners[Edge[1]] };
		const bool StartOnPlane{ std::abs(Start.z) <= PlaneEpsilon };
		const bool EndOnPlane{ std::abs(End.z) <= PlaneEpsilon };
		if (StartOnPlane) {
			IncludePoint(Start);
		}
		if (EndOnPlane) {
			IncludePoint(End);
		}
		if ((Start.z < -PlaneEpsilon && End.z > PlaneEpsilon) || (Start.z > PlaneEpsilon && End.z < -PlaneEpsilon)) {
			const float Fraction{ -Start.z / (End.z - Start.z) };
			IncludePoint(FVector3{ Start.x + (End.x - Start.x) * Fraction, Start.y + (End.y - Start.y) * Fraction, 0.0f });
		}
	}
	return HasPoint;
}
}

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

FStateChannel<uint8>::FReadWriter EditorViewport::GetGizmoMode() {
	return TransformGizmo.GetGizmoMode();
}

FStateChannel<uint8>::FReadWriter EditorViewport::GetGizmoCoordinateSpace() {
	return TransformGizmo.GetGizmoCoordinateSpace();
}

void EditorViewport::RenderGrid(const CameraProbe& Camera, const FVector3& CameraPosition, const D3D11_VIEWPORT& Viewport, FVector2D& FadeCenter, ELineDepthMode DepthMode) {
	const float GridInterval{ EditorContext != nullptr ? EditorContext->GetEditorSettings().GridSize : 1.0f };
	const float ProjectionYScale{ Camera.Projection.m[1][1] };
	if (!std::isfinite(GridInterval) || GridInterval <= 0.0f || ProjectionYScale <= 0.0f || Viewport.Width <= 0.0f || Viewport.Height <= 0.0f) {
		return;
	}
	float MinimumX{};
	float MaximumX{};
	float MinimumY{};
	float MaximumY{};

	if (!GetVisibleGridBounds(Camera, MinimumX, MaximumX, MinimumY, MaximumY)) {
		return;
	}

	const bool Perspective{ std::abs(Camera.Projection.m[2][3]) > 0.0001f };
	FadeCenter = Perspective ? FVector2D{ CameraPosition.x, CameraPosition.y } : FVector2D{ (MinimumX + MaximumX) * 0.5f, (MinimumY + MaximumY) * 0.5f };
	constexpr float GridRadius{ 550.0f };
	MinimumX = std::max(MinimumX, FadeCenter.x - GridRadius);
	MaximumX = std::min(MaximumX, FadeCenter.x + GridRadius);
	MinimumY = std::max(MinimumY, FadeCenter.y - GridRadius);
	MaximumY = std::min(MaximumY, FadeCenter.y + GridRadius);
	if (MinimumX >= MaximumX || MinimumY >= MaximumY) {
		return;
	}
	float Step{ GridInterval };
	int Level{ 0 };
	while (((MaximumX - MinimumX) / Step > 2048.0f || (MaximumY - MinimumY) / Step > 2048.0f) && Level < 8) {
		Step *= 10.0f;
		++Level;
	}

	const float Margin{ std::max(2.0f * Step, 3.0f) };
	MinimumX = std::max(MinimumX - Margin, FadeCenter.x - GridRadius);
	MaximumX = std::min(MaximumX + Margin, FadeCenter.x + GridRadius);
	MinimumY = std::max(MinimumY - Margin, FadeCenter.y - GridRadius);
	MaximumY = std::min(MaximumY + Margin, FadeCenter.y + GridRadius);
	const int64_t FirstX{ static_cast<int64_t>(std::ceil(static_cast<double>(MinimumX) / Step)) };
	const int64_t LastX{ static_cast<int64_t>(std::floor(static_cast<double>(MaximumX) / Step)) };
	const int64_t FirstY{ static_cast<int64_t>(std::ceil(static_cast<double>(MinimumY) / Step)) };
	const int64_t LastY{ static_cast<int64_t>(std::floor(static_cast<double>(MaximumY) / Step)) };
	for (int64_t Index{ FirstX }; Index <= LastX; ++Index) {
		const float TierSpacing{ Index % 100 == 0 ? -Step * 100.0f : Index % 10 == 0 ? -Step * 10.0f : Step };
		const float WidthPixels{ TierSpacing < 0.0f ? 1.5f : 1.0f };
		const float Position{ static_cast<float>(Index) * Step };
		const float Offset{ Position - FadeCenter.x };
		const float HalfLength{ std::sqrt(std::max(GridRadius * GridRadius - Offset * Offset, 0.0f)) };
		const float Start{ std::max(MinimumY, FadeCenter.y - HalfLength) };
		const float End{ std::min(MaximumY, FadeCenter.y + HalfLength) };
		if (End > Start) {
			LineRenderer->AddGridLine(FVector3{ Position, Start, 0.0f }, FVector3{ Position, End, 0.0f }, FVector4{ 0.5f, 0.5f, 0.5f, 1.0f }, WidthPixels, TierSpacing, DepthMode);
		}
	}
	for (int64_t Index{ FirstY }; Index <= LastY; ++Index) {
		const float TierSpacing{ Index % 100 == 0 ? -Step * 100.0f : Index % 10 == 0 ? -Step * 10.0f : Step };
		const float WidthPixels{ TierSpacing < 0.0f ? 1.5f : 1.0f };
		const float Position{ static_cast<float>(Index) * Step };
		const float Offset{ Position - FadeCenter.y };
		const float HalfLength{ std::sqrt(std::max(GridRadius * GridRadius - Offset * Offset, 0.0f)) };
		const float Start{ std::max(MinimumX, FadeCenter.x - HalfLength) };
		const float End{ std::min(MaximumX, FadeCenter.x + HalfLength) };
		if (End > Start) {
			LineRenderer->AddGridLine(FVector3{ Start, Position, 0.0f }, FVector3{ End, Position, 0.0f }, FVector4{ 0.5f, 0.5f, 0.5f, 1.0f }, WidthPixels, TierSpacing, DepthMode);
		}
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

void EditorViewport::RenderOrientationAxis(ID3D11DeviceContext* Context, const CameraProbe& Probe, const D3D11_VIEWPORT& Viewport) {
	constexpr float AxisScale{ 0.15f };
	constexpr float MaximumAxisSize{ 160.0f };
	constexpr float AxisMargin{ 5.0f };
	if (Context == nullptr || Viewport.Width <= AxisMargin * 2.0f || Viewport.Height <= AxisMargin * 2.0f) {
		return;
	}

	const float AxisSize{ std::min(std::min(Viewport.Width, Viewport.Height) * AxisScale, MaximumAxisSize) };
	const D3D11_VIEWPORT AxisViewport{ Viewport.TopLeftX + AxisMargin, Viewport.TopLeftY + AxisMargin, AxisSize, AxisSize, Viewport.MinDepth, Viewport.MaxDepth };

	FMatrix View{ Probe.View };
	View.Translation(FVector3{ 0.0f, 0.0f, 3.0f });
	const FMatrix Projection{ FMatrix::CreateOrthographic(2.5f, 2.5f, 0.1f, 10.f) };

	Context->RSSetViewports(1, &AxisViewport);

	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 1.0f, 0.0f, 0.0f }, 1.0f, FVector4{ 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 1.0f, 0.0f }, 1.0f, FVector4{ 0.0f, 1.0f, 0.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);
	LineRenderer->AddRay(FVector3{ 0.0f, 0.0f, 0.0f }, FVector3{ 0.0f, 0.0f, 1.0f }, 1.0f, FVector4{ 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f, ELineDepthMode::DepthTested);

	LineRenderer->Render(Context, FLineViewData{
		.ViewProjection = View * Projection,
		.ViewportSize = FVector2D{ AxisViewport.Width, AxisViewport.Height }
	});
	Context->RSSetViewports(1, &Viewport);
}

void EditorViewport::RenderSceneGuides(ID3D11DeviceContext* Context, const CameraProbe& Camera, const FVector3& CameraPosition, const D3D11_VIEWPORT& Viewport)
{
	const ELineDepthMode DepthMode = ELineDepthMode::DepthTested;
	const FEditorSettings Settings{ EditorContext != nullptr ? EditorContext->GetEditorSettings() : FEditorSettings{} };
	FVector2D FadeCenter{ CameraPosition.x, CameraPosition.y };

	if (Settings.mGridVisible) {
		RenderGrid(Camera, CameraPosition, Viewport, FadeCenter, DepthMode);
	}
	if (Settings.mAxisVisible) {
		RenderAxis(DepthMode);
	}
	RenderBounds(DepthMode);
	LineRenderer->Render(Context,FLineViewData{.ViewProjection = Camera.ViewProjection,
			.ViewportSize = FVector2D{
				Viewport.Width,
				Viewport.Height
			},
			.GridFade = FVector4{ FadeCenter.x, FadeCenter.y, 450.0f, 550.0f }
		}
	);
}
