// Macaw.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//
#include "PCH.h"
 
#include "framework.h"
#include "Macaw.h"

#include "Render/Renderer.h"

#include <d3d11.h>
#include <chrono>
#pragma comment(lib, "d3d11.lib")

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
  
#include "Core/Console/Console.h"
#include "Render/Panel/Console/ConsoleWindow.h"
#include "Core/Asset/FAssetRegistry.h"

#include "Render/Panel/Stats/StatWindow.h"

#include "Core/Base/FTransform.h"
#include "Scene/UWorld.h"
#include "Scene/AActor.h"
#include "Scene/Component/UCameraComponent.h"
#include "Scene/Component/UStaticMeshComponent.h"
#include "Scene/Component/UBoxColliderComponent.h"
#include "Scene/Component/UDirectionalLightComponent.h"
#include "Scene/Component/UPointLightComponent.h"
#include "Scene/Component/USpotLightComponent.h"
#include "Scene/FWorldEditorContext.h"

#include "Core/Base/TypeRegistry.h"

#include "Core/Channel/FMessageChannel.h"
#include "Core/Channel/FStateChannel.h"
#include "FMouseInput.h"
#include "Render/Panel/FEditorInfo.h"
#include "Render/Panel/FEditorUIManager.h"

#include "FMousePickRequestMessage.h"
#include "FMouseCameraRotateRequestMessage.h"
#include "FKeyboardInput.h"
#include "FKeyboardCameraMoveRequestMessage.h"
#include "FMouseCameraMoveRequestMessage.h"
#include "FMouseCameraDollyRequestMessage.h"

#include "Core/Base/UndoSystem/FUndoSystem.h"
#include "Core/Base/UndoSystem/FUndoMessages.h"
#include "Serialize/FArchiveMemory.h"

//test
#include "Render/Pipeline/UPipeline.h"
#include "Core/Asset/UMesh.h"
#include "Core/Asset/UTexture.h"

#include "Render/EditorView/EditorViewport.h"
#include "Render/EditorView/SSplitter.h"

#include "Core/Asset/UFont.h"
#include "Core/Asset/UFreeTypeFont.h"
#include "Scene/Component/UBillBoardComponent.h"
#include "Scene/Component/UBillBoardTextComponent.h"
#include "Scene/Component/UNameTagComponent.h"

#include "Serialize/FEditorConfigManager.h"

#include "Scene/Component/UBillboardComponent.h"
#include "Scene/Component/USubUVComponent.h"
#include "TObjectIterator.h"

#define MAX_LOADSTRING 100


#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")

constexpr bool WINDOWED = true;
constexpr uint32 DEFAULT_WINDOW_WIDTH = 1920;
constexpr uint32 DEFAULT_WINDOW_HEIGHT = 1080;

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

HWND hWnd = nullptr;

FMouseInput GMouseInput;
FKeyboardInput GKeyboardInput;

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
HWND gHWND;
FRenderer Renderer;

namespace {
    constexpr bool bEnableSceneSave = true;

    struct FViewportFrame {
        ImVec2 Position{};
        ImVec2 Size{};
        uint32 Width{ 0 };
        uint32 Height{ 0 };
        bool bVisible{ false };
        bool bHovered{ false };
        bool bFocused{ false };
    };

