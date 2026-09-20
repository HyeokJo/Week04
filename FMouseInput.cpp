#include "PCH.h"

#include "FMouseInput.h"
#include "FMousePickRequestMessage.h"
#include "FMouseCameraRotateRequestMessage.h"
#include "FKeyboardCameraMoveRequestMessage.h"
#include "FMouseCameraMoveRequestMessage.h"
#include "FMouseCameraDollyRequestMessage.h"

namespace
{
    std::int32_t GetMouseX(LPARAM LParam)
    {
        return static_cast<std::int32_t>(
            static_cast<std::int16_t>(LOWORD(LParam)));
    }

    std::int32_t GetMouseY(LPARAM LParam)
    {
        return static_cast<std::int32_t>(
            static_cast<std::int16_t>(HIWORD(LParam)));
    }
}

void FMouseInput::InitializeWorldCommandSender(
    FMessageChannel::FSender&& InSender)
{
    WorldCommandSender.emplace(std::move(InSender));
}

EKeyState FMouseInput::GetKeyState(EMouseSide Side) const
{
    return KeyStates[Side];
}

const FMouseInput::DragCapture&
FMouseInput::GetDragCapture(EMouseSide Side) const
{
    return ClickCaptures[Side];
}

void FMouseInput::ProcessWindowMessage(
    UINT Message,
    WPARAM WParam,
    LPARAM LParam)
{
    UNREFERENCED_PARAMETER(WParam);

    switch (Message)
    {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        const EMouseSide Side =
            Message == WM_LBUTTONDOWN ? Left : Right;
        const EMouseSide Other = (Side == Left) ? Right : Left;

        if (KeyStates[Other] == EKeyState::Pressed ||
        KeyStates[Other] == EKeyState::Down)
    {
        break;
    }

        EKeyState& State = KeyStates[Side];

        if (State == EKeyState::None ||
            State == EKeyState::Released)
        {
            State = EKeyState::Pressed;
            DragOwners[Side] = EDragOwner::None;

            DragCapture& Capture = ClickCaptures[Side];

            Capture.start = {
                GetMouseX(LParam),
                GetMouseY(LParam)
            };

            Capture.current = Capture.start;

            //if (Side == Right)
            {
                PendingDeltaX = 0.0f;
                PendingDeltaY = 0.0f;
            }
        }

        break;
    }

    case WM_MOUSEMOVE:
    {
        const POINT Position{
            GetMouseX(LParam),
            GetMouseY(LParam)
        };

        for (std::size_t Side = 0; Side < MAX; ++Side)
        {
            if (KeyStates[Side] != EKeyState::Pressed &&
                KeyStates[Side] != EKeyState::Down)
            {
                continue;
            }

            DragCapture& Capture = ClickCaptures[Side];

            //if (Side == Right)
            {
                PendingDeltaX += static_cast<float>(
                    Position.x - Capture.current.x);

                PendingDeltaY -= static_cast<float>(
                    Position.y - Capture.current.y);
            }

            Capture.current = Position;
        }

        break;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    {
        const EMouseSide Side =
            Message == WM_LBUTTONUP ? Left : Right;

        //좌,우를 같이 눌렀을때 PendingRotateDelta가 중복해서 누적되는걸 방지하기위해
        //한쪽을 누른상태인지 체크
        const EMouseSide Other = (Side == Left) ? Right : Left;
        if (KeyStates[Other] == EKeyState::Pressed ||
            KeyStates[Other] == EKeyState::Down)
        {
            break;
        }


        EKeyState& State = KeyStates[Side];

        if (State == EKeyState::Pressed ||
            State == EKeyState::Down)
        {
            DragCapture& Capture = ClickCaptures[Side];

            const POINT Position{
                GetMouseX(LParam),
                GetMouseY(LParam)
            };

            //if (Side == Right)
            {
                PendingDeltaX += static_cast<float>(
                    Position.x - Capture.current.x);

                PendingDeltaY -= static_cast<float>(
                    Position.y - Capture.current.y);
            }

            Capture.current = Position;
            State = EKeyState::Released;
        }

        break;
    }

    case WM_KILLFOCUS:
    case WM_CANCELMODE:
    case WM_CAPTURECHANGED:
        ResetKeyStates();
        break;

    case WM_MOUSEWHEEL:
        PendingWheelSteps += GET_WHEEL_DELTA_WPARAM(WParam) / static_cast<float>(WHEEL_DELTA);
        break;
    }
}

