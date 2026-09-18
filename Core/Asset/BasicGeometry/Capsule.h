#pragma once

#include <array>
#include <cmath>
#include <numbers>

namespace BasicGeometry {
	namespace Capsule {
		inline constexpr uint32 Segments = 32;
		inline constexpr uint32 HemisphereRings = 8;

		inline constexpr float Radius = 0.25f;
		inline constexpr float HalfCylinderHeight = 0.25f;

		inline constexpr uint32 RingCount = HemisphereRings * 2 + 2;
		inline constexpr uint32 VertexCount = RingCount * (Segments + 1);
		inline constexpr uint32 IndexCount = (RingCount - 1) * Segments * 6;

		struct FGeometry {
			std::array<FVector3, VertexCount> Positions{};
			std::array<FVector3, VertexCount> Normals{};
			std::array<FVector2D, VertexCount> TexCoords{};
			std::array<uint32, IndexCount> Indices{};
		};

		inline FGeometry GenerateGeometry() {
			FGeometry Result{};

			uint32 Vertex = 0;

			const float Top = HalfCylinderHeight + Radius;
			const float TotalHeight = Top * 2.0f;

			for (uint32 Ring = 0; Ring <= HemisphereRings; ++Ring) {
				const float T = static_cast<float>(Ring) / static_cast<float>(HemisphereRings);
				const float Phi = T * std::numbers::pi_v<float> *0.5f;
				const float RingRadius = std::sin(Phi) * Radius;
				const float Z = HalfCylinderHeight + std::cos(Phi) * Radius;
				const float NormalZ = std::cos(Phi);
				const float NormalRadius = std::sin(Phi);
				const float V = (Top - Z) / TotalHeight;

				for (uint32 Segment = 0; Segment <= Segments; ++Segment) {
					const float U = static_cast<float>(Segment) / static_cast<float>(Segments);
					const float Theta = U * std::numbers::pi_v<float> *2.0f;
					const float X = std::cos(Theta);
					const float Y = -std::sin(Theta);

					Result.Positions[Vertex] = FVector3{ X * RingRadius, Y * RingRadius, Z };
					Result.Normals[Vertex] = FVector3{ X * NormalRadius, Y * NormalRadius, NormalZ };
					Result.TexCoords[Vertex++] = FVector2D{ U, V };
				}
			}

			for (uint32 Ring = 0; Ring <= HemisphereRings; ++Ring) {
				const float T = static_cast<float>(Ring) / static_cast<float>(HemisphereRings);
				const float Phi = T * std::numbers::pi_v<float> *0.5f;
				const float RingRadius = std::cos(Phi) * Radius;
				const float Z = -HalfCylinderHeight - std::sin(Phi) * Radius;
				const float NormalZ = -std::sin(Phi);
				const float NormalRadius = std::cos(Phi);
				const float V = (Top - Z) / TotalHeight;

				for (uint32 Segment = 0; Segment <= Segments; ++Segment) {
					const float U = static_cast<float>(Segment) / static_cast<float>(Segments);
					const float Theta = U * std::numbers::pi_v<float> *2.0f;
					const float X = std::cos(Theta);
					const float Y = -std::sin(Theta);

					Result.Positions[Vertex] = FVector3{ X * RingRadius, Y * RingRadius, Z };
					Result.Normals[Vertex] = FVector3{ X * NormalRadius, Y * NormalRadius, NormalZ };
					Result.TexCoords[Vertex++] = FVector2D{ U, V };
				}
			}

			uint32 Index = 0;

			for (uint32 Ring = 0; Ring < RingCount - 1; ++Ring) {
				for (uint32 Segment = 0; Segment < Segments; ++Segment) {
					const uint32 TopLeft = Ring * (Segments + 1) + Segment;
					const uint32 TopRight = TopLeft + 1;
					const uint32 BottomLeft = (Ring + 1) * (Segments + 1) + Segment;
					const uint32 BottomRight = BottomLeft + 1;

					Result.Indices[Index++] = TopLeft;
					Result.Indices[Index++] = TopRight;
					Result.Indices[Index++] = BottomRight;

					Result.Indices[Index++] = TopLeft;
					Result.Indices[Index++] = BottomRight;
					Result.Indices[Index++] = BottomLeft;
				}
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