    bool DrawSplitterHandle(const char* Id, SSplitter& Splitter, ImGuiMouseCursor Cursor) {
        const FRect Rect = Splitter.GetHandleRect();
        if (Rect.IsEmpty()) {
            return false;
        }

        ImGui::SetCursorScreenPos(ImVec2(static_cast<float>(Rect.Min.X), static_cast<float>(Rect.Min.Y)));
        ImGui::InvisibleButton(Id, ImVec2(static_cast<float>(Rect.GetWidth()), static_cast<float>(Rect.GetHeight())));

        const bool bHovered = ImGui::IsItemHovered();
        const bool bActive = ImGui::IsItemActive();
        if (bHovered || bActive) {
            ImGui::SetMouseCursor(Cursor);
        }

        if (bActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 MousePosition = ImGui::GetMousePos();
            Splitter.DragTo({ static_cast<int32>(MousePosition.x), static_cast<int32>(MousePosition.y) });
        }

        const FRect UpdatedRect = Splitter.GetHandleRect();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(static_cast<float>(UpdatedRect.Min.X), static_cast<float>(UpdatedRect.Min.Y)),
            ImVec2(static_cast<float>(UpdatedRect.Max.X), static_cast<float>(UpdatedRect.Max.Y)),
            bActive ? IM_COL32(100, 150, 220, 255) : bHovered ? IM_COL32(85, 85, 85, 255) : IM_COL32(55, 55, 55, 255));
        return bActive;
    }

    void ConfigureTestStaticMesh(UStaticMeshComponent* MeshComponent, const FAssetHandle& MeshHandle, const FAssetHandle& PipelineHandle, const FAssetHandle& MaterialHandle, const FVector3& Location) {
        MeshComponent->SetMeshHandle(MeshHandle);
        MeshComponent->SetPipelineHandle(PipelineHandle);
        MeshComponent->SetMaterialHandle(MaterialHandle);
        MeshComponent->SetRelativeLocation(Location);
    }

    UStaticMeshComponent* AddTestStaticMesh(AActor* Actor, const FAssetHandle& MeshHandle, const FAssetHandle& PipelineHandle, const FAssetHandle& MaterialHandle, const FVector3& Location) {
        UStaticMeshComponent* MeshComponent = Actor->AddComponent<UStaticMeshComponent>();
        if (MeshComponent != nullptr) {
            ConfigureTestStaticMesh(MeshComponent, MeshHandle, PipelineHandle, MaterialHandle, Location);
        }
        return MeshComponent;
    }

    void AddTestCollider(AActor* Actor, USceneComponent* Parent, UMeshComponent* MeshComponent) {
        UBoxColliderComponent* Collider = Actor->AddComponent<UBoxColliderComponent>();
        if (Collider == nullptr || !Collider->AttachToComponent(Parent)) {
            return;
        }
        Collider->SetMeshComponent(MeshComponent);
    }

    void AddTestNameTag(AActor* Actor, USceneComponent* Root) {
        if (Actor == nullptr || Root == nullptr) {
            return;
        }

        UNameTagComponent* NameTag = Actor->AddComponent<UNameTagComponent>();
        NameTag->AttachToComponent(Root);
        NameTag->SetTargetActor(nullptr);
        NameTag->SetVisible(true);
        NameTag->SetActive(false);
    }

    void CreateComponentHierarchyTest(UWorld& World, const FAssetHandle& MeshHandle, const FAssetHandle& PipelineHandle, const FAssetHandle& MaterialHandle, const UMesh* Mesh) {
        AActor* Actor = World.AdoptActor<AActor>();
        UStaticMeshComponent* Root = AddTestStaticMesh(Actor, MeshHandle, PipelineHandle, MaterialHandle, { -12.0f, 0.0f, 8.0f });
        if (Root == nullptr || !Actor->SetRootComponent(Root)) {
            return;
        }

        USceneComponent* Parent = Root;
        for (uint32 Index = 0; Index < 4; ++Index) {
            UStaticMeshComponent* Child = AddTestStaticMesh(Actor, MeshHandle, PipelineHandle, MaterialHandle, { 0.0f, 0.0f, 2.0f });
            if (Child == nullptr || !Child->AttachToComponent(Parent)) {
                return;
            }
            Parent = Child;
        }

        AddTestCollider(Actor, Root, Root);
        AddTestNameTag(Actor, Root);
    }

    void CreateActorHierarchyTest(UWorld& World, const FAssetHandle& MeshHandle, const FAssetHandle& PipelineHandle, const FAssetHandle& MaterialHandle, const UMesh* Mesh) {
        AActor* ParentActor = World.AdoptActor<AActor>();
        UStaticMeshComponent* ParentRoot = AddTestStaticMesh(ParentActor, MeshHandle, PipelineHandle, MaterialHandle, { 12.0f, 0.0f, 8.0f });
        if (ParentRoot == nullptr || !ParentActor->SetRootComponent(ParentRoot)) {
            return;
        }
        AddTestCollider(ParentActor, ParentRoot, ParentRoot);
        AddTestNameTag(ParentActor, ParentRoot);

        AActor* ChildActor = World.AdoptActor<AActor>();
        UStaticMeshComponent* ChildRoot = AddTestStaticMesh(ChildActor, MeshHandle, PipelineHandle, MaterialHandle, { 0.0f, 0.0f, 3.0f });
        if (ChildRoot == nullptr || !ChildActor->SetRootComponent(ChildRoot)) {
            return;
        }

        if (!ChildRoot->AttachToComponent(ParentRoot)) {
            return;
        }

        AddTestCollider(ChildActor, ChildRoot, ChildRoot);
        AddTestNameTag(ChildActor, ChildRoot);
    }

    void CreateHierarchyTests(UWorld& World, const FAssetHandle& MeshHandle, const FAssetHandle& PipelineHandle, const FAssetHandle& MaterialHandle, const UMesh* Mesh) {
        CreateComponentHierarchyTest(World, MeshHandle, PipelineHandle, MaterialHandle, Mesh);
        CreateActorHierarchyTest(World, MeshHandle, PipelineHandle, MaterialHandle, Mesh);
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 여기에 코드를 입력합니다.
	TypeRegistry::Register(UObject::StaticTypeInfo());
	TypeRegistry::Register(UAsset::StaticTypeInfo());
    TypeRegistry::Register(UMesh::StaticTypeInfo());
    TypeRegistry::Register(UPipeline::StaticTypeInfo());
	TypeRegistry::Register(UTexture::StaticTypeInfo());
    TypeRegistry::Register(AActor::StaticTypeInfo());
    TypeRegistry::Register(UFont::StaticTypeInfo());
    TypeRegistry::Register(UFreeTypeFont::StaticTypeInfo());

	TypeRegistry::Register(UWorld::StaticTypeInfo());
	TypeRegistry::Register(UCameraComponent::StaticTypeInfo());
	TypeRegistry::Register(UStaticMeshComponent::StaticTypeInfo());
    TypeRegistry::Register(UCollisionComponent::StaticTypeInfo());
    TypeRegistry::Register(UBoxColliderComponent::StaticTypeInfo());
	TypeRegistry::Register(UDirectionalLightComponent::StaticTypeInfo());
	TypeRegistry::Register(UPointLightComponent::StaticTypeInfo());
	TypeRegistry::Register(USpotLightComponent::StaticTypeInfo());
	TypeRegistry::Register(UActorComponent::StaticTypeInfo());
	TypeRegistry::Register(USceneComponent::StaticTypeInfo());
    TypeRegistry::Register(UBillboardTextComponent::StaticTypeInfo());
    TypeRegistry::Register(UNameTagComponent::StaticTypeInfo());
	
    TypeRegistry::Register(UBillboardComponent::StaticTypeInfo());
    TypeRegistry::Register(USubUVComponent::StaticTypeInfo());

    // 전역 문자열을 초기화합니다.
    //LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    //LoadStringW(hInstance, IDC_MACAW, szWindowClass, MAX_LOADSTRING);
    wcscpy_s(szTitle, MAX_LOADSTRING, L"Macaw Engine");
    wcscpy_s(szWindowClass, MAX_LOADSTRING, L"MacawEngineClass");
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MACAW));

    MSG msg;

	Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Macaw Engine Initialized.");

    // test
    UWorld World{};
    FWorldEditorContext EditorContext{};
    World.SetEditorContext(&EditorContext);
	EditorContext.SetWorld(&World);

    Renderer.Create(gHWND, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);


    FAssetRegistry AssetRegistry;
    AssetRegistry.Initialize(Renderer.GetDevice(), 128);
    Renderer.BindAssetRegistry(&AssetRegistry);


    FMessageChannel WorldCommandChannel{ 64 };
    EditorContext.InitializeChannels(AssetRegistry, Renderer.GetDevice());

    

    EditorViewport EditorView{};
    EditorView.Initialize(Renderer.GetDevice(), AssetRegistry, Renderer.GetWindowInfoReader(), EditorContext);

    FEditorUIManager EditorUIManager;
    #ifdef OBJ_VIEWER
        EditorUIManager.InitializeViewer(AssetRegistry, gHWND, EditorContext);
    #else
        EditorUIManager.Initialize(World, AssetRegistry, EditorContext, gHWND, EditorView.GetGizmoMode(), EditorView.GetGizmoCoordinateSpace());
    #endif

    GMouseInput.InitializeWorldCommandSender(WorldCommandChannel.GetSender());
    GKeyboardInput.InitializeWorldCommandSender(WorldCommandChannel.GetSender());
	World.SetWindowInfoReader(Renderer.GetWindowInfoReader());
	World.SetAssetRegistry(&AssetRegistry);

    WorldCommandChannel.TryBind<FMousePickRequestMessage>(
        [&World](const FMousePickRequestMessage& Message) { World.HandleMousePickRequest(Message); });

    WorldCommandChannel.TryBind<FMouseCameraRotateRequestMessage>(
        [&World](const FMouseCameraRotateRequestMessage& Message)
        {
            World.HandleMouseCameraRotateRequest(Message);
        });

    WorldCommandChannel.TryBind<
        FKeyboardCameraMoveRequestMessage>(
            [&World](
                const FKeyboardCameraMoveRequestMessage& Message)
            {
                World.HandleKeyboardCameraMoveRequest(Message);
            });

    WorldCommandChannel.TryBind<
        FMouseCameraMoveRequestMessage>(
            [&World](
                const FMouseCameraMoveRequestMessage& Message)
            {
                World.HandleMouseCameraMoveRequestMessage(Message);
            });

    WorldCommandChannel.TryBind<
        FMouseCameraDollyRequestMessage>(
            [&World](
                const FMouseCameraDollyRequestMessage& Message)
            {
                World.HandleMouseCameraDollyRequestMessage(Message);
            });
	
	World.LoadScene("./scenes/NewScene.json", Renderer.GetDevice(), &AssetRegistry);

    AssetRegistry.Finalize(); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init((void*)hWnd);
    ImGui_ImplDX11_Init(Renderer.GetDevice(), Renderer.GetDeviceContext());
    
    ImGui::StyleColorsDark();
    
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // 창을 메인 윈도우 밖으로 끌면 ImGui 가 실제 OS 창을 만든다.
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    io.Fonts->AddFontFromFileTTF("./Content/Font/NotoSansKR-Medium.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesKorean());

    auto LastTickTime = std::chrono::steady_clock::now();

    std::array<SWindow, FRenderer::ViewportCount> ViewportRegions{};
    SSplitterH RootSplit{};
    SSplitterV TopSplit{};
    SSplitterV BottomSplit{};
    TopSplit.SetChildren(&ViewportRegions[0], &ViewportRegions[1]);
    BottomSplit.SetChildren(&ViewportRegions[2], &ViewportRegions[3]);
    RootSplit.SetChildren(&TopSplit, &BottomSplit);
    FRenderer::FViewportId ActiveViewportId{ 0 };

    while (true) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else {
            const auto CurrentTickTime = std::chrono::steady_clock::now();
            const float DeltaTime = std::chrono::duration<float>(CurrentTickTime - LastTickTime).count();
            LastTickTime = CurrentTickTime;

            // 오프스크린 패널은 ImGui 프레임이 시작되기 전에 그린다.
            // 여기서 서피스가 리사이즈되며 SRV 가 재생성될 수 있는데,
            // 그 뒤에 기록되는 드로우 명령이 항상 새 SRV 를 가리키게 하기 위함이다.
            EditorUIManager.RenderOffscreen(Renderer, AssetRegistry);

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			EditorUIManager.Tick();
			const ImGuiID DockSpaceId = EditorUIManager.GetDockSpaceId();

			std::array<FViewportFrame, FRenderer::ViewportCount> ViewportFrames{};
			#ifndef OBJ_VIEWER
			const ImVec2 MainViewportPosition = ImGui::GetMainViewport()->Pos;
			bool bSplitterActive = false;

			ImGui::SetNextWindowDockID(DockSpaceId, ImGuiCond_FirstUseEver);
			const bool bViewportWindowOpen = ImGui::Begin(
				"Viewports###SplitSceneViewport",
				nullptr,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

			if (bViewportWindowOpen) {
				const ImVec2 Origin = ImGui::GetCursorScreenPos();
				const ImVec2 AvailableSize = ImGui::GetContentRegionAvail();
				const int32 Width = static_cast<int32>(std::max(0.0f, AvailableSize.x));
				const int32 Height = static_cast<int32>(std::max(0.0f, AvailableSize.y));

				if (Width > 0 && Height > 0) {
					const FPoint Min{ static_cast<int32>(Origin.x), static_cast<int32>(Origin.y) };
					RootSplit.SetRect({ Min, { Min.X + Width, Min.Y + Height } });

					const bool bRootSplitActive = DrawSplitterHandle("##RootSplit", RootSplit, ImGuiMouseCursor_ResizeNS);
					const bool bTopSplitActive = DrawSplitterHandle("##TopSplit", TopSplit, ImGuiMouseCursor_ResizeEW);
					const bool bBottomSplitActive = DrawSplitterHandle("##BottomSplit", BottomSplit, ImGuiMouseCursor_ResizeEW);
					if (bTopSplitActive) {
						BottomSplit.SetRatio(TopSplit.GetRatio());
					}
					else if (bBottomSplitActive) {
						TopSplit.SetRatio(BottomSplit.GetRatio());
					}
					bSplitterActive = bRootSplitActive || bTopSplitActive || bBottomSplitActive;

					for (FRenderer::FViewportId Id = 0; Id < FRenderer::ViewportCount; ++Id) {
						FViewportFrame& Frame = ViewportFrames[Id];
						const FRect& Rect = ViewportRegions[Id].GetRect();
						if (Rect.IsEmpty()) {
							continue;
						}

						Frame.Position = ImVec2(static_cast<float>(Rect.Min.X), static_cast<float>(Rect.Min.Y));
						Frame.Size = ImVec2(static_cast<float>(Rect.GetWidth()), static_cast<float>(Rect.GetHeight()));
						Frame.Width = static_cast<uint32>(Rect.GetWidth());
						Frame.Height = static_cast<uint32>(Rect.GetHeight());
						Frame.bVisible = true;

						Renderer.ResizeSceneSurface(Id, Frame.Width, Frame.Height, Frame.Position.x - MainViewportPosition.x, Frame.Position.y - MainViewportPosition.y);
						ImGui::SetCursorScreenPos(Frame.Position);
						ImGui::Image(reinterpret_cast<ImTextureID>(Renderer.GetSceneShaderResourceView(Id)), Frame.Size);
						Frame.bHovered = !bSplitterActive && ImGui::IsItemHovered();
						if (Frame.bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
							ActiveViewportId = Id;
						}
					}

					const bool bParentFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
					for (FRenderer::FViewportId Id = 0; Id < FRenderer::ViewportCount; ++Id) {
						ViewportFrames[Id].bFocused = bParentFocused && Id == ActiveViewportId;
					}
				}
			}
			ImGui::End();

			const FViewportFrame& InputFrame = ViewportFrames[ActiveViewportId];
			if (InputFrame.bVisible) {
				Renderer.ResizeSceneSurface(ActiveViewportId, InputFrame.Width, InputFrame.Height, InputFrame.Position.x - MainViewportPosition.x, InputFrame.Position.y - MainViewportPosition.y);
			}

			const bool bBlockMouse = bSplitterActive || !InputFrame.bVisible || !InputFrame.bHovered;
			const bool bBlockKeyboard = bSplitterActive || !InputFrame.bVisible || !InputFrame.bFocused || ImGui::GetIO().WantCaptureKeyboard;
			EditorView.ProcessInput(GKeyboardInput, GMouseInput, bBlockMouse);
			GMouseInput.DispatchPendingWorldCommands(InputFrame.Width, InputFrame.Height, bBlockMouse);
			GKeyboardInput.DispatchPendingWorldCommands(DeltaTime, bBlockKeyboard);

            WorldCommandChannel.Dispatch();
            World.Tick(DeltaTime);
			EditorContext.Dispatch();

			for (FRenderer::FViewportId Id = 0; Id < FRenderer::ViewportCount; ++Id) {
				const FViewportFrame& Frame = ViewportFrames[Id];
				if (!Frame.bVisible) {
					continue;
				}

				Renderer.ResizeSceneSurface(Id, Frame.Width, Frame.Height, Frame.Position.x - MainViewportPosition.x, Frame.Position.y - MainViewportPosition.y);
				FRenderProbe& Probe = World.BuildRenderProbe();
				EditorView.RenderInProbe(Probe);
				Renderer.BeginSceneRender(Id);
				Renderer.RenderScene(Probe);
                EditorView.RenderSceneGuides(Renderer.GetDeviceContext(), Probe);
				Renderer.RenderGizmos(Probe);
				Renderer.RenderText(Probe);
                EditorView.RenderOrientationAxis(Renderer.GetDeviceContext(), Probe.MainCameraProbe);
			}
			#endif

			ImGui::Render();
			Renderer.BeginUiRender();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			// 메인 윈도우 밖으로 분리된 창들을 각자의 OS 창에 그린다.
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}

            Renderer.EndFrame();

            GMouseInput.EndFrame();
        }
    }
    
    FEditorConfigManager::Save(World.GetSettings());

    // ImGui 소멸
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if constexpr (bEnableSceneSave) {
		World.SaveScene("test", &AssetRegistry);
    }

    Renderer.Terminate();
    Renderer.ReportLiveObjects(); 
    return (int) msg.wParam;
}


