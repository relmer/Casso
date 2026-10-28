#include "Pch.h"

#include "Shell/Components/ShellMouseInput.h"

#include "Shell/Components/ShellInput.h"
#include "Shell/MachineHost.h"
#include "Shell/Components/ShellHistory.h"
#include "Shell/IInputHost.h"
#include "Shell/EmulatorShellInternal.h"
#include "Machines/Apple2/Common/AppleMouse.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::ClipPaddleCursorToClient
//
//  The pointer confined to the picture and parked in the middle -- the half of
//  "captured" the user can see. Kept apart from StartPaddleCapture because the
//  clip has to be laid on twice: once at the grab, and again when a cancel the
//  chrome's own layout raised has to be taken back (see OnCancelMode).
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::ClipPaddleCursorToClient()
{
    RECT   client  = {};
    POINT  topLeft = {};
    POINT  botRt   = {};
    RECT   clip    = {};
    POINT  center  = {};



    if (m_input.m_host.GetMainWindow() == nullptr || !GetClientRect (m_input.m_host.GetMainWindow(), &client))
    {
        return;
    }

    topLeft.x = client.left;
    topLeft.y = client.top;
    botRt.x   = client.right;
    botRt.y   = client.bottom;
    ClientToScreen (m_input.m_host.GetMainWindow(), &topLeft);
    ClientToScreen (m_input.m_host.GetMainWindow(), &botRt);

    clip.left   = topLeft.x;
    clip.top    = topLeft.y;
    clip.right  = botRt.x;
    clip.bottom = botRt.y;
    ClipCursor (&clip);

    center.x = (client.right  - client.left) / 2;
    center.y = (client.bottom - client.top)  / 2;
    ClientToScreen (m_input.m_host.GetMainWindow(), &center);
    SetCursorPos (center.x, center.y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::StartPaddleCapture
//
//  Hides and confines the cursor to the client area, parks it at center,
//  and begins relative tracking. No-op unless the mode is Paddle, the
//  window owns the foreground, and capture isn't already active. The
//  current (held) paddle position is pushed so a re-grab after an Esc
//  release resumes from where the dial was left.
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::StartPaddleCapture()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_input.m_pointerMode != InputMappingMode::Paddle, S_OK);
    BAIL_OUT_IF (m_paddleCaptured,                        S_OK);
    BAIL_OUT_IF (m_input.m_host.GetMainWindow() == nullptr,                       S_OK);
    BAIL_OUT_IF (GetForegroundWindow() != m_input.m_host.GetMainWindow(),         S_OK);

    m_paddleCaptured = true;

    //  THE BAND IS CLAIMED BEFORE THE MOUSE IS, and the order is the point.
    //  Docking resizes the picture, and Windows answers a resize by cancelling
    //  the mode of whoever holds the pointer -- so the layout runs while there
    //  is no grab to lose, and the grab is taken once it has settled. Traced
    //  in that order, no cancel arrives at all; the two guards below exist for
    //  the case where one does anyway.
    //
    //  NOT IN FULLSCREEN, where the band is zero however this ends: the bar
    //  hangs off the top edge there instead. A layout pass would be a settle
    //  of the whole scene plus a synchronous repaint for no change at all --
    //  and one caller of this grab is the fullscreen strip's own tick, which
    //  runs inside the frame the repaint would re-enter.
    m_input.m_host.ClaimCaptureBand();

    SetCapture (m_input.m_host.GetMainWindow());

    // Drive the per-thread ShowCursor counter negative so the arrow hides.
    while (ShowCursor (FALSE) >= 0)
    {
    }

    ClipPaddleCursorToClient();

    PushPaddlePosition();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::StopPaddleCapture
//
//  Releases the mouse capture and cursor clip, restores the cursor, and
//  clears the fire buttons so a held mouse button doesn't stick. No-op
//  when not captured. Leaves the input mode unchanged, so the dial holds
//  its position for a later re-grab.
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::StopPaddleCapture()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_paddleCaptured, S_OK);

    m_paddleCaptured = false;

    ClipCursor (nullptr);

    if (GetCapture() == m_input.m_host.GetMainWindow())
    {
        ReleaseCapture();
    }

    while (ShowCursor (TRUE) < 0)
    {
    }

    PushPaddleButton (0, false);
    PushPaddleButton (1, false);

    //  ...and the picture takes the height back -- windowed, where it gave any
    //  up. See StartPaddleCapture for why fullscreen is left alone.
    m_input.m_host.ReleaseCaptureBand();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::UpdatePaddleFromMouse
