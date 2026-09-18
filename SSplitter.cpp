#include "PCH.h"
#include "SSplitter.h"


void SSplitter::SetRatio(float InRatio)
{
    Ratio = std::clamp(InRatio, 0.0f, 1.0f);
    UpdateLayout();
}

void SSplitterH::SetRect(const FRect& InRect)
{
    SWindow::SetRect(InRect);
    UpdateLayout();
}


void SSplitterH::DragTo(FPoint Coord)
{
    const int32 TotalHeight = std::max(0, Rect.GetHeight());
    const int32 Thickness = std::clamp(HandleThickness, 0, TotalHeight);
    const int32 AvailableHeight = TotalHeight - Thickness;

    if (AvailableHeight <= 0)
    {
        return;
    }

    Ratio = (static_cast<float>(Coord.Y - Rect.Min.Y) - Thickness * 0.5f)
        / static_cast<float>(AvailableHeight);

    UpdateLayout();
}

void SSplitterH::UpdateLayout()
{
    if (SideLT == nullptr || SideRB == nullptr)
    {
        return;
    }
    const int32 TotalHeight = std::max(0, Rect.GetHeight());
    const int32 Thickness = std::clamp(HandleThickness, 0, TotalHeight);
    const int32 AvailableHeight = TotalHeight - Thickness;

    const int32 EffectiveMinimum = std::min(MinimumSideSize, AvailableHeight / 2);

    int32 TopHeight = static_cast<int32>(std::round(static_cast<float>(AvailableHeight) * Ratio));

    TopHeight = std::clamp(TopHeight,  EffectiveMinimum,  AvailableHeight - EffectiveMinimum);


    if (AvailableHeight > 0)
    {
        Ratio = static_cast<float>(TopHeight)
            / static_cast<float>(AvailableHeight);
    }

    const int32 SplitY = Rect.Min.Y + TopHeight;

    HandleRect = {
        { Rect.Min.X, SplitY },
        { Rect.Max.X, SplitY  + Thickness }
    };

    SideLT->SetRect({ Rect.Min, { Rect.Max.X, HandleRect.Min.Y }});

    SideRB->SetRect({{ Rect.Min.X, HandleRect.Max.Y },  Rect.Max });
}




void SSplitterV::SetRect(const FRect& InRect)
{
    SWindow::SetRect(InRect);
    UpdateLayout();
}

void SSplitterV::DragTo(FPoint Coord)
{
    const int32 TotalWidth = std::max(0, Rect.GetWidth());
    const int32 Thickness = std::clamp(HandleThickness, 0, TotalWidth);
    const int32 AvailableWidth = TotalWidth - Thickness;

    if (AvailableWidth <= 0)
    {
        return;
    }

    Ratio = (static_cast<float>(Coord.X - Rect.Min.X) - Thickness * 0.5f)
        / static_cast<float>(AvailableWidth);

    UpdateLayout();
}

void SSplitterV::UpdateLayout()
{
    if (SideLT == nullptr || SideRB == nullptr)
    {
        return;
    }

    const int32 TotalWidth = std::max(0, Rect.GetWidth());
    const int32 Thickness = std::clamp(HandleThickness, 0, TotalWidth);
    const int32 AvailableWidth = TotalWidth - Thickness;

    const int32 EffectiveMinimum =
        std::min(MinimumSideSize, AvailableWidth / 2);

    int32 LeftWidth = static_cast<int32>(
        std::round(static_cast<float>(AvailableWidth) * Ratio));

    LeftWidth = std::clamp(LeftWidth, EffectiveMinimum, AvailableWidth - EffectiveMinimum);

    if (AvailableWidth > 0)
    {
        Ratio = static_cast<float>(LeftWidth)
            / static_cast<float>(AvailableWidth);
    }

    const int32 SplitX = Rect.Min.X + LeftWidth;

    HandleRect = {
        { SplitX, Rect.Min.Y },
        { SplitX + Thickness , Rect.Max.Y }
    };

    SideLT->SetRect({ Rect.Min,{ HandleRect.Min.X, Rect.Max.Y } });
    SideRB->SetRect({{ HandleRect.Max.X, Rect.Min.Y }, Rect.Max});
}