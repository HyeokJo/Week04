#include "PCH.h"

#include "FViewportPresetLayout.h"

FViewportPresetLayout::FViewportPresetLayout(EViewportLayoutPreset InPreset) {
    SetPreset(InPreset);
}

void FViewportPresetLayout::SetPreset(EViewportLayoutPreset InPreset) {
    Reset();
    Preset = InPreset < EViewportLayoutPreset::Count ? InPreset : EViewportLayoutPreset::Single;

    switch (Preset) {
    case EViewportLayoutPreset::Single:
        ViewportCount = 1;
        Root = &ViewportRegions[0];
        break;

    case EViewportLayoutPreset::TwoTopBottom:
        ViewportCount = 2;
        Root = AddTopBottomSplitter(&ViewportRegions[0], &ViewportRegions[1]);
        SetDrawOrder({ static_cast<SSplitter*>(Root) });
        break;

    case EViewportLayoutPreset::TwoLeftRight:
        ViewportCount = 2;
        Root = AddLeftRightSplitter(&ViewportRegions[0], &ViewportRegions[1]);
        SetDrawOrder({ static_cast<SSplitter*>(Root) });
        break;

    case EViewportLayoutPreset::ThreeLeftOneRightTwo: {
        ViewportCount = 3;
        SSplitter* Right = AddTopBottomSplitter(&ViewportRegions[1], &ViewportRegions[2]);
        Root = AddLeftRightSplitter(&ViewportRegions[0], Right);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Right });
        break;
    }

    case EViewportLayoutPreset::ThreeLeftTwoRightOne: {
        ViewportCount = 3;
        SSplitter* Left = AddTopBottomSplitter(&ViewportRegions[0], &ViewportRegions[1]);
        Root = AddLeftRightSplitter(Left, &ViewportRegions[2]);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Left });
        break;
    }

    case EViewportLayoutPreset::ThreeTopOneBottomTwo: {
        ViewportCount = 3;
        SSplitter* Bottom = AddLeftRightSplitter(&ViewportRegions[1], &ViewportRegions[2]);
        Root = AddTopBottomSplitter(&ViewportRegions[0], Bottom);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Bottom });
        break;
    }

    case EViewportLayoutPreset::ThreeTopTwoBottomOne: {
        ViewportCount = 3;
        SSplitter* Top = AddLeftRightSplitter(&ViewportRegions[0], &ViewportRegions[1]);
        Root = AddTopBottomSplitter(Top, &ViewportRegions[2]);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Top });
        break;
    }

    case EViewportLayoutPreset::FourGrid: {
        ViewportCount = 4;
        SharedGridRatio = FSplitterRatio(0.5f);
        SSplitter* Top = AddLeftRightSplitter(&ViewportRegions[0], &ViewportRegions[1], 0.5f, &SharedGridRatio);
        SSplitter* Bottom = AddLeftRightSplitter(&ViewportRegions[2], &ViewportRegions[3], 0.5f, &SharedGridRatio);
        Root = AddTopBottomSplitter(Top, Bottom);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Top, Bottom });
        break;
    }

    case EViewportLayoutPreset::FourLeftOneRightThree: {
        ViewportCount = 4;
        SWindow* Right = BuildTopBottomThree(1, 2, 3);
        Root = AddLeftRightSplitter(&ViewportRegions[0], Right);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Splitters[1].get(), Splitters[0].get() });
        break;
    }

    case EViewportLayoutPreset::FourLeftThreeRightOne: {
        ViewportCount = 4;
        SWindow* Left = BuildTopBottomThree(0, 1, 2);
        Root = AddLeftRightSplitter(Left, &ViewportRegions[3]);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Splitters[1].get(), Splitters[0].get() });
        break;
    }

    case EViewportLayoutPreset::FourTopOneBottomThree: {
        ViewportCount = 4;
        SWindow* Bottom = BuildLeftRightThree(1, 2, 3);
        Root = AddTopBottomSplitter(&ViewportRegions[0], Bottom);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Splitters[1].get(), Splitters[0].get() });
        break;
    }

    case EViewportLayoutPreset::FourTopThreeBottomOne: {
        ViewportCount = 4;
        SWindow* Top = BuildLeftRightThree(0, 1, 2);
        Root = AddTopBottomSplitter(Top, &ViewportRegions[3]);
        SetDrawOrder({ static_cast<SSplitter*>(Root), Splitters[1].get(), Splitters[0].get() });
        break;
    }

    default:
        ViewportCount = 1;
        Root = &ViewportRegions[0];
        break;
    }

    SetRect(Rect);
}

EViewportLayoutPreset FViewportPresetLayout::GetPreset() const {
    return Preset;
}

