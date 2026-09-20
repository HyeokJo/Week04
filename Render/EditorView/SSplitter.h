#pragma once

#include "SWindow.h"

class SSplitter : public SWindow {
public:
    void SetChildren(SWindow* InFirst, SWindow* InSecond) {
        First = InFirst;
        Second = InSecond;
    }

    void SetRatio(float InRatio);
    float GetRatio() const { return Ratio; }
    const FRect& GetHandleRect() const { return HandleRect; }

    virtual void DragTo(FPoint Point) = 0;

protected:
    virtual void UpdateLayout() = 0;

    SWindow* First{ nullptr };
    SWindow* Second{ nullptr };
    FRect HandleRect{};
    int32 HandleThickness{ 6 };
    float Ratio{ 0.5f };
    int32 MinimumSideSize{ 100 };
};

// 수평 분할: 첫 번째 자식은 위, 두 번째 자식은 아래에 배치한다.
class SSplitterH final : public SSplitter {
public:
    void SetRect(const FRect& InRect) override;
    void DragTo(FPoint Point) override;

private:
    void UpdateLayout() override;
};

// 수직 분할: 첫 번째 자식은 왼쪽, 두 번째 자식은 오른쪽에 배치한다.
class SSplitterV final : public SSplitter {
public:
    void SetRect(const FRect& InRect) override;
    void DragTo(FPoint Point) override;

private:
    void UpdateLayout() override;
};
