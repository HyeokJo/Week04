#pragma once

#include <array>
#include <initializer_list>
#include <memory>
#include <vector>

#include "FEditorViewportTypes.h"
#include "SSplitter.h"

enum class EViewportLayoutPreset : uint8 {
    Single,
    TwoTopBottom,
    TwoLeftRight,
    ThreeLeftOneRightTwo,
    ThreeLeftTwoRightOne,
    ThreeTopOneBottomTwo,
    ThreeTopTwoBottomOne,
    FourGrid,
    FourLeftOneRightThree,
    FourLeftThreeRightOne,
    FourTopOneBottomThree,
    FourTopThreeBottomOne,
    Count
};

class FViewportPresetLayout {
public:
    static constexpr uint32 MaximumViewportCount = 4;
    static constexpr uint32 MaximumSplitterCount = 3;

    explicit FViewportPresetLayout(EViewportLayoutPreset InPreset = EViewportLayoutPreset::FourGrid);

    void SetPreset(EViewportLayoutPreset InPreset);
    EViewportLayoutPreset GetPreset() const;
    const char* GetPresetName() const;
    uint32 GetViewportCount() const;

    void SetRect(const FRect& InRect);
    void RefreshLayout();
    const FRect& GetViewportRect(FViewportId ViewportId) const;
    void CollectSplitters(std::vector<SSplitter*>& OutSplitters) const;

private:
    SSplitter* AddTopBottomSplitter(SWindow* First, SWindow* Second, float Ratio = 0.5f, const FSplitterRatio* SharedRatio = nullptr);
    SSplitter* AddLeftRightSplitter(SWindow* First, SWindow* Second, float Ratio = 0.5f, const FSplitterRatio* SharedRatio = nullptr);
    SSplitter* AddSplitter(std::unique_ptr<SSplitter> Splitter, SWindow* First, SWindow* Second, float Ratio, const FSplitterRatio* SharedRatio);
    SWindow* BuildTopBottomThree(FViewportId FirstId, FViewportId SecondId, FViewportId ThirdId);
    SWindow* BuildLeftRightThree(FViewportId FirstId, FViewportId SecondId, FViewportId ThirdId);
    void Reset();
    void SetDrawOrder(std::initializer_list<SSplitter*> InSplitters);

    EViewportLayoutPreset Preset = EViewportLayoutPreset::FourGrid;
    uint32 ViewportCount = MaximumViewportCount;
    FRect Rect{};
    std::array<SWindow, MaximumViewportCount> ViewportRegions{};
    std::array<std::unique_ptr<SSplitter>, MaximumSplitterCount> Splitters{};
    std::array<SSplitter*, MaximumSplitterCount> SplitterDrawOrder{};
    uint32 SplitterCount = 0;
    SWindow* Root = nullptr;
    FSplitterRatio SharedGridRatio{};
};
