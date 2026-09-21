#pragma once

#include <cstdint>

#include "FMath.h"
#include "Core/Base/TypeInfo.h"

struct FMousePickRequestMessage
{
	inline static const FTypeInfo TypeInfo{
		"FMousePickRequestMessage",
		nullptr,
		nullptr
	};

	static const FTypeInfo& StaticTypeInfo() noexcept
	{
		return TypeInfo;
	}

	std::int32_t ScreenX = 0;
	std::int32_t ScreenY = 0;
	std::int32_t ViewportLeft = 0;
	std::int32_t ViewportTop = 0;
	std::uint32_t ViewportWidth = 0;
	std::uint32_t ViewportHeight = 0;
	FMatrix ViewProjection{};

	FMousePickRequestMessage() = default;

	FMousePickRequestMessage(
		std::int32_t InScreenX,
		std::int32_t InScreenY,
		std::int32_t InViewportLeft,
		std::int32_t InViewportTop,
		std::uint32_t InViewportWidth,
		std::uint32_t InViewportHeight,
		const FMatrix& InViewProjection) noexcept
		: ScreenX(InScreenX)
		, ScreenY(InScreenY)
		, ViewportLeft(InViewportLeft)
		, ViewportTop(InViewportTop)
		, ViewportWidth(InViewportWidth)
		, ViewportHeight(InViewportHeight)
		, ViewProjection(InViewProjection)
	{
	}
};
