
// Macaw.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//
#include "PCH.h"
 
#include "framework.h"
#include "Macaw.h"

#include "Render/Renderer.h"
#include "Render/FLoadingScreen.h"

#include <d3d11.h>
#include <atomic>
#include <chrono>
#include <shellapi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "shell32.lib")

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
#ifdef OBJ_VIEWER
#include "FMouseCameraRotateRequestMessage.h"
#include "FKeyboardCameraMoveRequestMessage.h"
#include "FMouseCameraMoveRequestMessage.h"
#include "FMouseCameraDollyRequestMessage.h"
#endif
#include "FKeyboardInput.h"

#include "Core/Base/UndoSystem/FUndoSystem.h"
#include "Core/Base/UndoSystem/FUndoMessages.h"
#include "Serialize/FArchiveMemory.h"

//test
#include "Render/Pipeline/UPipeline.h"
#include "Core/Asset/UMesh.h"
#include "Core/Asset/UTexture.h"

#include "Render/EditorView/EditorViewport.h"
#include "Render/EditorView/FEditorViewport.h"

#include "Core/Asset/UFont.h"
#include "Core/Asset/UFreeTypeFont.h"
#include "Scene/Component/UBillBoardComponent.h"
#include "Scene/Component/UBillBoardTextComponent.h"
#include "Scene/Component/UNameTagComponent.h"
#include "Scene/Component/UScrollUVComponent.h"

#include "Serialize/FEditorConfigManager.h"
#include "Render/EditorView/FAssetThumbnailRenderer.h"


#include "Scene/Component/UBillboardComponent.h"
#include "Scene/Component/USubUVComponent.h"
#include "TObjectIterator.h"

#define MAX_LOADSTRING 100


#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")

constexpr bool WINDOWED = true;
constexpr uint32 DEFAULT_WINDOW_WIDTH = 1920;
constexpr uint32 DEFAULT_WINDOW_HEIGHT = 1080;
constexpr uint32 LoadingWindowWidth = 960;
constexpr uint32 LoadingWindowHeight = 540;

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

HWND hWnd = nullptr;

FMouseInput GMouseInput;
FKeyboardInput GKeyboardInput;
std::atomic<bool> GAcceptGameInput{};

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
HWND gHWND;
FRenderer Renderer;

namespace {
    constexpr bool bEnableSceneSave = true;

    struct FPendingExternalFileDrop {
        std::filesystem::path FilePath{};
        POINT ScreenPosition{};
    };

    std::vector<FPendingExternalFileDrop> PendingExternalFileDrops{};

    constexpr wchar_t ExternalDropOriginalWndProcProperty[] = L"Macaw.ExternalDropOriginalWndProc";

    void QueueExternalFileDrops(HWND WindowHandle, HDROP DropHandle) {
        POINT DropPosition{};
        DragQueryPoint(DropHandle, &DropPosition);
        ClientToScreen(WindowHandle, &DropPosition);

        const UINT FileCount = DragQueryFileW(DropHandle, 0xFFFFFFFF, nullptr, 0);
        for (UINT FileIndex = 0; FileIndex < FileCount; ++FileIndex) {
            const UINT CharacterCount = DragQueryFileW(DropHandle, FileIndex, nullptr, 0);
            std::wstring FilePath(CharacterCount + 1, L'\0');
            DragQueryFileW(DropHandle, FileIndex, FilePath.data(), CharacterCount + 1);
            FilePath.resize(CharacterCount);
            PendingExternalFileDrops.push_back({ std::filesystem::path{ FilePath }, DropPosition });
        }

        DragFinish(DropHandle);
    }

