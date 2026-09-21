#pragma once

#include "Render/Panel/FEditorWindow.h"
#include "Render/Panel/Stats/StatWindow.h"
#include "FEditorInfo.h"
#include "../../Core/Console/Console.h"
#include "../../Core/Channel/FStateChannel.h"

class FStatPanel : public FEditorWindow {
public:
    explicit FStatPanel(UWorld& InWorld, FStateChannel<FStatDisplayFlags>::FReader InReader) :
        FEditorWindow("Stats"), World(&InWorld), ModeReader(std::move(InReader)){
        //bVisible = false;
    }

    bool CheckVisible()
    {
        return !(ModeReader.Peek().bShowFps || ModeReader.Peek().bShowMemory || ModeReader.Peek().bObjectSystem);
    }

private:
    void DrawContents() override {
        DrawStatContents(*World, ModeReader.Peek());
    }

    UWorld* World = nullptr;

    FStateChannel<FStatDisplayFlags>::FReader ModeReader;
    FStatDisplayFlags StatFlags;

};
