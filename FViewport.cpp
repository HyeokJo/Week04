#include "PCH.h"
#include "FViewport.h"

uint32 FViewport::GetId() const
{
    return Id;
}

EViewportType FViewport::GetType() const
{
    return Type;
}

void FViewport::SetHovered(bool bInHovered)
{
    bHovered = bInHovered;
}

void FViewport::SetFocused(bool bInFocused)
{
    bFocused = bInFocused;
}

bool FViewport::IsHovered() const
{
    return bHovered;
}

bool FViewport::IsFocused() const
{
    return bFocused;
}

FPoint FViewport::ToLocal(FPoint Position) const
{
    return FPoint{
        Position.X - Rect.Min.X,
        Position.Y - Rect.Min.Y
    };
}

CameraProbe FViewport::BuildCameraProbe() const
{
    CameraProbe Probe{};

    if (Rect.IsEmpty())
    {
        return Probe;
    }

    const float AspectRatio =
        static_cast<float>(Rect.GetWidth()) /
        static_cast<float>(Rect.GetHeight());


    // 채워야할 내용
    switch (Type)
    {
    case EViewportType::Perspective:
        break;

    case EViewportType::Top:
        break;

    case EViewportType::Front:
        break;

    case EViewportType::Side:
        break;
    }

    Probe.ViewProjection = Probe.View * Probe.Projection;

    return Probe;
}