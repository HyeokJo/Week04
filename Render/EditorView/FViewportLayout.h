#pragma once

#include <array>
#include <memory>
#include <vector>

#include "SSplitter.h"

using FViewportId = uint32;

enum class EViewportSplitDirection : uint8 {
    Horizontal,
    Vertical
};

enum class EViewportLayoutNodeType : uint8 {
    Leaf,
    Splitter
};

using FViewportSplitRatio = FSplitterRatio;

class IViewportLayoutNode : public SWindow {
public:
    virtual EViewportLayoutNodeType GetNodeType() const = 0;
};

class FViewportLeafNode final : public IViewportLayoutNode {
public:
    explicit FViewportLeafNode(FViewportId InViewportId);

    EViewportLayoutNodeType GetNodeType() const override;
    FViewportId GetViewportId() const;

private:
    FViewportId ViewportId = 0;
};

class FViewportSplitterNode final : public IViewportLayoutNode {
public:
    FViewportSplitterNode(EViewportSplitDirection InDirection, float InSplitRatio, std::unique_ptr<IViewportLayoutNode> InFirst, std::unique_ptr<IViewportLayoutNode> InSecond, std::shared_ptr<FViewportSplitRatio> InSharedSplitRatio = nullptr);

    EViewportLayoutNodeType GetNodeType() const override;
    void SetRect(const FRect& InRect) override;

    EViewportSplitDirection GetDirection() const;
    float GetSplitRatio() const;
    void SetSplitRatio(float InSplitRatio);
    bool SharesSplitRatioWith(const FViewportSplitterNode& Other) const;
    const FRect& GetHandleRect() const;
    void DragTo(FPoint Point);

    IViewportLayoutNode* GetFirst();
    const IViewportLayoutNode* GetFirst() const;
    IViewportLayoutNode* GetSecond();
    const IViewportLayoutNode* GetSecond() const;

private:
    friend class FViewportLayout;

    std::unique_ptr<IViewportLayoutNode>& GetFirstStorage();
    std::unique_ptr<IViewportLayoutNode>& GetSecondStorage();

    EViewportSplitDirection Direction = EViewportSplitDirection::Horizontal;
    std::unique_ptr<IViewportLayoutNode> First;
    std::unique_ptr<IViewportLayoutNode> Second;
    std::unique_ptr<SSplitter> Splitter;
};

class FViewportLayout {
public:
    static constexpr uint32 MaximumViewportCount = 4;

    FViewportLayout() = default;
    explicit FViewportLayout(std::unique_ptr<IViewportLayoutNode> InRoot);

    FViewportLayout(const FViewportLayout&) = delete;
    FViewportLayout& operator=(const FViewportLayout&) = delete;
    FViewportLayout(FViewportLayout&&) noexcept = default;
    FViewportLayout& operator=(FViewportLayout&&) noexcept = default;

    static FViewportLayout CreateFourPane(const std::array<FViewportId, MaximumViewportCount>& ViewportIds);

    void SetRect(const FRect& InRect);
    void RefreshLayout();
    bool SplitLeaf(FViewportId TargetViewportId, FViewportId NewViewportId, EViewportSplitDirection Direction, float SplitRatio = 0.5f, bool bInsertAfter = true);
    bool RemoveLeaf(FViewportId ViewportId);

    FViewportLeafNode* FindLeaf(FViewportId ViewportId);
    const FViewportLeafNode* FindLeaf(FViewportId ViewportId) const;
    void CollectLeaves(std::vector<FViewportLeafNode*>& OutLeaves);
    void CollectLeaves(std::vector<const FViewportLeafNode*>& OutLeaves) const;
    void CollectSplitters(std::vector<FViewportSplitterNode*>& OutSplitters);
    void CollectSplitters(std::vector<const FViewportSplitterNode*>& OutSplitters) const;
    uint32 GetLeafCount() const;
    IViewportLayoutNode* GetRoot();
    const IViewportLayoutNode* GetRoot() const;

private:
    static bool SplitLeafRecursive(std::unique_ptr<IViewportLayoutNode>& Node, FViewportId TargetViewportId, FViewportId NewViewportId, EViewportSplitDirection Direction, float SplitRatio, bool bInsertAfter);
    static bool RemoveLeafRecursive(std::unique_ptr<IViewportLayoutNode>& Node, FViewportId ViewportId);
    static FViewportLeafNode* FindLeafRecursive(IViewportLayoutNode* Node, FViewportId ViewportId);
    static const FViewportLeafNode* FindLeafRecursive(const IViewportLayoutNode* Node, FViewportId ViewportId);
    static void CollectLeavesRecursive(IViewportLayoutNode* Node, std::vector<FViewportLeafNode*>& OutLeaves);
    static void CollectLeavesRecursive(const IViewportLayoutNode* Node, std::vector<const FViewportLeafNode*>& OutLeaves);
    static void CollectSplittersRecursive(IViewportLayoutNode* Node, std::vector<FViewportSplitterNode*>& OutSplitters);
    static void CollectSplittersRecursive(const IViewportLayoutNode* Node, std::vector<const FViewportSplitterNode*>& OutSplitters);

    std::unique_ptr<IViewportLayoutNode> Root;
};
