#include "PCH.h"

#include "FViewportLayout.h"

FViewportLeafNode::FViewportLeafNode(FViewportId InViewportId) : ViewportId(InViewportId) {
}

EViewportLayoutNodeType FViewportLeafNode::GetNodeType() const {
    return EViewportLayoutNodeType::Leaf;
}

FViewportId FViewportLeafNode::GetViewportId() const {
    return ViewportId;
}

FViewportSplitterNode::FViewportSplitterNode(EViewportSplitDirection InDirection, float InSplitRatio, std::unique_ptr<IViewportLayoutNode> InFirst, std::unique_ptr<IViewportLayoutNode> InSecond, std::shared_ptr<FViewportSplitRatio> InSharedSplitRatio) : Direction(InDirection), First(std::move(InFirst)), Second(std::move(InSecond)) {
    if (Direction == EViewportSplitDirection::Horizontal) {
        Splitter = std::make_unique<SSplitterV>();
    }
    else {
        Splitter = std::make_unique<SSplitterH>();
    }

    Splitter->SetChildren(First.get(), Second.get());

    if (InSharedSplitRatio != nullptr) {
        Splitter->SetRatioState(*InSharedSplitRatio);
    }
    else {
        Splitter->SetRatioState(FViewportSplitRatio(InSplitRatio));
    }
}

EViewportLayoutNodeType FViewportSplitterNode::GetNodeType() const {
    return EViewportLayoutNodeType::Splitter;
}

void FViewportSplitterNode::SetRect(const FRect& InRect) {
    SWindow::SetRect(InRect);
    Splitter->SetChildren(First.get(), Second.get());
    Splitter->SetRect(InRect);
}

EViewportSplitDirection FViewportSplitterNode::GetDirection() const {
    return Direction;
}

float FViewportSplitterNode::GetSplitRatio() const {
    return Splitter->GetRatio();
}

void FViewportSplitterNode::SetSplitRatio(float InSplitRatio) {
    Splitter->SetRatio(InSplitRatio);
}

bool FViewportSplitterNode::SharesSplitRatioWith(const FViewportSplitterNode& Other) const {
    return Splitter->GetRatioState().SharesStateWith(Other.Splitter->GetRatioState());
}

const FRect& FViewportSplitterNode::GetHandleRect() const {
    return Splitter->GetHandleRect();
}

void FViewportSplitterNode::DragTo(FPoint Point) {
    Splitter->DragTo(Point);
}

IViewportLayoutNode* FViewportSplitterNode::GetFirst() {
    return First.get();
}

const IViewportLayoutNode* FViewportSplitterNode::GetFirst() const {
    return First.get();
}

IViewportLayoutNode* FViewportSplitterNode::GetSecond() {
    return Second.get();
}

const IViewportLayoutNode* FViewportSplitterNode::GetSecond() const {
    return Second.get();
}

std::unique_ptr<IViewportLayoutNode>& FViewportSplitterNode::GetFirstStorage() {
    return First;
}

std::unique_ptr<IViewportLayoutNode>& FViewportSplitterNode::GetSecondStorage() {
    return Second;
}

FViewportLayout::FViewportLayout(std::unique_ptr<IViewportLayoutNode> InRoot) : Root(std::move(InRoot)) {
}

FViewportLayout FViewportLayout::CreateFourPane(const std::array<FViewportId, MaximumViewportCount>& ViewportIds) {
    std::shared_ptr<FViewportSplitRatio> SharedColumnRatio = std::make_shared<FViewportSplitRatio>(0.5f);
    std::unique_ptr<IViewportLayoutNode> Top = std::make_unique<FViewportSplitterNode>(EViewportSplitDirection::Horizontal, 0.5f, std::make_unique<FViewportLeafNode>(ViewportIds[0]), std::make_unique<FViewportLeafNode>(ViewportIds[1]), SharedColumnRatio);
    std::unique_ptr<IViewportLayoutNode> Bottom = std::make_unique<FViewportSplitterNode>(EViewportSplitDirection::Horizontal, 0.5f, std::make_unique<FViewportLeafNode>(ViewportIds[2]), std::make_unique<FViewportLeafNode>(ViewportIds[3]), SharedColumnRatio);
    std::unique_ptr<IViewportLayoutNode> Root = std::make_unique<FViewportSplitterNode>(EViewportSplitDirection::Vertical, 0.5f, std::move(Top), std::move(Bottom));
    return FViewportLayout(std::move(Root));
}

void FViewportLayout::SetRect(const FRect& InRect) {
    if (Root != nullptr) {
        Root->SetRect(InRect);
    }
}

