#pragma once

#include "Render/Panel/FEditorWindow.h"
#include "Render/Panel/Console/ConsoleWindow.h"

class FConsolePanel : public FEditorWindow {
public:
    explicit FConsolePanel(FConsoleOutputHandle InHandle) : FEditorWindow("Console"), Handle(InHandle) {
    }

private:
    void DrawContents() override {
        DrawConsoleContents(Handle);
    }

    FConsoleOutputHandle Handle;
};
