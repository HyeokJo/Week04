#pragma once

#include <d3d11.h>

#include <memory>
#include <optional>
#include <wrl/client.h>

#include "Core/Channel/FMessageChannel.h"
#include "Render/IRenderSurface.h"
#include "FWindowResizeRequestMessage.h"


class FRenderer;

class FGraphicsContext
{
public:
    FGraphicsContext();
    ~FGraphicsContext() = default;

    FGraphicsContext(const FGraphicsContext&) = delete;
    FGraphicsContext& operator=(const FGraphicsContext&) = delete;

    FGraphicsContext(FGraphicsContext&&) = delete;
    FGraphicsContext& operator=(FGraphicsContext&&) = delete;

    void SetRenderer(FRenderer& InRenderer);

    FMessageChannel::FSender GetWindowToGraphicsSender();

    void Dispatch();

private:

    FRenderer* Renderer = nullptr; // 소유하지 않음

    // 현재 채널 구현에서 Capacity 0은 무제한
    FMessageChannel WindowToGraphics{};

    std::optional<FWindowResizeRequestMessage> PendingResize;
};