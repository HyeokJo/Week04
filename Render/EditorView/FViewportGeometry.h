#pragma once

#include "Common.h"

struct FPoint {
    int32 X{ 0 };
    int32 Y{ 0 };
};

struct FRect {
    FPoint Min{};
    FPoint Max{};

    int32 GetWidth() const { return Max.X - Min.X; }
    int32 GetHeight() const { return Max.Y - Min.Y; }
    bool IsEmpty() const { return GetWidth() <= 0 || GetHeight() <= 0; }
    bool Contains(const FPoint& Point) const {
        return Point.X >= Min.X && Point.X < Max.X && Point.Y >= Min.Y && Point.Y < Max.Y;
    }
};
