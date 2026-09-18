#pragma once

#include <array>
#include <numbers>
#include <cmath>

namespace BasicGeometry {
	namespace Sphere {
		constexpr uint32 Segments = 32;
		constexpr uint32 Rings = 16;
		constexpr float Radius = 0.5f;

		constexpr uint32 VertexCount = (Rings + 1) * (Segments + 1);
		constexpr uint32 IndexCount = Rings * Segments * 6;

		struct FSphereGeometry {
			std::array<FVector3, VertexCount> Positions{};
			std::array<FVector3, VertexCount> Normals{};
			std::array<FVector2D, VertexCount> TexCoords{};
			std::array<uint32, IndexCount> Indices{};
		};

		inline FSphereGeometry GenerateSphere() {
			FSphereGeometry Geometry{};

			uint32 VertexIndex = 0;

			for (uint32 Ring = 0; Ring <= Rings; ++Ring) {
				const float V = static_cast<float>(Ring) / Rings;
				const float Phi = V * std::numbers::pi_v<float>;

				const float Z = std::cos(Phi);
				const float RingRadius = std::sin(Phi);

				for (uint32 Segment = 0; Segment <= Segments; ++Segment) {
					const float U = static_cast<float>(Segment) / Segments;
					const float Theta = U * std::numbers::pi_v<float> *2.0f;

					const float X = RingRadius * std::cos(Theta);
					const float Y = -RingRadius * std::sin(Theta);

					Geometry.Positions[VertexIndex] = FVector3{ X * Radius, Y * Radius, Z * Radius };
					Geometry.Normals[VertexIndex] = FVector3{ X, Y, Z };
					Geometry.TexCoords[VertexIndex] = FVector2D{ U, V };

					++VertexIndex;
				}
			}

			uint32 Index = 0;

			for (uint32 Ring = 0; Ring < Rings; ++Ring) {
				for (uint32 Segment = 0; Segment < Segments; ++Segment) {
					const uint32 TopLeft = Ring * (Segments + 1) + Segment;
					const uint32 TopRight = TopLeft + 1;
					const uint32 BottomLeft = (Ring + 1) * (Segments + 1) + Segment;
					const uint32 BottomRight = BottomLeft + 1;

					Geometry.Indices[Index++] = TopLeft;
					Geometry.Indices[Index++] = TopRight;
					Geometry.Indices[Index++] = BottomLeft;

					Geometry.Indices[Index++] = TopRight;
					Geometry.Indices[Index++] = BottomRight;
					Geometry.Indices[Index++] = BottomLeft;
				}
			}

			return Geometry;
		}

		inline const FSphereGeometry Geometry = GenerateSphere();

		inline const auto& Positions = Geometry.Positions;
		inline const auto& Normals = Geometry.Normals;
		inline const auto& TexCoords = Geometry.TexCoords;
		inline const auto& Indices = Geometry.Indices;
	}
}
