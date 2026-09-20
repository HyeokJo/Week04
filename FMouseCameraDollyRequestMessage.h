#pragma once

#include "Core/Base/TypeInfo.h"
struct FMouseCameraDollyRequestMessage
{
    inline static const FTypeInfo TypeInfo{
       "FMouseCameraDollyRequestMessage",
       nullptr,
       nullptr
    };

    static const FTypeInfo& StaticTypeInfo() noexcept
    {
        return TypeInfo;
    }

    float Steps = 0.0f;

    FMouseCameraDollyRequestMessage() = default;

    explicit FMouseCameraDollyRequestMessage(float InSteps) noexcept
        : Steps(InSteps)
    {
    }

};