void FViewportLayout::RefreshLayout() {
    if (Root == nullptr) {
        return;
    }

    const FRect LayoutRect = Root->GetRect();
    Root->SetRect(LayoutRect);
}

bool FViewportLayout::SplitLeaf(FViewportId TargetViewportId, FViewportId NewViewportId, EViewportSplitDirection Direction, float SplitRatio, bool bInsertAfter) {
    if (Root == nullptr || GetLeafCount() >= MaximumViewportCount || FindLeaf(NewViewportId) != nullptr) {
        return false;
    }

    const FRect LayoutRect = Root->GetRect();
    if (!SplitLeafRecursive(Root, TargetViewportId, NewViewportId, Direction, SplitRatio, bInsertAfter)) {
        return false;
    }

    Root->SetRect(LayoutRect);
    return true;
}

bool FViewportLayout::RemoveLeaf(FViewportId ViewportId) {
    if (Root == nullptr || Root->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        return false;
    }

    const FRect LayoutRect = Root->GetRect();
    if (!RemoveLeafRecursive(Root, ViewportId)) {
        return false;
    }

    Root->SetRect(LayoutRect);
    return true;
}

FViewportLeafNode* FViewportLayout::FindLeaf(FViewportId ViewportId) {
    return FindLeafRecursive(Root.get(), ViewportId);
}

const FViewportLeafNode* FViewportLayout::FindLeaf(FViewportId ViewportId) const {
    return FindLeafRecursive(Root.get(), ViewportId);
}

void FViewportLayout::CollectLeaves(std::vector<FViewportLeafNode*>& OutLeaves) {
    CollectLeavesRecursive(Root.get(), OutLeaves);
}

void FViewportLayout::CollectLeaves(std::vector<const FViewportLeafNode*>& OutLeaves) const {
    CollectLeavesRecursive(Root.get(), OutLeaves);
}

void FViewportLayout::CollectSplitters(std::vector<FViewportSplitterNode*>& OutSplitters) {
    CollectSplittersRecursive(Root.get(), OutSplitters);
}

void FViewportLayout::CollectSplitters(std::vector<const FViewportSplitterNode*>& OutSplitters) const {
    CollectSplittersRecursive(Root.get(), OutSplitters);
}

uint32 FViewportLayout::GetLeafCount() const {
    std::vector<const FViewportLeafNode*> Leaves;
    CollectLeaves(Leaves);
    return static_cast<uint32>(Leaves.size());
}

IViewportLayoutNode* FViewportLayout::GetRoot() {
    return Root.get();
}

const IViewportLayoutNode* FViewportLayout::GetRoot() const {
    return Root.get();
}

bool FViewportLayout::SplitLeafRecursive(std::unique_ptr<IViewportLayoutNode>& Node, FViewportId TargetViewportId, FViewportId NewViewportId, EViewportSplitDirection Direction, float SplitRatio, bool bInsertAfter) {
    if (Node->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        FViewportLeafNode* Leaf = static_cast<FViewportLeafNode*>(Node.get());
        if (Leaf->GetViewportId() != TargetViewportId) {
            return false;
        }

        std::unique_ptr<IViewportLayoutNode> Existing = std::move(Node);
        std::unique_ptr<IViewportLayoutNode> Added = std::make_unique<FViewportLeafNode>(NewViewportId);
        std::unique_ptr<IViewportLayoutNode> First = bInsertAfter ? std::move(Existing) : std::move(Added);
        std::unique_ptr<IViewportLayoutNode> Second = bInsertAfter ? std::move(Added) : std::move(Existing);
        Node = std::make_unique<FViewportSplitterNode>(Direction, SplitRatio, std::move(First), std::move(Second));
        return true;
    }

    FViewportSplitterNode* Splitter = static_cast<FViewportSplitterNode*>(Node.get());
    return SplitLeafRecursive(Splitter->GetFirstStorage(), TargetViewportId, NewViewportId, Direction, SplitRatio, bInsertAfter) || SplitLeafRecursive(Splitter->GetSecondStorage(), TargetViewportId, NewViewportId, Direction, SplitRatio, bInsertAfter);
}

