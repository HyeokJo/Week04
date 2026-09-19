#pragma once

#include <array>
#include <cmath>
#include <numbers>

namespace BasicGeometry {
	namespace Cone {
		inline constexpr uint32 Segments = 32;
		inline constexpr float Radius = 0.5f;
		inline constexpr float HalfHeight = 0.5f;

		inline constexpr uint32 SideVertexCount = Segments * 3;
		inline constexpr uint32 BottomVertexCount = Segments + 2;
		inline constexpr uint32 VertexCount = SideVertexCount + BottomVertexCount;
		inline constexpr uint32 IndexCount = Segments * 6;

		struct FGeometry {
			std::array<FVector3, VertexCount> Positions{};
			std::array<FVector3, VertexCount> Normals{};
			std::array<FVector2D, VertexCount> TexCoords{};
			std::array<uint32, IndexCount> Indices{};
		};

		inline FGeometry GenerateGeometry() {
			FGeometry Result{};

			uint32 Vertex = 0;
			uint32 Index = 0;

			const float NormalZ = Radius / (HalfHeight * 2.0f);
			const float NormalLength = std::sqrt(1.0f + NormalZ * NormalZ);

			for (uint32 Segment = 0; Segment < Segments; ++Segment) {
				const float U0 = static_cast<float>(Segment) / static_cast<float>(Segments);
				const float U1 = static_cast<float>(Segment + 1) / static_cast<float>(Segments);
				const float Angle0 = U0 * std::numbers::pi_v<float> *2.0f;
				const float Angle1 = U1 * std::numbers::pi_v<float> *2.0f;
				const float MidAngle = (Angle0 + Angle1) * 0.5f;

				const FVector3 Normal0{ std::cos(Angle0) / NormalLength, -std::sin(Angle0) / NormalLength, NormalZ / NormalLength };
				const FVector3 Normal1{ std::cos(Angle1) / NormalLength, -std::sin(Angle1) / NormalLength, NormalZ / NormalLength };
				const FVector3 ApexNormal{ std::cos(MidAngle) / NormalLength, -std::sin(MidAngle) / NormalLength, NormalZ / NormalLength };

				Result.Positions[Vertex] = FVector3{ std::cos(Angle0) * Radius, -std::sin(Angle0) * Radius, -HalfHeight };
				Result.Normals[Vertex] = Normal0;
				Result.TexCoords[Vertex++] = FVector2D{ U0, 1.0f };

				Result.Positions[Vertex] = FVector3{ 0.0f, 0.0f, HalfHeight };
				Result.Normals[Vertex] = ApexNormal;
				Result.TexCoords[Vertex++] = FVector2D{ (U0 + U1) * 0.5f, 0.0f };

				Result.Positions[Vertex] = FVector3{ std::cos(Angle1) * Radius, -std::sin(Angle1) * Radius, -HalfHeight };
				Result.Normals[Vertex] = Normal1;
				Result.TexCoords[Vertex++] = FVector2D{ U1, 1.0f };

				Result.Indices[Index++] = Vertex - 3;
				Result.Indices[Index++] = Vertex - 2;
				Result.Indices[Index++] = Vertex - 1;
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
