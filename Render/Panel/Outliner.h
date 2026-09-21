#pragma once

#include "ImGui/imgui.h"
#include "Render/Panel/FEditorWindow.h"
#include "Scene/UWorld.h"

class FWorldEditorContext;

class FOutlinerPanel : public FEditorWindow {
public:
    FOutlinerPanel(UWorld& InWorld, FWorldEditorContext& InEditorContext);

private:
    void DrawContents() override;
    void PushWindowStyle() override;
    void PopWindowStyle() override;

    bool MatchesActor(const AActor& Actor) const;
    bool IsActorAttachedTo(const AActor& Actor, const AActor& ParentActor) const;
    bool IsRootActor(const AActor& Actor) const;
    bool HasActorChildren(const AActor& Actor) const;
    void DrawActorDragSource(AActor& Actor);
    void AcceptActorChildDrop(AActor& ParentActor);
    void DrawRootActorDropTarget();
    void HandleDeleteShortcut();
    void DrawActor(AActor& Actor);
    void DrawRootActors();

    UWorld* World;
    FWorldEditorContext* EditorContext;
    ImGuiTextFilter ActorFilter;
};
