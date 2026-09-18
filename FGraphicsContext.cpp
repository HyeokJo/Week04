#include "PCH.h"

#include "FGraphicsContext.h"
#include "Render/FSceneRenderSurface.h"
#include "ErrorHandler.h"
#include "Render/Renderer.h"


/*FGraphicsContext::~FGraphicsContext()
{
    Shutdown();
}

void FGraphicsContext::Initialize(
    HWND WindowHandle,
    std::int32_t Width,
    std::int32_t Height)
{
    CreateDeviceAndSwapChain(WindowHandle, Width, Height);

    auto Surface = std::make_unique<FSceneRenderSurface>();
    Surface->InitializeSwapChain(Device.Get(), SwapChain.Get());

    const D3D11_VIEWPORT& Viewport = Surface->GetViewport();

    CurrentSize = {
        static_cast<std::int32_t>(Viewport.Width),
        static_cast<std::int32_t>(Viewport.Height)
    };

    BackBufferSurface = std::move(Surface);
};

void FGraphicsContext::RequestResize(
    std::int32_t Width,
    std::int32_t Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    // 실제 GPU 자원 변경은 여기서 하지 않는다.
    PendingSize = FSize{ Width, Height };
}

bool FGraphicsContext::ApplyPendingResize()
{
    if (!PendingSize.has_value())
    {
        return true;
    }

    if (!Device || !DeviceContext || !SwapChain || !BackBufferSurface)
    {
        // 초기화 전 요청은 유지한다.
        return false;
    }

    const FSize Requested = *PendingSize;

    if (BackBufferSurface->IsValid() &&
        Requested.Width == CurrentSize.Width &&
        Requested.Height == CurrentSize.Height)
    {
        PendingSize.reset();
        return true;
    }

    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    // 기존 Surface 내부에서 자원 해제 → ResizeBuffers → 재생성 수행.
    if (!BackBufferSurface->Resize(
        Device.Get(),
        Requested.Width,
        Requested.Height))
    {
        return false;
    }

    CurrentSize = Requested;
    PendingSize.reset();

    return true;
}

void FGraphicsContext::BeginBackBufferRender()
{
    BackBufferSurface->Bind(DeviceContext.Get());
    BackBufferSurface->Clear(DeviceContext.Get(), ClearColor);
}

void FGraphicsContext::Present()
{
    SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
}

void FGraphicsContext::Shutdown()
{
    if (DeviceContext)
    {
        DeviceContext->ClearState();
    }

    if (BackBufferSurface)
    {
        BackBufferSurface->Reset();
        BackBufferSurface.reset();
    }

    SwapChain.Reset();
    DeviceContext.Reset();
    Device.Reset();

    PendingSize.reset();
    CurrentSize = {};
}

// 스압체인 생성을 여기서?
void FGraphicsContext::CreateDeviceAndSwapChain(
    HWND WindowHandle,
    std::int32_t Width,
    std::int32_t Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

    // 스왑 체인 설정 구조체 초기화
    DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
    swapchaindesc.BufferDesc.Width = Width;
    swapchaindesc.BufferDesc.Height = Height;
    swapchaindesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 색상 포맷
    swapchaindesc.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용
    swapchaindesc.BufferCount = 2; // 더블 버퍼링
    swapchaindesc.OutputWindow = WindowHandle; // 렌더링할 창 핸들
    swapchaindesc.Windowed = TRUE; // 창 모드
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식
    swapchaindesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // 모드 전환 허용

#ifdef _DEBUG
    // Direct3D 장치와 스왑 체인을 생성
    ErrorHandler::ReportHRESULT(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
        featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
        &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext), "[ FGraphicsContext ]", "Failed to create Direct3D device and swap chain.", ErrorHandler::EErrorLevel::Critical);
#else 
    ErrorHandler::ReportHRESULT(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
        &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext), "[ FGraphicsContext ]", "Failed to create Direct3D device and swap chain.", ErrorHandler::EErrorLevel::Critical);
#endif

    // 생성된 스왑 체인의 정보 가져오기
    SwapChain->GetDesc(&swapchaindesc);

}*/


FGraphicsContext::FGraphicsContext()
{
    WindowToGraphics.TryBind<FWindowResizeRequestMessage>(
        [this](const FWindowResizeRequestMessage& Message)
        {
            if (Message.Width == 0 || Message.Height == 0)
            {
                return;
            }

            PendingResize = Message;
        });
}

void FGraphicsContext::SetRenderer(FRenderer& InRenderer)
{
    Renderer = &InRenderer;
}

FMessageChannel::FSender
FGraphicsContext::GetWindowToGraphicsSender()
{
    return WindowToGraphics.GetSender();
}

void FGraphicsContext::Dispatch()
{
    WindowToGraphics.Dispatch();

    if (Renderer == nullptr || !PendingResize.has_value())
    {
        return;
    }

    const FWindowResizeRequestMessage Request = *PendingResize;
    PendingResize.reset();

    Renderer->ReSize(Request.Width, Request.Height);
}


