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

    /*void Initialize(HWND WindowHandle, std::int32_t Width, std::int32_t Height);
    void Shutdown();

    void RequestResize(std::int32_t Width, std::int32_t Height);
    bool ApplyPendingResize();

    void BeginBackBufferRender();
    void Present();*/


    void SetRenderer(FRenderer& InRenderer);

    FMessageChannel::FSender GetWindowToGraphicsSender();

    void Dispatch();

    /*ID3D11Device* GetDevice() const
    {
        return Device.Get();
    }

    ID3D11DeviceContext* GetDeviceContext() const
    {
        return DeviceContext.Get();
    }*/

private:
    /*void CreateDeviceAndSwapChain(
        HWND WindowHandle,
        std::int32_t Width,
        std::int32_t Height);*/

private:
/*    struct FSize
    {
        std::int32_t Width = 0;
        std::int32_t Height = 0;
    };

    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;

    std::unique_ptr<IRenderSurface> BackBufferSurface;

    FSize CurrentSize;
    std::optional<FSize> PendingSize;

    const float ClearColor[4] = { 0.2f, 0.2f, 0.7f, 1.0f };*/

    FRenderer* Renderer = nullptr; // 소유하지 않음

    // 현재 채널 구현에서 Capacity 0은 무제한
    FMessageChannel WindowToGraphics{};

    std::optional<FWindowResizeRequestMessage> PendingResize;
};