void FMouseInput::Consume(EMouseSide Side)
{
    if (KeyStates[Side] == EKeyState::Pressed)
    {
        DragOwners[Side] = EDragOwner::Gizmo;
    }
}

void FMouseInput::DispatchPendingWorldCommands(
    std::uint32_t ViewportWidth,
    std::uint32_t ViewportHeight,
    bool bMouseCapturedByUI)
{
    // 누르기 시작한 시점에 입력 소유권을 결정한다.
    // 기즈모가 Consume한 버튼은 그대로 유지한다.
    for (std::size_t Side = 0; Side < MAX; ++Side)
    {
        if (KeyStates[Side] == EKeyState::Pressed &&
            DragOwners[Side] == EDragOwner::None)
        {
            DragOwners[Side] = bMouseCapturedByUI
                ? EDragOwner::UI
                : EDragOwner::World;
        }
    }

    if (!WorldCommandSender.has_value())
    {
        return;
    }


   #ifndef OBJ_VIEWER
    if (DragOwners[Left] == EDragOwner::World)
    {
        if (KeyStates[Left] == EKeyState::Pressed)
        {
            const DragCapture& Capture = ClickCaptures[Left];

            WorldCommandSender->TryEmplace<FMousePickRequestMessage>(
                Capture.start.x,
                Capture.start.y,
                ViewportWidth,
                ViewportHeight);
        }
    }

    if (DragOwners[Right] == EDragOwner::World &&
        (PendingDeltaX != 0.0f ||
            PendingDeltaY != 0.0f))
    {
        WorldCommandSender
            ->TryEmplace<FMouseCameraRotateRequestMessage>(
                PendingDeltaX,
                PendingDeltaY);
    }
#endif

#ifdef OBJ_VIEWER
    //TODO : 회전방식 바꿔야함
    const bool bHasDelta =
        PendingDeltaX != 0.0f || PendingDeltaY != 0.0f;

    if (DragOwners[Left] == EDragOwner::World && bHasDelta)
    {
        WorldCommandSender->TryEmplace<FMouseCameraRotateRequestMessage>(
            PendingDeltaX, PendingDeltaY);
    }

    if (DragOwners[Right] == EDragOwner::World && bHasDelta)
    {
        WorldCommandSender->TryEmplace<FMouseCameraMoveRequestMessage>(
            PendingDeltaX, PendingDeltaY);
    }

    if (!bMouseCapturedByUI && PendingWheelSteps != 0.0f)
    {
        WorldCommandSender->TryEmplace<FMouseCameraDollyRequestMessage>(PendingWheelSteps);
    }
    
#endif

    PendingDeltaX = 0.0f;
    PendingDeltaY = 0.0f;
    PendingWheelSteps = 0.0f;
}

void FMouseInput::EndFrame()
{
    for (std::size_t Side = 0; Side < MAX; ++Side)
    {
        EKeyState& State = KeyStates[Side];

        if (State == EKeyState::Pressed)
        {
            State = EKeyState::Down;
        }
        else if (State == EKeyState::Released)
        {
            State = EKeyState::None;
            DragOwners[Side] = EDragOwner::None;
        }
    }

    PendingDeltaX = 0.0f;
    PendingDeltaY = 0.0f;
}

void FMouseInput::ResetKeyStates()
{
    for (EKeyState& State : KeyStates)
    {
        // 드래그를 종료할 수 있도록 Released를 한 프레임 유지한다.
        if (State == EKeyState::Pressed ||
            State == EKeyState::Down)
        {
            State = EKeyState::Released;
        }
    }

    PendingDeltaX = 0.0f;
    PendingDeltaY = 0.0f;
}
