#pragma once

#include "Render/Panel/FEditorWindow.h"
#include "Render/Panel/Console/ConsoleWindow.h"
#include "FEditorInfo.h"

class FConsolePanel : public FEditorWindow {
public:
    explicit FConsolePanel(FConsoleOutputHandle InHandle, FStateChannel<FStatDisplayFlags>::FWriter Writer)
        : FEditorWindow("Console"), Handle(InHandle), ModeWriter(std::move(Writer)) {
    }

private:
    void DrawContents() override {
        DrawConsoleContents(Handle, ModeWriter);
    }

    FConsoleOutputHandle Handle;

    FStateChannel<FStatDisplayFlags>::FWriter ModeWriter;
};
