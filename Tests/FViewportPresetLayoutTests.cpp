#include "PCH.h"
#include "doctest.h"

#include "Render/EditorView/FViewportPresetLayout.h"

namespace {
struct FPresetCase {
    EViewportLayoutPreset Preset;
    uint32 ViewportCount;
    uint32 SplitterCount;
};

void CheckTopBottomStack(const FViewportPresetLayout& Layout, FViewportId FirstId, FViewportId SecondId, FViewportId ThirdId) {
    const FRect& First = Layout.GetViewportRect(FirstId);
    const FRect& Second = Layout.GetViewportRect(SecondId);
    const FRect& Third = Layout.GetViewportRect(ThirdId);

    CHECK(First.Min.X == Second.Min.X);
    CHECK(Second.Min.X == Third.Min.X);
    CHECK(First.Max.X == Second.Max.X);
    CHECK(Second.Max.X == Third.Max.X);
    CHECK(First.Max.Y < Second.Min.Y);
    CHECK(Second.Max.Y < Third.Min.Y);
}

void CheckLeftRightRow(const FViewportPresetLayout& Layout, FViewportId FirstId, FViewportId SecondId, FViewportId ThirdId) {
    const FRect& First = Layout.GetViewportRect(FirstId);
    const FRect& Second = Layout.GetViewportRect(SecondId);
    const FRect& Third = Layout.GetViewportRect(ThirdId);

    CHECK(First.Min.Y == Second.Min.Y);
    CHECK(Second.Min.Y == Third.Min.Y);
    CHECK(First.Max.Y == Second.Max.Y);
    CHECK(Second.Max.Y == Third.Max.Y);
    CHECK(First.Max.X < Second.Min.X);
    CHECK(Second.Max.X < Third.Min.X);
}
}