//
//  Maps one WM_MOUSEMOVE while paddle-captured: the motion relative to the
//  client center is scaled (s_kPaddleSweepInches of DPI-scaled travel =
//  full range) into the held paddle axes, then the cursor is snapped back
//  to center for unbounded relative motion. The zero-delta move our own
//  recenter generates is ignored.
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::UpdatePaddleFromMouse (int xClient, int yClient)
{
    HRESULT  hr         = S_OK;
    RECT     client     = {};
    int      centerX    = 0;
    int      centerY    = 0;
    int      dx         = 0;
    int      dy         = 0;
    UINT     dpi        = 96;
    float    unitsPerPx = 0.0f;
    POINT    center     = {};



    BAIL_OUT_IF (!m_paddleCaptured, S_OK);

    GetClientRect (m_input.m_host.GetMainWindow(), &client);
    centerX = (client.right  - client.left) / 2;
    centerY = (client.bottom - client.top)  / 2;
    dx      = xClient - centerX;
    dy      = yClient - centerY;

    // The SetCursorPos recenter below re-enters here with a zero delta.
    BAIL_OUT_IF (dx == 0 && dy == 0, S_OK);

    dpi = GetDpiForWindow (m_input.m_host.GetMainWindow());
    if (dpi == 0)
    {
        dpi = 96;
    }

    unitsPerPx    = s_kPaddleRange / (s_kPaddleSweepInches * (float) dpi);
    m_paddleAxisX = std::clamp (m_paddleAxisX + (float) dx * unitsPerPx, s_kPaddleMinF, s_kPaddleMaxF);
    m_paddleAxisY = std::clamp (m_paddleAxisY + (float) dy * unitsPerPx, s_kPaddleMinF, s_kPaddleMaxF);

    PushPaddlePosition();

    center.x = centerX;
    center.y = centerY;
    ClientToScreen (m_input.m_host.GetMainWindow(), &center);
    SetCursorPos (center.x, center.y);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::PushPaddlePosition
//
//  Submits the held paddle axes to the game-port mixer as the mouse paddle's
//  contribution, keeping its buttons as they are.
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::PushPaddlePosition()
{
    Byte  x = (Byte) (m_paddleAxisX + 0.5f);
    Byte  y = (Byte) (m_paddleAxisY + 0.5f);



    m_mousePaddleContribution.paddle[0] = x;
    m_mousePaddleContribution.paddle[1] = y;
    m_input.m_gamePortMixer.Submit (GamePortSource::MousePaddle, m_mousePaddleContribution);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::PushPaddleButton
//
//  Submits a paddle fire button (0 or 1) to the game-port mixer as part of
//  the mouse paddle's contribution, keeping its axes as they are.
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::PushPaddleButton (int index, bool pressed)
{
    constexpr int  kMouseButtonCount = 2;
    HRESULT        hr                = S_OK;



    CBRA (index >= 0 && index < kMouseButtonCount);

    m_mousePaddleContribution.buttons.set (static_cast<size_t> (index), pressed);
    m_input.m_gamePortMixer.Submit (GamePortSource::MousePaddle, m_mousePaddleContribution);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::IsGuestMouseActive
//
////////////////////////////////////////////////////////////////////////////////

bool ShellMouseInput::IsGuestMouseActive() const
{
    // The fullscreen drive strip's hotkey summon "releases" the guest mouse
    // for the interaction; the FSM restores it when the strip hides.
    return m_input.m_pointerMode == InputMappingMode::Mouse && m_input.m_machine.GetMouse() != nullptr
        && m_input.m_mouseConnected && !m_input.m_host.IsGuestMouseSuppressed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::IsGuestMouseLive
//
//  "Mouse-aware software is actually running", so Mouse mode stays invisible
//  at a BASIC prompt: hardware truth (the IOU's own interrupt enables, which
//  only the $C079 / $C05x programming sequence can set) rather than anything
//  garbage RAM could fake.
//
//  EITHER enable counts. The gate first read X/Y alone, on the assumption
//  that SETMOUSE programs ENBXY for every active mode -- it does not.
//  MousePaint's main app asks for mode $09 (mouse on + VBL interrupt, no
//  movement interrupt), so the firmware issues DISXY / ENVBL and X/Y stays
//  masked for as long as the app runs. That read as a dead mouse: the pointer
//  froze wherever the firmware left it and clicks went nowhere, while the
//  startup screen and tutorial -- different modes -- worked.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellMouseInput::IsGuestMouseLive() const
{
    return IsGuestMouseActive() &&
           (m_input.m_machine.GetMouse()->AreXyInterruptsEnabled() || m_input.m_machine.GetMouse()->AreVblInterruptsEnabled());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::UpdateGuestMouseFromHost
//
//  Absolute host->guest mapping for //c Mouse mode. The mouse firmware owns
//  position and clamping, publishing both through the slot-7 screen holes:
//
//      position   $047F/$057F (X lo/hi)   $04FF/$05FF (Y lo/hi)
//      clamp min  $047D/$057D (X)         $04FD/$05FD (Y)
//      clamp max  $067D/$077D (X)         $06FD/$07FD (Y)
//
//  The host position's fraction across the viewport maps into the live
//  clamp window, and the delta from the firmware's current position is
//  queued as movement units. Self-correcting: any units the firmware
//  clamps away are re-derived from the holes on the next move. Sanity
//  checks make this a no-op until the guest app has initialized the mouse
//  firmware (pre-INITMOUSE holes are garbage). PeekByte reads the CPU's
//  memory array directly (same cross-thread pattern as screen scraping).
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::UpdateGuestMouseFromHost (int xPx, int yPx)
{
    bool                                  isGateOpen = false;
    std::optional<uint32_t>               target;
    HostInputGate::Entry                  gate;



    // Only Mouse mode with the guest's mouse on makes the pointer guest
    // input; anything else over the picture is the host's own pointer.
    if (!IsGuestMouseLive())
    {
        return;
    }

    // Through the glass when the CRT is shown, so only the picture counts.
    // Off the picture the guest mouse is released to wherever the firmware
    // last put it. The DEVICE projects the fraction into the firmware's live
    // clamp window on the CPU thread (AppleMouse::Tick -> RetargetFromHoles):
    // guest memory must not be read from the UI thread.
    target = m_input.m_host.MapPointerToPicture (xPx, yPx);

    // Behind live in reverse execution's history the mouse holds a recorded
    // position, so the pointer's place is held instead, until the guest's
    // mouse would move to it or the machine is live.
    isGateOpen = m_input.m_machine.GetHostInputGate().TryEnter (gate);

    if (!isGateOpen)
    {
        m_input.m_history.GetDivergenceGate().SetMouseTarget (target);
        m_input.m_history.PublishHeldInput();
    }
    else if (target.has_value())
    {
        m_input.m_machine.GetMouse()->SetHostTargetFraction (static_cast<uint16_t> (*target >> 16), static_cast<uint16_t> (*target & 0xFFFF));
    }
    else
    {
        m_input.m_machine.GetMouse()->ClearHostTarget();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::PressGuestMouseButton
//
//  Only once guest software has turned the mouse on, so clicks are not
//  swallowed at a BASIC prompt, and only over the picture: through the glass
//  when the CRT is on, else across the viewport. Behind live the press is
//  deferred until the guest reads the button; a release made behind live is
//  injected once the machine is live.
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::PressGuestMouseButton (int x, int y)
{
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;



    if (!IsGuestMouseLive() || !m_input.m_host.MapPointerToPicture (x, y).has_value())
    {
        return;
    }

    isGateOpen = m_input.m_machine.GetHostInputGate().TryEnter (gate);
    m_input.m_heldHostInputs.OnPress (HeldHostInputs::kMouseButton, isGateOpen);

    if (!isGateOpen)
    {
        m_input.m_history.HoldInputBehindLive (HeldInput::MousePress);
        return;
    }

    m_input.m_machine.GetMouse()->SetButton (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput::ReleaseGuestMouseButton
//
//  Any left release drops the //c guest mouse button, over the picture or
//  not, so a press inside the picture released outside it also releases the
//  button. Behind live the release is deferred until the machine is live, and
//  a deferred press the guest has not read yet is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void ShellMouseInput::ReleaseGuestMouseButton()
{
    HostInputGate::Entry                 gate;
    bool                                 isGateOpen = false;



    if (!IsGuestMouseActive())
    {
        return;
    }

    isGateOpen = m_input.m_machine.GetHostInputGate().TryEnter (gate);
    m_input.m_heldHostInputs.OnRelease (HeldHostInputs::kMouseButton, isGateOpen);

    if (!isGateOpen)
    {
        m_input.m_history.GetDivergenceGate().ReleaseMousePress();
        m_input.m_history.PublishHeldInput();
        return;
    }

    m_input.m_machine.GetMouse()->SetButton (false);
}