    LRESULT CALLBACK ExternalDropWndProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam) {
        const WNDPROC OriginalWndProc = reinterpret_cast<WNDPROC>(
            GetPropW(WindowHandle, ExternalDropOriginalWndProcProperty));

        if (Message == WM_DROPFILES) {
            QueueExternalFileDrops(WindowHandle, reinterpret_cast<HDROP>(WParam));
            return 0;
        }

        if (Message == WM_NCDESTROY) {
            SetWindowLongPtrW(WindowHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OriginalWndProc));
            RemovePropW(WindowHandle, ExternalDropOriginalWndProcProperty);
        }

        return OriginalWndProc != nullptr
            ? CallWindowProcW(OriginalWndProc, WindowHandle, Message, WParam, LParam)
            : DefWindowProcW(WindowHandle, Message, WParam, LParam);
    }

    void EnableExternalDropsForImGuiViewports() {
        for (ImGuiViewport* Viewport : ImGui::GetPlatformIO().Viewports) {
            HWND ViewportWindow = static_cast<HWND>(Viewport->PlatformHandle);
            if (ViewportWindow == nullptr) {
                continue;
            }

            DragAcceptFiles(ViewportWindow, TRUE);

            if (ViewportWindow == hWnd || GetPropW(ViewportWindow, ExternalDropOriginalWndProcProperty) != nullptr) {
                continue;
            }

            const WNDPROC OriginalWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(ViewportWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ExternalDropWndProc)));
            if (OriginalWndProc != nullptr) {
                SetPropW(
                    ViewportWindow,
                    ExternalDropOriginalWndProcProperty,
                    reinterpret_cast<HANDLE>(OriginalWndProc));
            }
        }
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

	struct FApplicationObjects {
		std::unique_ptr<UWorld> mWorld{};
		std::unique_ptr<FWorldEditorContext> mEditorContext{};
		std::unique_ptr<FAssetRegistry> mAssetRegistry{};
		std::unique_ptr<FAssetThumbnailRenderer> mThumbnailRenderer{};
		std::unique_ptr<FMessageChannel> mWorldCommandChannel{};
		std::unique_ptr<EditorViewport> mEditorView{};
		std::unique_ptr<FEditorUIManager> mEditorUIManager{};
		FEditorSettings mEditorSettings{};
	};

	void RegisterObjectTypes() {
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
		TypeRegistry::Register(UScrollUVComponent::StaticTypeInfo());
	}

	bool InitializeApplication(FApplicationObjects& Application, FLoadingProgress& Progress) {
		Progress.SetProgress(0.02f, "Registering object types");
		RegisterObjectTypes();

		Progress.SetProgress(0.06f, "Initializing renderer resources");
		if (!Renderer.Initialize()) {
			return false;
		}

		Progress.SetProgress(0.10f, "Creating world services");
		Application.mWorld = std::make_unique<UWorld>();
		Application.mEditorContext = std::make_unique<FWorldEditorContext>();
		Application.mAssetRegistry = std::make_unique<FAssetRegistry>();
		Application.mThumbnailRenderer = std::make_unique<FAssetThumbnailRenderer>();
		Application.mWorldCommandChannel = std::make_unique<FMessageChannel>(64);
		Application.mEditorView = std::make_unique<EditorViewport>();
		Application.mEditorUIManager = std::make_unique<FEditorUIManager>();

		Progress.SetProgress(0.13f, "Loading editor settings");
		if (!FEditorConfigManager::Load(Application.mEditorSettings)) {
			FEditorConfigManager::Save(Application.mEditorSettings);
		}

		Application.mEditorContext->SetEditorSettings(Application.mEditorSettings);
		Application.mWorld->SetEditorContext(Application.mEditorContext.get());
		Application.mEditorContext->SetWorld(Application.mWorld.get());

		const FAssetRegistry::FProgressCallback AssetProgressCallback{ [&Progress](float AssetProgress, const std::string& Status) {
			Progress.SetProgress(0.15f + AssetProgress * 0.55f, Status);
		} };

		const bool AssetsInitialized{ Application.mAssetRegistry->Initialize(Renderer.GetDevice(), 128, AssetProgressCallback) };
		if (!AssetsInitialized) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Some optional assets failed to load. Initialization will continue.");
		}

		Renderer.BindAssetRegistry(Application.mAssetRegistry.get());
		Application.mWorld->SetAssetRegistry(Application.mAssetRegistry.get());

		Progress.SetProgress(0.73f, "Initializing editor channels");
		Application.mEditorContext->InitializeChannels(*Application.mAssetRegistry, Renderer.GetDevice());
		GMouseInput.InitializeWorldCommandSender(Application.mWorldCommandChannel->GetSender());
		GKeyboardInput.InitializeWorldCommandSender(Application.mWorldCommandChannel->GetSender());
		Application.mWorldCommandChannel->TryBind<FMousePickRequestMessage>([&Application](const FMousePickRequestMessage& Message) { Application.mWorld->HandleMousePickRequest(Message); });