TEST_SUITE("Viewport Preset Layout") {
    TEST_CASE("Every supported preset has a fixed viewport and splitter count") {
        constexpr std::array Cases{
            FPresetCase{ EViewportLayoutPreset::Single, 1, 0 },
            FPresetCase{ EViewportLayoutPreset::TwoTopBottom, 2, 1 },
            FPresetCase{ EViewportLayoutPreset::TwoLeftRight, 2, 1 },
            FPresetCase{ EViewportLayoutPreset::ThreeLeftOneRightTwo, 3, 2 },
            FPresetCase{ EViewportLayoutPreset::ThreeLeftTwoRightOne, 3, 2 },
            FPresetCase{ EViewportLayoutPreset::ThreeTopOneBottomTwo, 3, 2 },
            FPresetCase{ EViewportLayoutPreset::ThreeTopTwoBottomOne, 3, 2 },
            FPresetCase{ EViewportLayoutPreset::FourGrid, 4, 3 },
            FPresetCase{ EViewportLayoutPreset::FourLeftOneRightThree, 4, 3 },
            FPresetCase{ EViewportLayoutPreset::FourLeftThreeRightOne, 4, 3 },
            FPresetCase{ EViewportLayoutPreset::FourTopOneBottomThree, 4, 3 },
            FPresetCase{ EViewportLayoutPreset::FourTopThreeBottomOne, 4, 3 }
        };

        for (const FPresetCase& Case : Cases) {
            FViewportPresetLayout Layout(Case.Preset);
            Layout.SetRect({ { 0, 0 }, { 1200, 900 } });

            CHECK(Layout.GetViewportCount() == Case.ViewportCount);

            std::vector<SSplitter*> Splitters;
            Layout.CollectSplitters(Splitters);
            CHECK(Splitters.size() == Case.SplitterCount);

            for (FViewportId Id = 0; Id < Case.ViewportCount; ++Id) {
                CHECK_FALSE(Layout.GetViewportRect(Id).IsEmpty());
            }

            for (FViewportId Id = Case.ViewportCount; Id < FViewportPresetLayout::MaximumViewportCount; ++Id) {
                CHECK(Layout.GetViewportRect(Id).IsEmpty());
            }
        }
    }

    TEST_CASE("Two and three viewport presets reproduce their named topology") {
        FViewportPresetLayout Layout(EViewportLayoutPreset::TwoTopBottom);
        Layout.SetRect({ { 0, 0 }, { 1200, 900 } });
        CHECK(Layout.GetViewportRect(0).Max.Y < Layout.GetViewportRect(1).Min.Y);

        Layout.SetPreset(EViewportLayoutPreset::TwoLeftRight);
        CHECK(Layout.GetViewportRect(0).Max.X < Layout.GetViewportRect(1).Min.X);

        Layout.SetPreset(EViewportLayoutPreset::ThreeLeftOneRightTwo);
        CHECK(Layout.GetViewportRect(0).Max.X < Layout.GetViewportRect(1).Min.X);
        CHECK(Layout.GetViewportRect(0).Max.X < Layout.GetViewportRect(2).Min.X);
        CHECK(Layout.GetViewportRect(1).Max.Y < Layout.GetViewportRect(2).Min.Y);

        Layout.SetPreset(EViewportLayoutPreset::ThreeLeftTwoRightOne);
        CHECK(Layout.GetViewportRect(0).Max.Y < Layout.GetViewportRect(1).Min.Y);
        CHECK(Layout.GetViewportRect(0).Max.X < Layout.GetViewportRect(2).Min.X);
        CHECK(Layout.GetViewportRect(1).Max.X < Layout.GetViewportRect(2).Min.X);

        Layout.SetPreset(EViewportLayoutPreset::ThreeTopOneBottomTwo);
        CHECK(Layout.GetViewportRect(0).Max.Y < Layout.GetViewportRect(1).Min.Y);
        CHECK(Layout.GetViewportRect(0).Max.Y < Layout.GetViewportRect(2).Min.Y);
        CHECK(Layout.GetViewportRect(1).Max.X < Layout.GetViewportRect(2).Min.X);

        Layout.SetPreset(EViewportLayoutPreset::ThreeTopTwoBottomOne);
        CHECK(Layout.GetViewportRect(0).Max.X < Layout.GetViewportRect(1).Min.X);
        CHECK(Layout.GetViewportRect(0).Max.Y < Layout.GetViewportRect(2).Min.Y);
        CHECK(Layout.GetViewportRect(1).Max.Y < Layout.GetViewportRect(2).Min.Y);
    }

    TEST_CASE("Four viewport presets support grid and one by three arrangements") {
        FViewportPresetLayout Layout(EViewportLayoutPreset::FourGrid);
        Layout.SetRect({ { 0, 0 }, { 1200, 900 } });

        std::vector<SSplitter*> Splitters;
        Layout.CollectSplitters(Splitters);
        REQUIRE(Splitters.size() == 3);
        Splitters[1]->DragTo({ 400, 100 });
        Layout.RefreshLayout();
        CHECK(Layout.GetViewportRect(0).Max.X == Layout.GetViewportRect(2).Max.X);

        Layout.SetPreset(EViewportLayoutPreset::FourLeftOneRightThree);
        CHECK(Layout.GetViewportRect(0).Max.X < Layout.GetViewportRect(1).Min.X);
        CheckTopBottomStack(Layout, 1, 2, 3);

        Layout.SetPreset(EViewportLayoutPreset::FourLeftThreeRightOne);
        CheckTopBottomStack(Layout, 0, 1, 2);
        CHECK(Layout.GetViewportRect(2).Max.X < Layout.GetViewportRect(3).Min.X);

        Layout.SetPreset(EViewportLayoutPreset::FourTopOneBottomThree);
        CHECK(Layout.GetViewportRect(0).Max.Y < Layout.GetViewportRect(1).Min.Y);
        CheckLeftRightRow(Layout, 1, 2, 3);

        Layout.SetPreset(EViewportLayoutPreset::FourTopThreeBottomOne);
        CheckLeftRightRow(Layout, 0, 1, 2);
        CHECK(Layout.GetViewportRect(2).Max.Y < Layout.GetViewportRect(3).Min.Y);
    }
}
