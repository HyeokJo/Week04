#pragma once

#include "Common.h"

using FViewportId = uint32;

enum class EProjectionType : uint8 {
    Perspective,
    Orthographic
};

enum class EOrthographicView : uint8 {
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom
};