bool FViewportLayout::RemoveLeafRecursive(std::unique_ptr<IViewportLayoutNode>& Node, FViewportId ViewportId) {
    if (Node->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        return false;
    }

    FViewportSplitterNode* Splitter = static_cast<FViewportSplitterNode*>(Node.get());
    IViewportLayoutNode* First = Splitter->GetFirst();
    IViewportLayoutNode* Second = Splitter->GetSecond();

    if (First->GetNodeType() == EViewportLayoutNodeType::Leaf && static_cast<FViewportLeafNode*>(First)->GetViewportId() == ViewportId) {
        Node = std::move(Splitter->GetSecondStorage());
        return true;
    }

    if (Second->GetNodeType() == EViewportLayoutNodeType::Leaf && static_cast<FViewportLeafNode*>(Second)->GetViewportId() == ViewportId) {
        Node = std::move(Splitter->GetFirstStorage());
        return true;
    }

    return RemoveLeafRecursive(Splitter->GetFirstStorage(), ViewportId) || RemoveLeafRecursive(Splitter->GetSecondStorage(), ViewportId);
}

FViewportLeafNode* FViewportLayout::FindLeafRecursive(IViewportLayoutNode* Node, FViewportId ViewportId) {
    if (Node == nullptr) {
        return nullptr;
    }

    if (Node->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        FViewportLeafNode* Leaf = static_cast<FViewportLeafNode*>(Node);
        return Leaf->GetViewportId() == ViewportId ? Leaf : nullptr;
    }

    FViewportSplitterNode* Splitter = static_cast<FViewportSplitterNode*>(Node);
    FViewportLeafNode* FirstResult = FindLeafRecursive(Splitter->GetFirst(), ViewportId);
    return FirstResult != nullptr ? FirstResult : FindLeafRecursive(Splitter->GetSecond(), ViewportId);
}

const FViewportLeafNode* FViewportLayout::FindLeafRecursive(const IViewportLayoutNode* Node, FViewportId ViewportId) {
    if (Node == nullptr) {
        return nullptr;
    }

    if (Node->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        const FViewportLeafNode* Leaf = static_cast<const FViewportLeafNode*>(Node);
        return Leaf->GetViewportId() == ViewportId ? Leaf : nullptr;
    }

    const FViewportSplitterNode* Splitter = static_cast<const FViewportSplitterNode*>(Node);
    const FViewportLeafNode* FirstResult = FindLeafRecursive(Splitter->GetFirst(), ViewportId);
    return FirstResult != nullptr ? FirstResult : FindLeafRecursive(Splitter->GetSecond(), ViewportId);
}

void FViewportLayout::CollectLeavesRecursive(IViewportLayoutNode* Node, std::vector<FViewportLeafNode*>& OutLeaves) {
    if (Node == nullptr) {
        return;
    }

    if (Node->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        OutLeaves.push_back(static_cast<FViewportLeafNode*>(Node));
        return;
    }

    FViewportSplitterNode* Splitter = static_cast<FViewportSplitterNode*>(Node);
    CollectLeavesRecursive(Splitter->GetFirst(), OutLeaves);
    CollectLeavesRecursive(Splitter->GetSecond(), OutLeaves);
}

void FViewportLayout::CollectLeavesRecursive(const IViewportLayoutNode* Node, std::vector<const FViewportLeafNode*>& OutLeaves) {
    if (Node == nullptr) {
        return;
    }

    if (Node->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        OutLeaves.push_back(static_cast<const FViewportLeafNode*>(Node));
        return;
    }

    const FViewportSplitterNode* Splitter = static_cast<const FViewportSplitterNode*>(Node);
    CollectLeavesRecursive(Splitter->GetFirst(), OutLeaves);
    CollectLeavesRecursive(Splitter->GetSecond(), OutLeaves);
}

void FViewportLayout::CollectSplittersRecursive(IViewportLayoutNode* Node, std::vector<FViewportSplitterNode*>& OutSplitters) {
    if (Node == nullptr || Node->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        return;
    }

    FViewportSplitterNode* Splitter = static_cast<FViewportSplitterNode*>(Node);
    OutSplitters.push_back(Splitter);
    CollectSplittersRecursive(Splitter->GetFirst(), OutSplitters);
    CollectSplittersRecursive(Splitter->GetSecond(), OutSplitters);
}

void FViewportLayout::CollectSplittersRecursive(const IViewportLayoutNode* Node, std::vector<const FViewportSplitterNode*>& OutSplitters) {
    if (Node == nullptr || Node->GetNodeType() == EViewportLayoutNodeType::Leaf) {
        return;
    }

    const FViewportSplitterNode* Splitter = static_cast<const FViewportSplitterNode*>(Node);
    OutSplitters.push_back(Splitter);
    CollectSplittersRecursive(Splitter->GetFirst(), OutSplitters);
    CollectSplittersRecursive(Splitter->GetSecond(), OutSplitters);
}
