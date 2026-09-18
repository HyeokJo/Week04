#include "PCH.h"
#include "FViewerToolBar.h"
#include "ImGui/imgui.h"
void FViewerToolBar::DrawPanel()
{
    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }

    if (ImGui::BeginMenu("Import"))
    {
        if (ImGui::MenuItem("Open Model..."))
        {
           
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();
    int RenderIndex = static_cast<int>(EditorContext->GetRenderModeState());
    const char* RenderModes[] = { "Lit", "Unlit", "Wireframe", "Lit Wireframe" };
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Render Mode", &RenderIndex, RenderModes, IM_ARRAYSIZE(RenderModes)))
    {
        EditorContext->SetRenderModeState(static_cast<size_t>(RenderIndex));
    }

    ImGui::EndMainMenuBar();
}