const char* FViewportPresetLayout::GetPresetName() const {
    switch (Preset) {
    case EViewportLayoutPreset::Single: return "1: Single";
    case EViewportLayoutPreset::TwoTopBottom: return "2: Top | Bottom";
    case EViewportLayoutPreset::TwoLeftRight: return "2: Left | Right";
    case EViewportLayoutPreset::ThreeLeftOneRightTwo: return "3: Left 1 | Right 2";
    case EViewportLayoutPreset::ThreeLeftTwoRightOne: return "3: Left 2 | Right 1";
    case EViewportLayoutPreset::ThreeTopOneBottomTwo: return "3: Top 1 | Bottom 2";
    case EViewportLayoutPreset::ThreeTopTwoBottomOne: return "3: Top 2 | Bottom 1";
    case EViewportLayoutPreset::FourGrid: return "4: Grid";
    case EViewportLayoutPreset::FourLeftOneRightThree: return "4: Left 1 | Right 3";
    case EViewportLayoutPreset::FourLeftThreeRightOne: return "4: Left 3 | Right 1";
    case EViewportLayoutPreset::FourTopOneBottomThree: return "4: Top 1 | Bottom 3";
    case EViewportLayoutPreset::FourTopThreeBottomOne: return "4: Top 3 | Bottom 1";
    default: return "Unknown";
    }
}

uint32 FViewportPresetLayout::GetViewportCount() const {
    return ViewportCount;
}

void FViewportPresetLayout::SetRect(const FRect& InRect) {
    Rect = InRect;
    if (Root != nullptr) {
        Root->SetRect(Rect);
    }
}

void FViewportPresetLayout::RefreshLayout() {
    SetRect(Rect);
}

const FRect& FViewportPresetLayout::GetViewportRect(FViewportId ViewportId) const {
    return ViewportRegions[ViewportId].GetRect();
}

void FViewportPresetLayout::CollectSplitters(std::vector<SSplitter*>& OutSplitters) const {
    for (uint32 Index = 0; Index < SplitterCount; ++Index) {
        OutSplitters.push_back(SplitterDrawOrder[Index]);
    }
}

SSplitter* FViewportPresetLayout::AddTopBottomSplitter(SWindow* First, SWindow* Second, float Ratio, const FSplitterRatio* SharedRatio) {
    return AddSplitter(std::make_unique<SSplitterH>(), First, Second, Ratio, SharedRatio);
}

SSplitter* FViewportPresetLayout::AddLeftRightSplitter(SWindow* First, SWindow* Second, float Ratio, const FSplitterRatio* SharedRatio) {
    return AddSplitter(std::make_unique<SSplitterV>(), First, Second, Ratio, SharedRatio);
}

SSplitter* FViewportPresetLayout::AddSplitter(std::unique_ptr<SSplitter> Splitter, SWindow* First, SWindow* Second, float Ratio, const FSplitterRatio* SharedRatio) {
    if (SplitterCount >= MaximumSplitterCount) {
        return nullptr;
    }

    Splitter->SetChildren(First, Second);
    Splitter->SetRatioState(SharedRatio != nullptr ? *SharedRatio : FSplitterRatio(Ratio));
    SSplitter* Result = Splitter.get();
    Splitters[SplitterCount++] = std::move(Splitter);
    return Result;
}

SWindow* FViewportPresetLayout::BuildTopBottomThree(FViewportId FirstId, FViewportId SecondId, FViewportId ThirdId) {
    SSplitter* LastTwo = AddTopBottomSplitter(&ViewportRegions[SecondId], &ViewportRegions[ThirdId]);
    return AddTopBottomSplitter(&ViewportRegions[FirstId], LastTwo, 1.0f / 3.0f);
}

SWindow* FViewportPresetLayout::BuildLeftRightThree(FViewportId FirstId, FViewportId SecondId, FViewportId ThirdId) {
    SSplitter* LastTwo = AddLeftRightSplitter(&ViewportRegions[SecondId], &ViewportRegions[ThirdId]);
    return AddLeftRightSplitter(&ViewportRegions[FirstId], LastTwo, 1.0f / 3.0f);
}

void FViewportPresetLayout::Reset() {
    Root = nullptr;
    SplitterCount = 0;
    SplitterDrawOrder.fill(nullptr);

    for (std::unique_ptr<SSplitter>& Splitter : Splitters) {
        Splitter.reset();
    }

    for (SWindow& Region : ViewportRegions) {
        Region.SetRect({});
    }
}

void FViewportPresetLayout::SetDrawOrder(std::initializer_list<SSplitter*> InSplitters) {
    uint32 Index = 0;
    for (SSplitter* Splitter : InSplitters) {
        SplitterDrawOrder[Index++] = Splitter;
    }
}
