#pragma once

#include <array>
#include <cstddef>

namespace BasicGeometry {
	// Rotate the former Y-up mesh basis +90 degrees around X while preserving winding.
	constexpr FVector3 ToZUp(const FVector3& Vector) {
		return FVector3{ Vector.x, -Vector.z, Vector.y };
	}

	template <std::size_t Count>
	constexpr std::array<FVector3, Count> ToZUp(const std::array<FVector3, Count>& Vectors) {
		std::array<FVector3, Count> Result{};

		for (std::size_t Index = 0; Index < Count; ++Index) {
			Result[Index] = ToZUp(Vectors[Index]);
		}

		return Result;
	}
}
