#include "PCH.h"
#include "FMousePickRequestMessage.h"

const FTypeInfo& FMousePickRequestMessage::StaticTypeInfo() noexcept {
	return TypeInfo;
}

FMousePickRequestMessage::FMousePickRequestMessage(std::int32_t InScreenX, std::int32_t InScreenY, std::int32_t InViewportLeft, std::int32_t InViewportTop, std::uint32_t InViewportWidth, std::uint32_t InViewportHeight, const FMatrix& InViewProjection, const FMatrix& InView) noexcept
	: 	ScreenX(InScreenX),
		ScreenY(InScreenY),
		ViewportLeft(InViewportLeft),
		ViewportTop(InViewportTop),
		ViewportWidth(InViewportWidth),
		ViewportHeight(InViewportHeight),
		ViewProjection(InViewProjection),
		View(InView)
{
}
