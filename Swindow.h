#pragma once

#include "FViewportGeometry.h"

class SWindow
{
public:
    virtual ~SWindow() = default;

    virtual void SetRect(const FRect& InRect)
    {
        Rect = InRect;
    }

    const FRect& GetRect() const
    {
        return Rect;
    }

    virtual bool IsHover(FPoint Coord) const
    {
        return Rect.Contains(Coord);
    }

protected:
    FRect Rect;
};