#ifdef OBJ_VIEWER
		Application.mWorldCommandChannel->TryBind<FMouseCameraRotateRequestMessage>([&Application](const FMouseCameraRotateRequestMessage& Message) { Application.mWorld->HandleMouseCameraRotateRequest(Message); });
		Application.mWorldCommandChannel->TryBind<FKeyboardCameraMoveRequestMessage>([&Application](const FKeyboardCameraMoveRequestMessage& Message) { Application.mWorld->HandleKeyboardCameraMoveRequest(Message); });
		Application.mWorldCommandChannel->TryBind<FMouseCameraMoveRequestMessage>([&Application](const FMouseCameraMoveRequestMessage& Message) { Application.mWorld->HandleMouseCameraMoveRequestMessage(Message); });
		Application.mWorldCommandChannel->TryBind<FMouseCameraDollyRequestMessage>([&Application](const FMouseCameraDollyRequestMessage& Message) { Application.mWorld->HandleMouseCameraDollyRequestMessage(Message); });
#endif

		Progress.SetProgress(0.78f, "Initializing editor view");
		Application.mEditorView->Initialize(Renderer.GetDevice(), *Application.mAssetRegistry, *Application.mEditorContext);

#ifdef OBJ_VIEWER
		Application.mEditorUIManager->InitializeViewer(*Application.mAssetRegistry, gHWND, *Application.mEditorContext);
#else
		Application.mEditorUIManager->Initialize(*Application.mWorld, Renderer, *Application.mAssetRegistry, *Application.mEditorContext, gHWND, Application.mEditorView->GetGizmoMode(), Application.mEditorView->GetGizmoCoordinateSpace(), Application.mThumbnailRenderer.get());
