#include "PCH.h"
#include "../Console/ConsoleWindow.h"
#include "ImGui/imgui.h"
#include "../FEditorInfo.h"

#include <sstream>

// 로그 저장 시
using enum ELogLevel;
using enum ELogCategory;

namespace
{
    const char* LogCategoryToString(ELogCategory Category)
    {
        switch (Category)
        {
        case ELogCategory::Core:    return "[Core]";
        case ELogCategory::Render:  return "[Render]";
        case ELogCategory::Physics: return "[Physics]";
        case ELogCategory::Etc: return "[Etc]";
        default:                    return "[Unknown]";
        }
    }

    void ExecuteCommand(FConsoleOutputHandle Handle, const char* Input, FStateChannel<FStatDisplayFlags>::FWriter Writer)
    {
        std::istringstream Stream(Input);

        FString Command;
        //Stream >> Command;

        std::getline(Stream, Command);

        if (Command == "clear")
        {
            Console::Clear(Handle);
        }
        else if (Command == "help")
        {
            Console::AddLog(
                Handle,
                Log,
                Core,
                "Commands: clear, echo, error");
        }
        else if (Command == "error")
        {
            FString Text;
            std::getline(Stream >> std::ws, Text);

            Console::AddLog(
                Handle,
                Error,
                Etc,
                "Error Test");
        }
        else if (Command == "echo")
        {
            FString Text;
            std::getline(Stream >> std::ws, Text);

            Console::AddLog(
                Handle,
                Log,
                Core,
                "> %s",
                Text.c_str());
        }
        else if (Command == "stat fps")
        {
            Writer.Modify([](FStatDisplayFlags& Flags) { Flags.bShowFps = !Flags.bShowFps; });
        }
        else if (Command == "stat memory")
        {
            Writer.Modify([](FStatDisplayFlags& Flags) { Flags.bShowMemory = !Flags.bShowMemory; });
        }
        else if (Command == "stat object system")
        {
            Writer.Modify([](FStatDisplayFlags& Flags) { Flags.bObjectSystem = !Flags.bObjectSystem; });
        }
        else if (Command == "stat none")
        {
            Writer.Modify([](FStatDisplayFlags& Flags) { Flags.bShowFps = false; });
            Writer.Modify([](FStatDisplayFlags& Flags) { Flags.bShowMemory = false; });
            Writer.Modify([](FStatDisplayFlags& Flags) { Flags.bObjectSystem = false; });
        }
        else
        {
            Console::AddLog(
                Handle,
                Warning,
                Core,
                "Unknown command: %s",
                Command.c_str());
        }
    }
}

void DrawConsoleContents(FConsoleOutputHandle Handle, FStateChannel<FStatDisplayFlags>::FWriter Writer) {
    const size_t Count = Console::GetMessageCount(Handle);

    // 로그 영역
    float FooterHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f;

    ImGui::BeginChild(
        "LogRegion",
        ImVec2(0, -FooterHeight),
        true
    );

    Console::Flush(Handle);

    for (size_t Index = 0; Index < Count; ++Index)
    {
        const FConsoleMessage& Message = Console::GetMessageAt(Handle, Index);

        ImGui::TextUnformatted(Message.Time.c_str());
        ImGui::SameLine();

        const char* LevelText = "[Log]";
        ImVec4 LevelColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        switch (Message.Level)
        {
        case ELogLevel::Warning:
            LevelText = "[Warning]";
            LevelColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            break;

        case ELogLevel::Error:
            LevelText = "[Error]";
            LevelColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            break;

        case ELogLevel::Fatal:
            LevelText = "[Fatal]";
            LevelColor = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
            break;

        default:
            break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor);
        ImGui::TextUnformatted(LevelText);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImGui::TextUnformatted(LogCategoryToString(Message.Category));
        ImGui::SameLine();

        ImGui::TextUnformatted(Message.Text.c_str());
    }

    ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    static char InputBuf[256] = "";

    ImGui::Separator();

    if (ImGui::InputText(
        "##ConsoleInput",
        InputBuf,
        sizeof(InputBuf),
        ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (InputBuf[0] != '\0')
        {
            ExecuteCommand(Console::STDOutHandle, InputBuf, Writer);
            InputBuf[0] = '\0';

            ImGui::SetKeyboardFocusHere(-1);
        }
    }

    ImGui::TextDisabled("Type 'help' and press ENTER for available commands.");
}

