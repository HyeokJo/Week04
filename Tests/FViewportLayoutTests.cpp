#include "PCH.h"
#include "doctest.h"

#include "Render/EditorView/FViewportLayout.h"

TEST_SUITE("Viewport Layout Tree") {
    TEST_CASE("Four pane layout reproduces the initial two by two geometry") {
        FViewportLayout Layout = FViewportLayout::CreateFourPane({ 0, 1, 2, 3 });
        Layout.SetRect({ { 0, 0 }, { 1000, 800 } });

        REQUIRE(Layout.GetLeafCount() == 4);

        const FRect& TopLeft = Layout.FindLeaf(0)->GetRect();
        const FRect& TopRight = Layout.FindLeaf(1)->GetRect();
        const FRect& BottomLeft = Layout.FindLeaf(2)->GetRect();
        const FRect& BottomRight = Layout.FindLeaf(3)->GetRect();

        CHECK(TopLeft.Min.X == 0);
        CHECK(TopLeft.Min.Y == 0);
        CHECK(TopLeft.Max.X == 497);
        CHECK(TopLeft.Max.Y == 397);
        CHECK(TopRight.Min.X == 503);
        CHECK(TopRight.Min.Y == 0);
        CHECK(BottomLeft.Min.X == 0);
        CHECK(BottomLeft.Min.Y == 403);
        CHECK(BottomRight.Min.X == 503);
        CHECK(BottomRight.Min.Y == 403);
        CHECK(BottomRight.Max.X == 1000);
        CHECK(BottomRight.Max.Y == 800);

        std::vector<FViewportSplitterNode*> Splitters;
        Layout.CollectSplitters(Splitters);
        REQUIRE(Splitters.size() == 3);
        CHECK_FALSE(Splitters[0]->SharesSplitRatioWith(*Splitters[1]));
        CHECK(Splitters[1]->SharesSplitRatioWith(*Splitters[2]));

        Splitters[1]->SetSplitRatio(0.25f);
        Layout.RefreshLayout();

        CHECK(Splitters[2]->GetSplitRatio() == doctest::Approx(Splitters[1]->GetSplitRatio()));
        CHECK(Layout.FindLeaf(0)->GetRect().Max.X == 249);
        CHECK(Layout.FindLeaf(2)->GetRect().Max.X == 249);
    }

    TEST_CASE("Splitting a leaf replaces it with a splitter and rejects duplicate ids") {
        FViewportLayout Layout(std::make_unique<FViewportLeafNode>(10));
        Layout.SetRect({ { 0, 0 }, { 600, 400 } });

        CHECK(Layout.SplitLeaf(10, 20, EViewportSplitDirection::Horizontal));
        CHECK(Layout.GetLeafCount() == 2);
        CHECK(Layout.FindLeaf(10) != nullptr);
        CHECK(Layout.FindLeaf(20) != nullptr);
        CHECK_FALSE(Layout.SplitLeaf(10, 20, EViewportSplitDirection::Vertical));
        CHECK_FALSE(Layout.SplitLeaf(99, 30, EViewportSplitDirection::Vertical));

        REQUIRE(Layout.GetRoot() != nullptr);
        CHECK(Layout.GetRoot()->GetNodeType() == EViewportLayoutNodeType::Splitter);
        CHECK(Layout.FindLeaf(10)->GetRect().Max.X == 297);
        CHECK(Layout.FindLeaf(20)->GetRect().Min.X == 303);

        REQUIRE(Layout.SplitLeaf(20, 30, EViewportSplitDirection::Horizontal));
        std::vector<FViewportSplitterNode*> Splitters;
        Layout.CollectSplitters(Splitters);
        REQUIRE(Splitters.size() == 2);
        CHECK_FALSE(Splitters[0]->SharesSplitRatioWith(*Splitters[1]));

        REQUIRE(Layout.SplitLeaf(30, 40, EViewportSplitDirection::Vertical));
        CHECK(Layout.GetLeafCount() == FViewportLayout::MaximumViewportCount);
        CHECK_FALSE(Layout.SplitLeaf(40, 50, EViewportSplitDirection::Horizontal));
        CHECK(Layout.FindLeaf(50) == nullptr);
    }

    TEST_CASE("Removing a leaf collapses its parent into the remaining sibling") {
        FViewportLayout Layout(std::make_unique<FViewportLeafNode>(0));
        REQUIRE(Layout.SplitLeaf(0, 1, EViewportSplitDirection::Horizontal));
        REQUIRE(Layout.SplitLeaf(1, 2, EViewportSplitDirection::Vertical));
        Layout.SetRect({ { 0, 0 }, { 900, 600 } });

        CHECK(Layout.RemoveLeaf(1));
        CHECK(Layout.GetLeafCount() == 2);
        CHECK(Layout.FindLeaf(1) == nullptr);
        CHECK(Layout.FindLeaf(0) != nullptr);
        CHECK(Layout.FindLeaf(2) != nullptr);
        CHECK(Layout.FindLeaf(2)->GetRect().Min.X == 453);
        CHECK(Layout.FindLeaf(2)->GetRect().Min.Y == 0);
        CHECK(Layout.FindLeaf(2)->GetRect().Max.Y == 600);

        CHECK(Layout.RemoveLeaf(0));
        CHECK(Layout.GetLeafCount() == 1);
        REQUIRE(Layout.GetRoot() != nullptr);
        CHECK(Layout.GetRoot()->GetNodeType() == EViewportLayoutNodeType::Leaf);
        CHECK(Layout.FindLeaf(2) != nullptr);
        CHECK(Layout.FindLeaf(2)->GetRect().Min.X == 0);
        CHECK(Layout.FindLeaf(2)->GetRect().Max.X == 900);
        CHECK_FALSE(Layout.RemoveLeaf(2));
    }
}