#endif

		Progress.SetProgress(0.86f, "Loading scene");
		const bool SceneLoaded{ Application.mWorld->LoadScene("./scenes/MainScene1.json", Renderer.GetDevice(), Application.mAssetRegistry.get()) };
		if (!SceneLoaded) {
			Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "The startup scene failed to load. Initialization will continue with an empty world.");
		}

		Progress.SetProgress(0.96f, "Finalizing assets");
		Application.mAssetRegistry->Finalize();
		Application.mThumbnailRenderer->Create(&Renderer, Application.mAssetRegistry.get());
		Progress.SetProgress(1.0f, "Ready");
		return true;
	}

	void RestoreGameWindow(HWND WindowHandle) {
		const DWORD Style{ WINDOWED ? WS_OVERLAPPEDWINDOW : WS_POPUP };
		const DWORD ExtendedStyle{ WINDOWED ? WS_EX_OVERLAPPEDWINDOW : WS_EX_APPWINDOW };
		RECT WindowRectangle{ 0, 0, static_cast<LONG>(DEFAULT_WINDOW_WIDTH), static_cast<LONG>(DEFAULT_WINDOW_HEIGHT) };

		if (WINDOWED) {
			AdjustWindowRectEx(&WindowRectangle, Style, FALSE, ExtendedStyle);
		}

		const int WindowWidth{ WindowRectangle.right - WindowRectangle.left };
		const int WindowHeight{ WindowRectangle.bottom - WindowRectangle.top };
		const int PositionX{ (GetSystemMetrics(SM_CXSCREEN) - WindowWidth) / 2 };
		const int PositionY{ (GetSystemMetrics(SM_CYSCREEN) - WindowHeight) / 2 };

		SetWindowLongPtrW(WindowHandle, GWL_STYLE, static_cast<LONG_PTR>(Style));
		SetWindowLongPtrW(WindowHandle, GWL_EXSTYLE, static_cast<LONG_PTR>(ExtendedStyle));
		SetWindowPos(WindowHandle, nullptr, PositionX, PositionY, WindowWidth, WindowHeight, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW);
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE PreviousInstance, _In_ LPWSTR CommandLine, _In_ int ShowCommand) {
	UNREFERENCED_PARAMETER(PreviousInstance);
	UNREFERENCED_PARAMETER(CommandLine);

	wcscpy_s(szTitle, MAX_LOADSTRING, L"Macaw Engine");
	wcscpy_s(szWindowClass, MAX_LOADSTRING, L"MacawEngineClass");
	MyRegisterClass(Instance);

	if (!InitInstance(Instance, ShowCommand)) {
		return FALSE;
	}

	Renderer.Create(gHWND, LoadingWindowWidth, LoadingWindowHeight);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(static_cast<void*>(hWnd));
	ImGui_ImplDX11_Init(Renderer.GetDevice(), Renderer.GetDeviceContext());
	ImGui::StyleColorsDark();

	ImGuiIO& Io{ ImGui::GetIO() };
	Io.Fonts->AddFontFromFileTTF("./Content/Font/NotoSansKR-Medium.ttf", 16.0f, nullptr, Io.Fonts->GetGlyphRangesKorean());

	const HACCEL AcceleratorTable{ LoadAccelerators(Instance, MAKEINTRESOURCE(IDC_MACAW)) };
	FApplicationObjects Application{};
	FLoadingScreen LoadingScreen{};

	if (!LoadingScreen.Run(Renderer, AcceleratorTable, [&Application](FLoadingProgress& Progress) { return InitializeApplication(Application, Progress); })) {
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		Renderer.BindAssetRegistry(nullptr);
		Application.mEditorUIManager.reset();
		Application.mEditorView.reset();
		Application.mThumbnailRenderer.reset();
		Application.mWorldCommandChannel.reset();
		Application.mWorld.reset();
		Application.mAssetRegistry.reset();
		Application.mEditorContext.reset();
		Renderer.Terminate();
		return FALSE;
	}

	RestoreGameWindow(hWnd);
	Io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	Io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	GAcceptGameInput.store(true, std::memory_order_release);
	Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Macaw Engine Initialized.");

	MSG Message{};
	bool Running{ true };
	auto LastTickTime{ std::chrono::steady_clock::now() };

	while (Running) {
		while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE)) {
			if (Message.message == WM_QUIT) {
				Running = false;
				break;
			}

			if (!TranslateAccelerator(Message.hwnd, AcceleratorTable, &Message)) {
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}
		}

		if (!Running) {
			break;
		}

		const auto CurrentTickTime{ std::chrono::steady_clock::now() };
		const float DeltaTime{ std::chrono::duration<float>(CurrentTickTime - LastTickTime).count() };
		LastTickTime = CurrentTickTime;

		Application.mThumbnailRenderer->Tick();
		Application.mEditorUIManager->RenderOffscreen(Renderer, *Application.mAssetRegistry);

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		Application.mEditorUIManager->Tick();

		for (const FPendingExternalFileDrop& Drop : PendingExternalFileDrops) {
			Application.mEditorUIManager->HandleExternalFileDrop(Drop.FilePath, ImVec2{ static_cast<float>(Drop.ScreenPosition.x), static_cast<float>(Drop.ScreenPosition.y) });
		}

		PendingExternalFileDrops.clear();

