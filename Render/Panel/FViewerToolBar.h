#pragma once
#include "Render/Panel/IEditorPanel.h"

#include "../../Scene/FWorldEditorContext.h"

class FViewerToolBar final : public IEditorPanel 
{
public:
	FViewerToolBar(FWorldEditorContext& InEditorContext) : EditorContext(&InEditorContext) 
	{};

	~FViewerToolBar() = default;


	FViewerToolBar(const FViewerToolBar&) = delete;
	FViewerToolBar& operator=(const FViewerToolBar&) = delete;

	FViewerToolBar(FViewerToolBar&&) = delete;
	FViewerToolBar& operator=(FViewerToolBar&&) = delete;

	void DrawPanel() override;
private:
	FWorldEditorContext* EditorContext = nullptr;

};