#pragma once

#include <array>
#include <cmath>
#include <numbers>

namespace BasicGeometry {
	namespace Cylinder {
		inline constexpr uint32 Segments = 32;
		inline constexpr float Radius = 0.5f;
		inline constexpr float HalfHeight = 0.5f;

		inline constexpr uint32 SideVertexCount = (Segments + 1) * 2;
		inline constexpr uint32 CapVertexCount = Segments + 2;
		inline constexpr uint32 VertexCount = SideVertexCount + CapVertexCount * 2;
		inline constexpr uint32 IndexCount = Segments * 12;

		struct FGeometry {
			std::array<FVector3, VertexCount> Positions{};
			std::array<FVector3, VertexCount> Normals{};
			std::array<FVector2D, VertexCount> TexCoords{};
			std::array<uint32, IndexCount> Indices{};
		};

		inline FGeometry GenerateGeometry() {
			FGeometry Result{};

			uint32 Vertex = 0;

			for (uint32 Segment = 0; Segment <= Segments; ++Segment) {
				const float U = static_cast<float>(Segment) / static_cast<float>(Segments);
				const float Angle = U * std::numbers::pi_v<float> *2.0f;
				const float X = std::cos(Angle);
				const float Y = -std::sin(Angle);

				Result.Positions[Vertex] = FVector3{ X * Radius, Y * Radius, -HalfHeight };
				Result.Normals[Vertex] = FVector3{ X, Y, 0.0f };
				Result.TexCoords[Vertex++] = FVector2D{ U, 1.0f };

				Result.Positions[Vertex] = FVector3{ X * Radius, Y * Radius, HalfHeight };
				Result.Normals[Vertex] = FVector3{ X, Y, 0.0f };
				Result.TexCoords[Vertex++] = FVector2D{ U, 0.0f };
			}

			const uint32 TopStart = Vertex;

			Result.Positions[Vertex] = FVector3{ 0.0f, 0.0f, HalfHeight };
			Result.Normals[Vertex] = FVector3{ 0.0f, 0.0f, 1.0f };
			Result.TexCoords[Vertex++] = FVector2D{ 0.5f, 0.5f };

			for (uint32 Segment = 0; Segment <= Segments; ++Segment) {
				const float Angle = static_cast<float>(Segment) / static_cast<float>(Segments) * std::numbers::pi_v<float> *2.0f;
				const float X = std::cos(Angle);
				const float Y = -std::sin(Angle);

				Result.Positions[Vertex] = FVector3{ X * Radius, Y * Radius, HalfHeight };
				Result.Normals[Vertex] = FVector3{ 0.0f, 0.0f, 1.0f };
				Result.TexCoords[Vertex++] = FVector2D{ X * 0.5f + 0.5f, -Y * 0.5f + 0.5f };
			}

			const uint32 BottomStart = Vertex;

			Result.Positions[Vertex] = FVector3{ 0.0f, 0.0f, -HalfHeight };
			Result.Normals[Vertex] = FVector3{ 0.0f, 0.0f, -1.0f };
			Result.TexCoords[Vertex++] = FVector2D{ 0.5f, 0.5f };

			for (uint32 Segment = 0; Segment <= Segments; ++Segment) {
				const float Angle = static_cast<float>(Segment) / static_cast<float>(Segments) * std::numbers::pi_v<float> *2.0f;
				const float X = std::cos(Angle);
				const float Y = -std::sin(Angle);

				Result.Positions[Vertex] = FVector3{ X * Radius, Y * Radius, -HalfHeight };
				Result.Normals[Vertex] = FVector3{ 0.0f, 0.0f, -1.0f };
				Result.TexCoords[Vertex++] = FVector2D{ X * 0.5f + 0.5f, Y * 0.5f + 0.5f };
			}

			uint32 Index = 0;

			for (uint32 Segment = 0; Segment < Segments; ++Segment) {
				const uint32 Bottom0 = Segment * 2;
				const uint32 Top0 = Bottom0 + 1;
				const uint32 Bottom1 = Bottom0 + 2;
				const uint32 Top1 = Bottom0 + 3;

				Result.Indices[Index++] = Bottom0;
				Result.Indices[Index++] = Top0;
				Result.Indices[Index++] = Top1;

				Result.Indices[Index++] = Bottom0;
				Result.Indices[Index++] = Top1;
				Result.Indices[Index++] = Bottom1;
			}

			for (uint32 Segment = 0; Segment < Segments; ++Segment) {
				Result.Indices[Index++] = TopStart;
				Result.Indices[Index++] = TopStart + Segment + 2;
				Result.Indices[Index++] = TopStart + Segment + 1;
			}

			for (uint32 Segment = 0; Segment < Segments; ++Segment) {
				Result.Indices[Index++] = BottomStart;
				Result.Indices[Index++] = BottomStart + Segment + 1;
				Result.Indices[Index++] = BottomStart + Segment + 2;
			}

			return Result;
		}

		inline const FGeometry Geometry = GenerateGeometry();

		inline const auto& Positions = Geometry.Positions;
		inline const auto& Normals = Geometry.Normals;
		inline const auto& TexCoords = Geometry.TexCoords;
		inline const auto& Indices = Geometry.Indices;
	}
}