#ifndef OBJ_VIEWER
		FViewportHostWindow* ViewportHostWindow{ Application.mEditorUIManager->GetViewportHostWindow() };
		ViewportHostWindow->ProcessInput(*Application.mEditorView, GKeyboardInput, GMouseInput, DeltaTime);
		Application.mWorldCommandChannel->Dispatch();
		Application.mWorld->Tick(DeltaTime);
		Application.mEditorContext->Dispatch();

		for (FViewportId Id{}; Id < FViewportHostWindow::MaximumViewportCount; ++Id) {
			FEditorViewport* Viewport{ ViewportHostWindow->PrepareViewportForRender(Id) };
			if (Viewport == nullptr) {
				continue;
			}

			CameraProbe Camera{};
			if (!Viewport->BuildCameraProbe(Camera)) {
				continue;
			}

			FRenderProbe& Probe{ Application.mWorld->BuildRenderProbe() };
			Application.mEditorView->RenderInProbe(Probe, Camera, Viewport->GetRenderViewport());
			Renderer.RenderScene(Viewport->GetRenderSurface(), Probe, Camera, Viewport->GetRenderSettings());
			Application.mEditorView->RenderSceneGuides(Renderer.GetDeviceContext(), Camera, Viewport->GetCameraPosition(), Viewport->GetRenderViewport());
			Renderer.RenderGizmos(Viewport->GetRenderSurface(), Probe, Camera);
			Renderer.RenderText(Probe, Camera);
			Application.mEditorView->RenderOrientationAxis(Renderer.GetDeviceContext(), Camera);
		}
#endif

		ImGui::Render();
		Renderer.BeginUiRender();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		if (Io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			ImGui::UpdatePlatformWindows();
			EnableExternalDropsForImGuiViewports();
			ImGui::RenderPlatformWindowsDefault();
		}

		Renderer.EndFrame();
		GMouseInput.EndFrame();
	}

	Application.mEditorSettings = Application.mEditorContext->GetEditorSettings();
	if (FViewportHostWindow* Host{ Application.mEditorUIManager->GetViewportHostWindow() }) {
		Host->CaptureLayoutSettings(Application.mEditorSettings);
	}
	FEditorConfigManager::Save(Application.mEditorSettings);

	if constexpr (bEnableSceneSave) {
		Application.mWorld->SaveScene("test", Application.mAssetRegistry.get());
	}

	Application.mThumbnailRenderer->Terminate();
	Application.mEditorUIManager->ReleaseRenderResources();
	GAcceptGameInput.store(false, std::memory_order_release);
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	Renderer.BindAssetRegistry(nullptr);
	Application.mEditorUIManager.reset();
	Application.mEditorView.reset();
	Application.mThumbnailRenderer.reset();
	Application.mWorldCommandChannel.reset();
	Application.mWorld.reset();
	Application.mAssetRegistry.reset();
	Application.mEditorContext.reset();
	Renderer.Terminate();
	Renderer.ReportLiveObjects();
	return static_cast<int>(Message.wParam);
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
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
	hInst = hInstance;

	const DWORD Style{ WS_POPUP };
	const DWORD ExtendedStyle{ WS_EX_APPWINDOW };
	const int PositionX{ (GetSystemMetrics(SM_CXSCREEN) - static_cast<int>(LoadingWindowWidth)) / 2 };
	const int PositionY{ (GetSystemMetrics(SM_CYSCREEN) - static_cast<int>(LoadingWindowHeight)) / 2 };

	hWnd = CreateWindowExW(ExtendedStyle, szWindowClass, szTitle, Style, PositionX, PositionY, static_cast<int>(LoadingWindowWidth), static_cast<int>(LoadingWindowHeight), nullptr, nullptr, hInstance, nullptr);

	if (hWnd == nullptr) {
		const DWORD ErrorCode{ GetLastError() };
		OutputDebugStringA(("Window Creation Failed! Error Code: " + std::to_string(ErrorCode) + "\n").c_str());
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	DragAcceptFiles(hWnd, TRUE);
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

	if (GAcceptGameInput.load(std::memory_order_acquire)) {
		GMouseInput.ProcessWindowMessage(message, wParam, lParam);
		GKeyboardInput.ProcessWindowMessage(message, wParam, lParam);
	}

    switch (message)
    {
    case WM_DROPFILES:
    {
        const HDROP DropHandle = reinterpret_cast<HDROP>(wParam);
        QueueExternalFileDrops(hWnd, DropHandle);
        return 0;
    }
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
