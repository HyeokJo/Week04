#pragma once

#include "Swindow.h"
#include <Core/Base/FRenderProbe.h>

// 카메라 종류를 위한 타입
enum class EViewportType
{
    Perspective,
    Top,
    Front,
    Side
};

class FViewport: public SWindow
{
public:
    FViewport(uint32 InId, EViewportType InType) : Id(InId), Type(InType)
    {
    }
	uint32 GetId() const;
	EViewportType GetType() const;

    void SetHovered(bool bInHovered);
    void SetFocused(bool bInFocused);

    bool IsHovered() const;
    bool IsFocused() const;

    FPoint ToLocal(FPoint Position) const;
    CameraProbe BuildCameraProbe() const;


private:
    uint32 Id = 0;
    EViewportType Type;

    bool bVisible = false;
    bool bHovered = false;
    bool bFocused = false;
};