#pragma once

#include "PCH.h"
#include "IEditorPanel.h"
#include "FControlPanel.h"
#include "FPropertyPanel.h"
#include "FConsolePanel.h"
#include "FStatPanel.h"
#include "FViewerPanel.h"
#include "FAssetBrowserPanel.h"
#include "Outliner.h"
#include "FViewerToolBar.h"

#include "Core/Channel/FStateChannel.h"
#include "../../Scene/FWorldEditorContext.h"

class FEditorUIManager
{
public:
    void Initialize(
        UWorld& World,

        FAssetRegistry& AssetRegistry,

        FWorldEditorContext& EditorContext,

        HWND WindowHandle,

        FStateChannel<uint8>::FReadWriter GizmoSender,
        FStateChannel<uint8>::FReadWriter GizmoCoordinateSpaceSender
    )
    {
        Panels.emplace_back(
            std::make_unique<FControlPanel>(
                EditorContext,
                WindowHandle,
                EditorContext.GetEditorToWorldSender()
            )
        );

        Panels.emplace_back(
            std::make_unique<FPropertyPanel>(
                EditorContext,
                std::move(GizmoSender),
                std::move(GizmoCoordinateSpaceSender)
            )
        );

        Panels.emplace_back(
            std::make_unique<FConsolePanel>(
                Console::STDOutHandle
            )
        );

        Panels.emplace_back(
            std::make_unique<FStatPanel>(World)
        );

        Panels.emplace_back(
            std::make_unique<FAssetBrowserPanel>(AssetRegistry)
        );

		Panels.emplace_back(
			std::make_unique<FOutlinerPanel>(World, EditorContext)
		);

        AddViewerPanel(AssetRegistry, WindowHandle, EditorContext);
    }

    void InitializeViewer(FAssetRegistry& AssetRegistry, HWND WindowHandle, FWorldEditorContext& EditorContext)
    {
        Panels.emplace_back(
            std::make_unique<FViewerToolBar>(EditorContext)
        );

        AddViewerPanel(AssetRegistry, WindowHandle, EditorContext);
    }

    void Tick()
    {
        for (const std::unique_ptr<IEditorPanel>& Panel : Panels)
        {
            if (Panel != nullptr &&
                Panel->IsVisible())
            {
                Panel->DrawPanel();
            }
        }
    }

    // 렌더 단계에서 호출한다. 오프스크린이 필요한 패널만 실제로 동작한다.
    void RenderOffscreen(FRenderer& Renderer, FAssetRegistry& AssetRegistry)
    {
        for (const std::unique_ptr<IEditorPanel>& Panel : Panels)
        {
            if (Panel != nullptr &&
                Panel->IsVisible())
            {
                Panel->RenderOffscreen(Renderer, AssetRegistry);
            }
        }
    }

private:
    void AddViewerPanel(FAssetRegistry& AssetRegistry, HWND WindowHandle, FWorldEditorContext& EditorContext)
    {
        Panels.emplace_back(
            std::make_unique<FViewerPanel>(AssetRegistry, WindowHandle, EditorContext.GetEditorToWorldSender())
        );
    }

    std::vector<std::unique_ptr<IEditorPanel>> Panels;
};