//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MACAW));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   함수: InitInstance(HINSTANCE, int)
//
//   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   주석:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.


    if (WINDOWED) {
        DWORD style = WS_OVERLAPPEDWINDOW;
        DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;

        int posX = (GetSystemMetrics(SM_CXSCREEN) / 2) - (static_cast<int>(DEFAULT_WINDOW_WIDTH) / 2);
        int posY = (GetSystemMetrics(SM_CYSCREEN) / 2) - (static_cast<int>(DEFAULT_WINDOW_HEIGHT) / 2);

        RECT adjustedRect{ 0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT };
        ::AdjustWindowRectEx(std::addressof(adjustedRect), style, FALSE, exStyle);

        hWnd = CreateWindowEx(
            exStyle,                                // 확장 스타일
            szWindowClass,                          // 윈도우 클래스 이름
            szTitle,                                // 윈도우 타이틀 
            style,                                  // 윈도우 스타일
            posX, posY,                             // 위치
            adjustedRect.right - adjustedRect.left,
            adjustedRect.bottom - adjustedRect.top, // 크기
            nullptr,                                // 부모 윈도우
            nullptr,                                // 메뉴
            hInstance,                              // 인스턴스 핸들
            nullptr                                 // 추가 매개변수
        );
    }
    else {
        DWORD style = WS_POPUP;
        DWORD exStyle = NULL;

        int posX = (GetSystemMetrics(SM_CXSCREEN) / 2) - (static_cast<int>(DEFAULT_WINDOW_WIDTH) / 2);
        int posY = (GetSystemMetrics(SM_CYSCREEN) / 2) - (static_cast<int>(DEFAULT_WINDOW_HEIGHT) / 2);

        hWnd = CreateWindowEx(
            exStyle,                                // 확장 스타일
            szWindowClass,                          // 윈도우 클래스 이름
            szTitle,                                // 윈도우 타이틀 
            style,                                  // 윈도우 스타일
            posX, posY,                             // 위치 
            DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, // 크기
            nullptr,                                // 부모 윈도우
            nullptr,                                // 메뉴
            hInstance,                              // 인스턴스 핸들
            nullptr                                 // 추가 매개변수
        );
    }

    if (!hWnd) {
        // GetLastError()는 실패한 원인의 에러 코드를 반환합니다.
        DWORD errorCode = GetLastError();

        // errorCode가 1407 이면 -> "클래스를 찾을 수 없습니다" (1번 원인)
        // errorCode가 1400 이면 -> "잘못된 윈도우 핸들입니다"
        // errorCode를 구글이나 MS 공식 문서에 검색하면 원인이 바로 나옵니다.
        OutputDebugStringA(("Window Creation Failed! Error Code: " + std::to_string(errorCode) + "\n").c_str());
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

	gHWND = hWnd;

    return TRUE;
}

//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//
//  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
//  WM_PAINT    - 주 창을 그립니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (const auto result = ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
        return result;
    }

    GMouseInput.ProcessWindowMessage(message, wParam, lParam);

    GKeyboardInput.ProcessWindowMessage(message, wParam, lParam);

    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
			uint32 width = LOWORD(lParam);
			uint32 height = HIWORD(lParam);
			Renderer.ReSize(width, height);
		}
		break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


