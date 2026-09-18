#pragma once
#include <Common.h>

struct FPoint
{
    int32 X = 0;
    int32 Y = 0;

    constexpr FPoint() = default;
    constexpr FPoint(int32 InX, int32 InY)
        : X(InX), Y(InY)
    {
    }
};

struct FRect
{
    FPoint Min;
    FPoint Max;

    constexpr int32 GetWidth() const
    {
        return Max.X - Min.X;
    }

    constexpr int32 GetHeight() const
    {
        return Max.Y - Min.Y;
    }

    constexpr bool IsEmpty() const
    {
        return GetWidth() <= 0 || GetHeight() <= 0;
    }

    constexpr bool Contains(const FPoint& Point) const
    {
        return Point.X >= Min.X && Point.X < Max.X &&
            Point.Y >= Min.Y && Point.Y < Max.Y;
    }
};