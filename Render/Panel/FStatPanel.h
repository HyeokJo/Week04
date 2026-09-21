#pragma once

#include "Render/Panel/FEditorWindow.h"
#include "Render/Panel/Stats/StatWindow.h"

class FStatPanel : public FEditorWindow {
public:
    explicit FStatPanel(UWorld& InWorld) : FEditorWindow("Stats"), World(&InWorld) {
    }

private:
    void DrawContents() override {
        DrawStatContents(*World);
    }

    UWorld* World = nullptr;
};
