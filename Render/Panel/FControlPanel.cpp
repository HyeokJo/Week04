#include "PCH.h"
#include "FControlPanel.h"

#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <map>

#include "../../Serialize/FEditorConfigManager.h"
#include "../../Core/Base/TypeRegistry.h"
#include "../../Scene/AActor.h"
#include "../../Scene/Component/UActorComponent.h"
#include "../../Scene/Component/UStaticMeshComponent.h"
#include "../../Scene/UWorld.h"
#include "../Pipeline/UPipeline.h"

#include "../../Core/Console/Console.h"

void FControlPanel::DrawPanel()  
{
    // 전역 메뉴 바는 뷰포트의 상단에 고정되며 도킹 레이아웃의 일부가 아니다.
    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }

    const char* PrimitiveMeshTypes[] =
    {
        "/Game/System/Mesh/Cube.bin", "/Game/System/Mesh/Sphere.bin",
        "/Game/System/Mesh/Plane.bin", "/Game/System/Mesh/Cylinder.bin",
		"/Game/System/Mesh/Capsule.bin", "/Game/System/Mesh/Cone.bin",
		"/Game/System/Mesh/Torus.bin", "/Game/System/Mesh/Pyramid.bin"
    };

    // Create: 기존의 Primitive 생성/삭제 기능을 한 그룹으로 유지한다.
    if (ImGui::BeginMenu("Create"))
    {
        std::vector<const FTypeInfo*> SpawnableComponentTypes;
        for (const FTypeInfo* Type : TypeRegistry::GetRegisteredTypes())
        {
            if (Type != nullptr && Type->Creator != nullptr &&
                Type->IsA(UActorComponent::StaticTypeInfo()))
            {
                SpawnableComponentTypes.push_back(Type);
            }
        }

        ImGui::TextDisabled("Spawn Component");
        if (SpawnableComponentTypes.empty())
        {
            ImGui::TextDisabled("No spawnable component types are registered.");
        }
        else
        {
            if (SelectedComponentIndex < 0)
            {
                const auto StaticMeshType = std::ranges::find_if(SpawnableComponentTypes, [](const FTypeInfo* Type) {
                    return Type->IsA(UStaticMeshComponent::StaticTypeInfo());
                });

                SelectedComponentIndex = StaticMeshType != SpawnableComponentTypes.end()
                    ? static_cast<int>(std::distance(SpawnableComponentTypes.begin(), StaticMeshType))
                    : 0;
            }
            SelectedComponentIndex = std::clamp(
                SelectedComponentIndex, 0, static_cast<int>(SpawnableComponentTypes.size()) - 1);
            const FTypeInfo* SelectedComponentType = SpawnableComponentTypes[SelectedComponentIndex];

            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo("Component", SelectedComponentType->TypeName.data()))
            {
                for (int Index = 0; Index < static_cast<int>(SpawnableComponentTypes.size()); ++Index)
                {
                    const bool bIsSelected = Index == SelectedComponentIndex;
                    if (ImGui::Selectable(SpawnableComponentTypes[Index]->TypeName.data(), bIsSelected))
                    {
                        SelectedComponentIndex = Index;
                        SelectedComponentType = SpawnableComponentTypes[Index];
                    }

                    if (bIsSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const bool bIsStaticMesh = SelectedComponentType->IsA(UStaticMeshComponent::StaticTypeInfo());
            if (bIsStaticMesh)
            {
                ImGui::SetNextItemWidth(180.0f);
                ImGui::Combo("Mesh", &SelectedMeshIndex, PrimitiveMeshTypes, IM_ARRAYSIZE(PrimitiveMeshTypes));
            }

            ImGui::InputInt("Number of Objects to Spawn", &SpawnCountToRequest);
            if (SpawnCountToRequest < 1)
            {
                SpawnCountToRequest = 1;
            }

            if (ImGui::Button("Spawn Object(s)"))
            {
                EditorToWorldSender.TryEmplace<FMessageSpawnComponent>(
                    FString(SelectedComponentType->TypeName.data()),
                    FString(bIsStaticMesh ? PrimitiveMeshTypes[SelectedMeshIndex] : ""),
                    static_cast<uint32>(SpawnCountToRequest));
            }
        }

        ImGui::EndMenu();
    }

    // Scene: 저장과 불러오기, 씬 이름 편집을 기존과 같은 흐름으로 제공한다.
    if (ImGui::BeginMenu("Scene"))
    {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("Scene Name", SceneNameBuffer, IM_ARRAYSIZE(SceneNameBuffer));

        if (ImGui::Button("Save Scene"))
        {
            EditorToWorldSender.TryEmplace<FMessageSaveScene>(FString(SceneNameBuffer));
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Scene"))
        {
            const FString FilePath = OpenFileDialog();

            if (!FilePath.empty())
            {
                EditorToWorldSender.TryEmplace<FMessageLoadScene>(FString(FilePath));
            }

            size_t SlashPos = FilePath.find_last_of("\\/");
            std::string FileName;
            if (SlashPos != std::string::npos)
                FileName = FilePath.substr(SlashPos + 1);
            else
                FileName = FilePath;

            size_t DotPos = FileName.find_last_of('.');
            if (DotPos != std::string::npos)
                FileName = FileName.substr(0, DotPos);

            if (FileName.size() < sizeof(SceneNameBuffer))
                std::memcpy(SceneNameBuffer, FileName.data(), FileName.size() + 1);
        }

        ImGui::EndMenu();
    }

    // Components: Scene 전체 Component를 타입별로 묶어 Active 상태를 관리한다.
    if (ImGui::BeginMenu("Components"))
    {
        UWorld* World = EditorContext != nullptr ? EditorContext->GetWorld() : nullptr;
        if (World == nullptr)
        {
            ImGui::TextDisabled("World is unavailable.");
        }
        else
        {
            struct FComponentTypeState
            {
                size_t ActiveCount = 0;
                std::vector<UActorComponent*> Components;
            };

            std::map<FString, FComponentTypeState> ComponentsByType;
            for (const std::unique_ptr<AActor>& Actor : World->GetActors())
            {
                if (Actor == nullptr)
                {
                    continue;
                }

                for (const std::unique_ptr<UActorComponent>& Component : Actor->GetComponents())
                {
                    if (Component != nullptr)
                    {
                        FComponentTypeState& TypeState = ComponentsByType[FString(Component->GetTypeInfo()->TypeName.data())];
                        TypeState.Components.push_back(Component.get());
                        TypeState.ActiveCount += Component->IsActive() ? 1 : 0;
                    }
                }
            }

            ComponentFilter.Draw("Search types##SceneComponents", 240.0f);
            const auto IsTypeVisible = [this](const FString& TypeName) {
                return ComponentFilter.PassFilter(TypeName.c_str());
            };
            const auto SetVisibleTypesActive = [&ComponentsByType, &IsTypeVisible](bool bActive) {
                for (auto& [TypeName, TypeState] : ComponentsByType)
                {
                    if (!IsTypeVisible(TypeName))
                    {
                        continue;
                    }

                    for (UActorComponent* Component : TypeState.Components)
                    {
                        if (Component != nullptr)
                        {
                            Component->SetActive(bActive);
                        }
                    }
                }
            };

            if (ImGui::Button("Enable filtered"))
            {
                SetVisibleTypesActive(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Disable filtered"))
            {
                SetVisibleTypesActive(false);
            }
            ImGui::Separator();

            bool bHasVisibleType = false;
            for (const auto& [TypeName, TypeState] : ComponentsByType)
            {
                if (!IsTypeVisible(TypeName))
                {
                    continue;
                }

                bHasVisibleType = true;
                ImGui::PushID(TypeName.c_str());
                const size_t ComponentCount = TypeState.Components.size();
                const bool bAllActive = TypeState.ActiveCount == ComponentCount;
                const bool bMixed = TypeState.ActiveCount != 0 && !bAllActive;
                bool bTypeActive = bAllActive;
                ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, bMixed);
                FString Label = TypeName;
                Label += " (";
                Label += std::to_string(TypeState.ActiveCount).c_str();
                Label += "/";
                Label += std::to_string(ComponentCount).c_str();
                Label += ")";
                const bool bTypeChanged = ImGui::Checkbox(Label.c_str(), &bTypeActive);
                ImGui::PopItemFlag();
                if (bTypeChanged)
                {
                    for (UActorComponent* Component : TypeState.Components)
                    {
                        if (Component != nullptr)
                        {
                            Component->SetActive(bTypeActive);
                        }
                    }
                }
                ImGui::PopID();
            }

            if (!bHasVisibleType)
            {
                ImGui::TextDisabled("No component types match the current filter.");
            }
        }

        ImGui::EndMenu();
    }

    // Camera: Viewport camera navigation settings are editor-owned.
    if (ImGui::BeginMenu("Camera"))
    {
        const FEditorSettings Settings = EditorContext->GetEditorSettings();
        float MoveSensitivity = Settings.MoveSensitivity;
        if (ImGui::SliderFloat("MoveSensitivity", &MoveSensitivity, 1.f, 100.0f))
        {
            EditorContext->SetMoveSensitivity(MoveSensitivity);
        }

        float RotationSensitivity = Settings.RotationSensitivity;
        if (ImGui::SliderFloat("RotationSensitivity", &RotationSensitivity, 0.1f, 5.0f))
        {
            EditorContext->SetRotationSensitivity(RotationSensitivity);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Grid"))
    {
        float GridSize = EditorContext->GetEditorSettings().GridSize;
        if (ImGui::SliderFloat("GridSize", &GridSize, 0.1f, 100.0f))
        {
            EditorContext->SetGridSize(GridSize);
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();
    const ERenderMode RenderModeValues[] = {
        ERenderMode::Lit,
        ERenderMode::Unlit,
        ERenderMode::Wireframe,
        ERenderMode::LitWireframe
    };
    int RenderIndex = 0;
    for (int Index = 0; Index < IM_ARRAYSIZE(RenderModeValues); ++Index) {
        if (EditorContext->GetRenderModeState() == static_cast<size_t>(RenderModeValues[Index])) {
            RenderIndex = Index;
            break;
        }
    }
    const char* RenderModes[] = { "Lit", "Unlit", "Wireframe", "Lit Wireframe" };
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Render Mode", &RenderIndex, RenderModes, IM_ARRAYSIZE(RenderModes)))
    {
        EditorContext->SetRenderModeState(static_cast<size_t>(RenderModeValues[RenderIndex]));
    }

    if (ImGui::Button("Import"))
    {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Import Button Click");
        

        OPENFILENAMEA OpenFileName = { 0 };

        OpenFileName.lStructSize = sizeof(OpenFileName);
        OpenFileName.hwndOwner = WindowHandle;

        OpenFileName.lpstrFilter = "OBJ Files(*.obj)\0*.obj\0All Files(*.*)\0*.*\0";

        OpenFileName.nMaxFile = MAX_PATH;

        OpenFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        OpenFileName.lpstrDefExt = "obj";

        FString FilePath = OpenFileDialog(FString("./Content/ModelingFiles"), OpenFileName);

        EditorToWorldSender.TryEmplace<FMessageImportMesh>(FString("ObjImport"), FString(FilePath), FString("./Content/Metadata/MonkeyMesh.meta"));
         
    }

    // 남은 공간의 오른쪽 끝에 성능 정보를 고정한다.
    const char* FpsText = "FPS: %.1f";
    const float FpsWidth = ImGui::CalcTextSize("FPS: 000.0").x;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - FpsWidth - ImGui::GetStyle().WindowPadding.x);
    ImGui::Text(FpsText, ImGui::GetIO().Framerate);




   

    ImGui::EndMainMenuBar();
}



FString FControlPanel::OpenFileDialog() {
    char FileName[MAX_PATH] = { 0 };
    OPENFILENAMEA OpenFileName = { 0 };

    OpenFileName.lStructSize = sizeof(OpenFileName);
    OpenFileName.hwndOwner = WindowHandle;

    OpenFileName.lpstrFilter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    OpenFileName.lpstrFile = FileName;
    OpenFileName.nMaxFile = MAX_PATH;

    OpenFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    OpenFileName.lpstrDefExt = "json";

    std::string InitialDirectoryPath = std::filesystem::absolute("./scenes").string();

    if (!std::filesystem::exists(InitialDirectoryPath))
    {
        std::filesystem::create_directories(InitialDirectoryPath);
    }

    OpenFileName.lpstrInitialDir = InitialDirectoryPath.c_str();

    if (GetOpenFileNameA(&OpenFileName))
    {
        return FString(FileName);
    }

    return "";
}

FString FControlPanel::OpenFileDialog(const FString& FilePath, const OPENFILENAMEA& OFN)
{
    char FileName[MAX_PATH] = { 0 };

    OPENFILENAMEA OpenFileName = OFN;

    OpenFileName.lpstrFile = FileName;

    std::string InitialDirectoryPath = std::filesystem::absolute(FilePath.c_str()).string();

    if (!std::filesystem::exists(InitialDirectoryPath))
    {
        std::filesystem::create_directories(InitialDirectoryPath);
    }

    OpenFileName.lpstrInitialDir = InitialDirectoryPath.c_str();

    if (GetOpenFileNameA(&OpenFileName))
    {
        return FString(FileName);
    }

    return "";
}
