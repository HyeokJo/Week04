#include "PCH.h"

#include "Render/EditorView/SSplitter.h"

FSplitterRatio::FSplitterRatio(float InValue) : Value(std::make_shared<float>(std::clamp(InValue, 0.0f, 1.0f))) {
}

float FSplitterRatio::GetValue() const {
    return *Value;
}

void FSplitterRatio::SetValue(float InValue) {
    *Value = std::clamp(InValue, 0.0f, 1.0f);
}

bool FSplitterRatio::SharesStateWith(const FSplitterRatio& Other) const {
    return Value == Other.Value;
}

void SSplitter::SetRatio(float InRatio) {
    Ratio.SetValue(InRatio);
    UpdateLayout();
}

void SSplitter::SetRatioState(const FSplitterRatio& InRatio) {
    Ratio = InRatio;
}

void SSplitterH::SetRect(const FRect& InRect) {
    SWindow::SetRect(InRect);
    UpdateLayout();
}

void SSplitterH::DragTo(FPoint Point) {
    const int32 AvailableHeight = std::max(0, Rect.GetHeight() - HandleThickness);
    if (AvailableHeight == 0) {
        return;
    }

    Ratio.SetValue((static_cast<float>(Point.Y - Rect.Min.Y) - HandleThickness * 0.5f) / static_cast<float>(AvailableHeight));
    UpdateLayout();
}

void SSplitterH::UpdateLayout() {
    if (First == nullptr || Second == nullptr) {
        HandleRect = {};
        return;
    }

    const int32 Thickness = std::clamp(HandleThickness, 0, std::max(0, Rect.GetHeight()));
    const int32 AvailableHeight = std::max(0, Rect.GetHeight() - Thickness);
    const int32 Minimum = std::min(MinimumSideSize, AvailableHeight / 2);
    const int32 FirstHeight = std::clamp(static_cast<int32>(std::round(AvailableHeight * Ratio.GetValue())), Minimum, AvailableHeight - Minimum);
    Ratio.SetValue(AvailableHeight > 0 ? static_cast<float>(FirstHeight) / AvailableHeight : 0.5f);

    const int32 SplitY = Rect.Min.Y + FirstHeight;
    HandleRect = { { Rect.Min.X, SplitY }, { Rect.Max.X, SplitY + Thickness } };
    First->SetRect({ Rect.Min, { Rect.Max.X, HandleRect.Min.Y } });
    Second->SetRect({ { Rect.Min.X, HandleRect.Max.Y }, Rect.Max });
}

void SSplitterV::SetRect(const FRect& InRect) {
    SWindow::SetRect(InRect);
    UpdateLayout();
}

void SSplitterV::DragTo(FPoint Point) {
    const int32 AvailableWidth = std::max(0, Rect.GetWidth() - HandleThickness);
    if (AvailableWidth == 0) {
        return;
    }

    Ratio.SetValue((static_cast<float>(Point.X - Rect.Min.X) - HandleThickness * 0.5f) / static_cast<float>(AvailableWidth));
    UpdateLayout();
}

void SSplitterV::UpdateLayout() {
    if (First == nullptr || Second == nullptr) {
        HandleRect = {};
        return;
    }

    const int32 Thickness = std::clamp(HandleThickness, 0, std::max(0, Rect.GetWidth()));
    const int32 AvailableWidth = std::max(0, Rect.GetWidth() - Thickness);
    const int32 Minimum = std::min(MinimumSideSize, AvailableWidth / 2);
    const int32 FirstWidth = std::clamp(static_cast<int32>(std::round(AvailableWidth * Ratio.GetValue())), Minimum, AvailableWidth - Minimum);
    Ratio.SetValue(AvailableWidth > 0 ? static_cast<float>(FirstWidth) / AvailableWidth : 0.5f);

    const int32 SplitX = Rect.Min.X + FirstWidth;
    HandleRect = { { SplitX, Rect.Min.Y }, { SplitX + Thickness, Rect.Max.Y } };
    First->SetRect({ Rect.Min, { HandleRect.Min.X, Rect.Max.Y } });
    Second->SetRect({ { HandleRect.Max.X, Rect.Min.Y }, Rect.Max });
}
