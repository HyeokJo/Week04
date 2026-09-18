#include "PCH.h"

#include "FGraphicsContext.h"
#include "Render/FSceneRenderSurface.h"
#include "ErrorHandler.h"
#include "Render/Renderer.h"

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


