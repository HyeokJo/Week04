#pragma once

#include "Core/Base/TypeInfo.h"

struct FWindowResizeRequestMessage
{
    inline static const FTypeInfo TypeInfo{
           "FWindowResizeRequestMessage",
           nullptr,
           nullptr
    };

    static const FTypeInfo& StaticTypeInfo() noexcept
    {
        return TypeInfo;
    }

    std::int32_t Width = 0;
    std::int32_t Height = 0;

    FWindowResizeRequestMessage() = default;

    FWindowResizeRequestMessage(
        std::int32_t InWidth, std::int32_t InHeight) noexcept
        : Width(InWidth), Height(InHeight)
    {
    }

};