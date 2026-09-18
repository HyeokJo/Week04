#pragma once

#include "Swindow.h"

class SSplitter : public SWindow
{
public:
    void SetChildren(SWindow* InSideLT, SWindow* InSideRB)
    {
        SideLT = InSideLT;
        SideRB = InSideRB;
    };

    void SetRatio(float InRatio);
    float GetRatio() const { return Ratio; }

    const FRect& GetHandleRect() const
    {
        return HandleRect;
    }

    bool IsHover(FPoint Coord) const override
    {
        return HandleRect.Contains(Coord);
    }

    virtual void DragTo(FPoint Coord) = 0;

protected:
    virtual void UpdateLayout() = 0;

    SWindow* SideLT = nullptr;
    SWindow* SideRB = nullptr;

    FRect HandleRect{};

    int32 HandleThickness = 6;
    float Ratio = 0.5f;
    int32 MinimumSideSize = 100;
};

class SSplitterH : public SSplitter
{
public:
    void SetRect(const FRect& InRect) override;
    void DragTo(FPoint Coord) override;

protected:
    void UpdateLayout() override;
};

class SSplitterV : public SSplitter
{
public:
    void SetRect(const FRect& InRect) override;
    void DragTo(FPoint Coord) override;

protected:
    void UpdateLayout() override;
};
