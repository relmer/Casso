#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Controllers/InputModeRules.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Ui/Chrome/DriveLabelTruncation.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Ui/PrinterPanel.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Window/DxuiHwndSource.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetChromeFocusIndex
//
//  Move the keyboard chrome-focus ring to a new slot (-1 = guest) and refresh
//  which widget paints its focus visual.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetChromeFocusIndex (int index)
{
    m_chromeFocusIndex = index;
    UpdateChromeFocusVisuals();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdateChromeFocusVisuals
//
//  Push the current ring index into the MainMenu (focused-closed menu title),
//  the joystick-mode button, and the two drive widgets so exactly one of them
//  paints a focus ring.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateChromeFocusVisuals()
{
    int  index = m_chromeFocusIndex;



    if (index >= s_kChromeFocusMenuFirst && index <= s_kChromeFocusMenuLast)
    {
        m_mainMenu.SetFocusedMenu ((MainMenuId) index);
    }
    else
    {
        m_mainMenu.ClearFocus();
    }

    m_toolbar.SetFocusIndex ((index >= s_kChromeFocusToolbarFirst && index <= s_kChromeFocusToolbarLast)
                                 ? index - s_kChromeFocusToolbarFirst : -1);

    m_driveChrome[0].SetFocused (index == s_kChromeFocusDrive0);
    m_driveChrome[1].SetFocused (index == s_kChromeFocusDrive1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HandleChromeFocusKey
//
//  Own every keydown while the chrome keyboard-focus ring is active. Tab /
//  Shift+Tab traverse the whole ring (menu titles -> drives, wrapping);
//  Left/Right move among the menu titles; Enter/Space/Down open a dropdown
//  or activate the focused drive; Esc/F10 leave the ring. When
//  a dropdown is open, keys delegate to MainMenu and the index is reconciled
//  with whatever the menu did. Always consumes the key.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::HandleChromeFocusKey (WPARAM vk)
{
    bool  shift       = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
    int   dir         = shift ? -1 : 1;
    int   index       = m_chromeFocusIndex;
    bool  exitVk      = (vk == VK_ESCAPE || vk == VK_F10);
    bool  menuIsOpen  = m_mainMenu.IsOpen();
    bool  onMenuTitle = index >= s_kChromeFocusMenuFirst && index <= s_kChromeFocusMenuLast;
    bool  onToolbar   = index >= s_kChromeFocusToolbarFirst && index <= s_kChromeFocusToolbarLast;



    // A toolbar picker or a keyboard-opened flyout owns navigation; Escape
    // closes it and leaves the entry focused.
    if (m_toolbar.OwnsKeyboard())
    {
        (void) m_toolbar.HandleKey (vk);
    }

    // An open dropdown owns navigation; delegate and reconcile the ring.
    else if (menuIsOpen)
    {
        bool  ringOwned = (m_chromeFocusIndex != s_kChromeFocusNone);
        int   openIdx   = (int) m_mainMenu.GetOpenMenu();

        m_mainMenu.HandleKey (vk);

        if (m_mainMenu.IsOpen())
        {
            // Still open: a ring-owned menu tracks the (possibly switched)
            // title. A menu opened outside the ring (Alt mnemonic / mouse)
            // stays un-owned so closing it returns to the guest rather than
            // stranding focus on a title the user never Tab'd to.
            if (ringOwned)
            {
                SetChromeFocusIndex ((int) m_mainMenu.GetOpenMenu());
            }
        }
        else if (exitVk && ringOwned)
        {
            // Esc/F10 closed a ring-opened dropdown: keep the title focused.
            SetChromeFocusIndex (openIdx);
        }
        else
        {
            // Dispatched a command (or closed a menu the ring never owned):
            // hand focus back to the guest.
            SetChromeFocusIndex (s_kChromeFocusNone);
        }
    }
    else if (exitVk)
    {
        SetChromeFocusIndex (s_kChromeFocusNone);
    }
    else if (vk == VK_TAB)
    {
        SetChromeFocusIndex ((index + dir + s_kChromeFocusCount) % s_kChromeFocusCount);
    }

    // A menu title is focused with its dropdown closed. Left/Right wrap within
    // the titles here rather than walking the whole ring.
    else if (onMenuTitle && vk == VK_LEFT)
    {
        SetChromeFocusIndex ((index == s_kChromeFocusMenuFirst) ? s_kChromeFocusMenuLast : index - 1);
    }
    else if (onMenuTitle && vk == VK_RIGHT)
    {
        SetChromeFocusIndex ((index == s_kChromeFocusMenuLast) ? s_kChromeFocusMenuFirst : index + 1);
    }
    else if (onMenuTitle && (vk == VK_DOWN || vk == VK_RETURN || vk == VK_SPACE))
    {
        m_mainMenu.Open ((MainMenuId) index, true);
    }

    // A toolbar entry is focused: Enter, Space or Down activates it, which
    // for a picker opens its list and for the volume entry opens its flyout
    // with the slider taking the keys that follow.
    else if (onToolbar && (vk == VK_DOWN || vk == VK_RETURN || vk == VK_SPACE))
    {
        m_toolbar.ActivateFocused();
    }

    // A toolbar entry or a drive widget is focused. Left/Right walk the whole
    // ring so horizontal arrows feel natural along the strips.
    else if (vk == VK_LEFT)
    {
        SetChromeFocusIndex ((index - 1 + s_kChromeFocusCount) % s_kChromeFocusCount);
    }
    else if (vk == VK_RIGHT)
    {
        SetChromeFocusIndex ((index + 1) % s_kChromeFocusCount);
    }
    else if (vk == VK_RETURN || vk == VK_SPACE)
    {
        if (index == s_kChromeFocusDrive0)
        {
            BrowseForDisk (m_driveChrome[0].GetDrive());
        }
        else if (index == s_kChromeFocusDrive1)
        {
            BrowseForDisk (m_driveChrome[1].GetDrive());
        }
    }

    // The ring owns every keydown it sees -- an unrecognized key is swallowed
    // rather than leaking through to the guest. See the banner.
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
//  Routes one pointer move through every chrome consumer, in priority order.
//
//  The two guest-input modes sit at the top and behave OPPOSITELY, which is
//  the thing to know before editing this:
//
//    paddle mode  captures. It consumes the move outright -- relative motion
//                 drives the held axes and the cursor is snapped back to
//                 center -- so the chrome must never see it, and the function
//                 bails immediately.
//    mouse mode   does NOT capture. It maps the position to the guest and
//                 then deliberately falls through to normal routing, because
//                 the viewport carries no chrome and moves over the menu bar
//                 or drive band must keep working exactly as before.
//
//  Below that, UiShell gets first refusal (it owns the caption bar and nav
//  strip); anything it claims ends the walk. What remains is the shell's own
//  chrome: joystick button, command toolbar, //c switch strip, and the drive
//  widgets, each pairing a hover update with a tooltip show / hide request.
//
//  Tooltips are REQUESTS, not shows -- the dwell timers in TryPresentUiFrame decide
//  when a popup actually appears, so a fast pass over three widgets does not
//  flash three tooltips.
//
//  The drive walk runs before the shell dispatch because it has to visit
//  every widget regardless of hit: leaving a widget is what re-arms its
//  basename marquee, so an early exit on the first hit would strand the
//  previously hovered drive mid-scroll.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    HRESULT            hr           = S_OK;
    DxuiMessageResult  result       = DxuiMessageResult::NotHandled;
    int                x            = ((int) (short) LOWORD (lParam));
    int                y            = ((int) (short) HIWORD (lParam));
    bool               leftDown     = (wParam & MK_LBUTTON) != 0;
    bool               shellHandled = false;
    DriveWidget *      wpDrive      = nullptr;
    int64_t            nowMs        = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                          std::chrono::steady_clock::now().time_since_epoch()).count();



    // The compass sees every move: armed, it owns the gesture; idle, the
    // call is what keeps its hover highlight honest. Ahead of the drags
    // below because a press the compass took must never feed the orbit's
    // own anchor math as well.
    if (!m_paddleCaptured && m_sceneCompass.OnPointerMove (x, y))
    {
        return DxuiMessageResult::Handled;
    }

    // A pan in flight owns the move outright. Measured from the ANCHOR the
    // press recorded rather than accumulated frame to frame, so the scene
    // tracks the cursor exactly however far or slowly it travels and a long
    // drag cannot creep away from it.
    //
    // The paddle check below cannot be reached while this claims the move, so
    // the capture is tested HERE too. A pan cannot start under paddle capture
    // today -- the press handler bails before arming one -- but that is one
    // early-out in another function away from being untrue, and the failure
    // it would cause is a game whose paddles stop responding.
    // An orbit in flight owns the move the same way a pan does, whichever
    // button is driving it.
    if (m_sceneOrbiting && !m_paddleCaptured &&
        ((m_sceneOrbitLeftBtn && leftDown) ||
         (!m_sceneOrbitLeftBtn && (wParam & MK_RBUTTON) != 0)))
    {
        // Under the slop this is still a click in the making, so the scene
        // must not stir: a picture that shifts a pixel under a press and
        // shifts back is worse than one that does not move at all. The
        // right-button orbit has no click to protect and turns at once.
        if (!m_sceneOrbitMoved &&
            m_sceneOrbitLeftBtn &&
            std::abs (x - m_sceneOrbitStartPx.x) <= s_kSceneOrbitSlopPx &&
            std::abs (y - m_sceneOrbitStartPx.y) <= s_kSceneOrbitSlopPx)
        {
            return DxuiMessageResult::Handled;
        }

        m_sceneOrbitMoved = true;

        UpdateSceneOrbit (x, y);
        return DxuiMessageResult::Handled;
    }

    // THE TILT FOLLOWS THE POINTER, not the mark. Dragging up tips the face
    // up and dragging down tips it down, whichever mark the gesture started
    // on -- the marks say which way the control goes, they are not two
    // separate handles that move in opposite senses. Screen y grows downward,
    // so the travel is negated to get "up is up".
    if (m_bezelTilting && leftDown && !m_paddleCaptured)
    {
        m_deskScene.SetBezelTilt (m_bezelTiltStartRad
                                  + ((float) (m_bezelTiltStartPx.y - y)) * kBezelTiltRadPerPx);
        InvalidateSceneComposition();

        return DxuiMessageResult::Handled;
    }

    if (m_scenePanning && leftDown && !m_paddleCaptured)
    {
        RECT   box    = m_deskScene.Composition().viewportPx;
        float  width  = (float) (box.right - box.left);
        float  height = (float) (box.bottom - box.top);

        if (width > 0.0f && height > 0.0f)
        {
            // A pixel of cursor travel is two NDC units across the whole
            // viewport, and NDC y runs opposite client y.
            m_sceneView.panX = m_scenePanStartX
                             + ((float) (x - m_scenePanStartPx.x) / width)  * 2.0f;
            m_sceneView.panY = m_scenePanStartY
                             - ((float) (y - m_scenePanStartPx.y) / height) * 2.0f;

            ClampSceneView();
            InvalidateSceneComposition();
        }

        return DxuiMessageResult::Handled;
    }

    // Paddle mode owns the pointer while captured: relative motion drives
    // the held paddle axes and the cursor is snapped back to center, so the
    // chrome never sees the move.
    if (m_paddleCaptured)
    {
        UpdatePaddleFromMouse (x, y);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_paddleCaptured, S_OK);

    // //c Mouse mode (non-capturing): a move over the emulator viewport
    // drives the guest mouse via absolute mapping. Deliberately falls
    // through to normal routing — the viewport has no chrome, and moves
    // outside it (menu bar, drive band) behave exactly as before.
    if (IsGuestMouseActive())
    {
        UpdateGuestMouseFromHost (x, y);
    }

    // A fresh hover over a drive widget replays its basename marquee, so
    // the full filename can be re-read on demand. The same pass notes a
    // write-protected drive under the pointer so the WP tooltip can show.
    for (DriveWidget & drive : m_driveChrome)
    {
        RECT  outer  = drive.GetOuterRect();
        bool  inside = x >= outer.left && x < outer.right &&
                       y >= outer.top  && y < outer.bottom;

        if (drive.UpdateMarqueeHover (inside, nowMs))
        {
            // The band's button treatment appeared or went away. A static
            // emulator picture presents no frames on its own, so without this
            // the highlight would land on whatever frame happened next.
            m_d3dRenderer.MarkRedrawNeeded();
        }

        if (inside && drive.IsWriteProtected())
        {
            wpDrive = &drive;
        }
    }

    shellHandled = m_uiShell.OnMouseMove (x, y, leftDown);

    if (shellHandled)
    {
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (shellHandled, S_OK);


    // Command toolbar hover / slider drag (DCR-2). In icon-only mode the
    // hovered button's label surfaces as a tooltip (no labels on the strip).
    if (m_toolbar.OnToolbarMouseMove (x, y))
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    {
        RECT             anchor = {};
        const wchar_t *  tip    = m_toolbar.GetTooltipAt (x, y, anchor);

        if (tip != nullptr)
        {
            m_toolbarTooltip.RequestShow (anchor, tip, nowMs);
        }
        else
        {
            m_toolbarTooltip.RequestHide (nowMs);
        }
    }

    // //c switch strip: hover state and a per-part tooltip (reset / 80/40 /
    // keyboard). Inert on non-//c machines (hidden).
    if (MachineHasCaseSwitches())
    {
        const wchar_t * tip          = m_switchBar.GetTooltipTextAt (x, y);
        bool            hoverChanged = m_switchBar.SetHovered (m_switchBar.HitTest (x, y));
        bool            partChanged  = m_switchBar.SetHoverPoint (x, y);

        // The strip is painted only when a frame is presented, and a static
        // screen presents none; a highlight that moved asks for one.
        if (hoverChanged || partChanged)
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }

        if (tip != nullptr)
        {
            m_switchBarTooltip.RequestShow (m_switchBar.GetBounds(), tip, nowMs);
        }
        else
        {
            m_switchBarTooltip.RequestHide (nowMs);
        }
    }

    // Drive hover tooltip, suppressed while the joystick button owns the
    // hover (mutually exclusive bands, but be explicit). Windowed, the 3D
    // scene's name strips already show every basename and padlock, so the
    // only tooltip left is the padlock's WHY -- the write-protect
    // composition, anchored to the label it explains. Dwelling on the case
    // itself volunteers nothing the strip is not already saying. Fullscreen
    // shows no labels, so the overlay strip's drives keep the name tooltip,
    // joined by the write-protect composition when the disk is protected --
    // there is no padlock anywhere else to ask. The 2D path keeps its dwell
    // tooltip for protected drives only (the basename lives on the widget's
    // marquee label there).
    {
        std::wstring  tip;
        RECT          anchor = {};

        if (DeskSceneActive())
        {
            // The name strip answers for the padlock in BOTH presentations:
            // the strip carries names and locks in fullscreen now, so the
            // lock explains itself there the same way it does on the desk.
            for (int i = 0; i < (int) m_sceneDriveLabelRect.size(); i++)
            {
                POINT  lp = { x, y };

                if (m_sceneDriveLabelRect[i].right > m_sceneDriveLabelRect[i].left &&
                    PtInRect (&m_sceneDriveLabelRect[i], lp) &&
                    m_driveWidgetState[i].writeProtect.Any())
                {
                    anchor = m_sceneDriveLabelRect[i];
                    tip    = ComposeWriteProtectTooltip (
                                 i + 1,
                                 std::filesystem::path (m_machine.GetDiskStore().GetSourcePath (6, i))
                                     .filename().wstring(),
                                 m_driveWidgetState[i].writeProtect);
                    break;
                }
            }
        }

        if (tip.empty() && DeskSceneActive() && m_d3dRenderer.IsFullscreen())
        {
            POINT  pt = { x, y };

            if (m_stripRectPx.bottom > m_stripRectPx.top &&
                PtInRect (&m_stripRectPx, pt))
            {
                SceneHitResult  sceneHit = StripHit (x, y);

                if (sceneHit.target == SceneHitResult::Target::Drive)
                {
                    std::wstring  imageName = std::filesystem::path (
                        m_machine.GetDiskStore().GetSourcePath (6, sceneHit.driveIndex)).filename().wstring();

                    anchor = m_stripComp.driveRectPx[sceneHit.driveIndex];
                    tip    = ComposeWriteProtectTooltip (
                                 sceneHit.driveIndex + 1, imageName,
                                 m_driveWidgetState[sceneHit.driveIndex].writeProtect);

                    if (tip.empty())
                    {
                        tip = imageName;
                    }
                }
            }
        }

        if (tip.empty() && !DeskSceneActive() && wpDrive != nullptr)
        {
            std::wstring  imageName = std::filesystem::path (
                m_machine.GetDiskStore().GetSourcePath (6, wpDrive->GetDrive())).filename().wstring();

            anchor = wpDrive->GetOuterRect();
            tip    = ComposeWriteProtectTooltip (wpDrive->GetDrive() + 1, imageName, wpDrive->WriteProtect());
        }

        if (!tip.empty())
        {
            m_driveTooltip.RequestShow (anchor, tip, nowMs);
        }
        else
        {
            m_driveTooltip.RequestHide (nowMs);
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseLeave
//
//  Routes through UiShell so chrome painters (title-bar caption
//  buttons, nav strip) drop their hot-button / hover state when the
//  cursor exits the window via the non-client area.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMouseLeave()
{
    int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                         std::chrono::steady_clock::now().time_since_epoch()).count();



    m_uiShell.OnMouseLeave();

    // Drop drive marquee-hover state so re-entering the window re-triggers
    // the basename scroll.
    for (DriveWidget & drive : m_driveChrome)
    {
        drive.UpdateMarqueeHover (false, nowMs);
    }

    m_toolbar.OnToolbarMouseLeave();
    m_toolbarTooltip.RequestHide (nowMs);
    m_driveTooltip.RequestHide (nowMs);

    {
        bool  hoverChanged = m_switchBar.SetHovered (false);
        bool  pressChanged = m_switchBar.SetPressedPart (Apple2cSwitchBar::Part::None);

        if (hoverChanged || pressChanged)
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }
    }

    m_switchBarTooltip.RequestHide (nowMs);

    // //c Mouse mode: the cursor left the window entirely — release the
    // guest mouse target (non-capturing contract).
    if (m_machine.GetMouse() != nullptr)
    {
        m_machine.GetMouse()->ClearHostTarget();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsGuestMouseActive
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsGuestMouseActive() const
{
    // The fullscreen drive strip's hotkey summon "releases" the guest mouse
    // for the interaction; the FSM restores it when the strip hides.
    return m_pointerMode == InputMappingMode::Mouse && m_machine.GetMouse() != nullptr
        && m_mouseConnected && !m_stripSuppressGuestMouse;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsGuestMouseLive
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

bool EmulatorShell::IsGuestMouseLive() const
{
    return IsGuestMouseActive() &&
           (m_machine.GetMouse()->AreXyInterruptsEnabled() || m_machine.GetMouse()->AreVblInterruptsEnabled());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateGuestMouseFromHost
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

void EmulatorShell::UpdateGuestMouseFromHost (int xPx, int yPx)
{
    const RECT & vp        = m_viewportBoundsPx;
    int          vpW       = vp.right  - vp.left;
    int          vpH       = vp.bottom - vp.top;
    bool         isLive    = IsGuestMouseLive() && vpW > 1 && vpH > 1;
    uint16_t     fx        = 0;
    uint16_t     fy        = 0;
    bool         isInside  = xPx >= vp.left && xPx < vp.right &&
                             yPx >= vp.top  && yPx < vp.bottom;



    if (isLive && CrtMonitorActive())
    {
        // Curvature-correct mapping (spec 018): the pixel comes from the
        // inverse projection through the glass, so only the picture counts
        // -- pointer positions off the glass (including what used to be
        // letterbox bars) release the guest mouse.
        SceneHitResult  hit = DeskSceneHit (xPx, yPx);

        if (hit.target == SceneHitResult::Target::Glass)
        {
            fx = static_cast<uint16_t> (MulDiv (hit.emulatedPixel.x, 65535, kFramebufferWidth - 1));
            fy = static_cast<uint16_t> (MulDiv (hit.emulatedPixel.y, 65535, kFramebufferHeight - 1));

            m_machine.GetMouse()->SetHostTargetFraction (fx, fy);
        }
        else
        {
            m_machine.GetMouse()->ClearHostTarget();
        }
    }
    else if (isLive && !isInside)
    {
        // Leaving the viewport releases the guest mouse to wherever the
        // firmware last put it (non-capturing contract).
        m_machine.GetMouse()->ClearHostTarget();
    }
    else if (isLive)
    {
        // Publish the viewport fraction only. The DEVICE projects it into the
        // firmware's live clamp window on the CPU thread (AppleMouse::Tick ->
        // RetargetFromHoles): guest memory must not be read from the UI thread
        // -- the CPU's debug array is not the live MMU-mapped RAM, and bus
        // reads here would race the CPU thread. (The original PeekByte-based
        // mapping read stale bytes and silently no-oped in production.)
        fx = static_cast<uint16_t> (MulDiv (xPx - vp.left, 65535, vpW - 1));
        fy = static_cast<uint16_t> (MulDiv (yPx - vp.top,  65535, vpH - 1));

        m_machine.GetMouse()->SetHostTargetFraction (fx, fy);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSetCursor
//
//  //c Mouse mode is non-capturing but hides the host cursor while it is
//  over the emulator viewport (the guest draws its own pointer); leaving
//  the viewport -- or the client area -- restores the normal arrow.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnSetCursor (WORD hitTest)
{
    DxuiMessageResult  result     = DxuiMessageResult::NotHandled;
    POINT              pt         = {};
    bool               overGuest  = false;



    // Only hide the cursor once guest software has turned the mouse on
    // (IsGuestMouseLive) -- over a BASIC prompt or a non-mouse game the guest
    // draws no pointer, so hiding the host cursor would just look broken.
    // Short-circuit order matters: the cursor is only read once that holds.
    overGuest = hitTest == HTCLIENT
                && IsGuestMouseLive()
                && GetCursorPos (&pt)
                && ScreenToClient (m_hwnd, &pt);

    if (overGuest && CrtMonitorActive())
    {
        // With the desk scene, "over the display" means over the curved
        // glass itself, not the bounding rect around it.
        SceneHitResult  hit = DeskSceneHit (pt.x, pt.y);

        overGuest = hit.target == SceneHitResult::Target::Glass;
    }
    else if (overGuest)
    {
        overGuest = pt.x >= m_viewportBoundsPx.left && pt.x < m_viewportBoundsPx.right
                 && pt.y >= m_viewportBoundsPx.top  && pt.y < m_viewportBoundsPx.bottom;
    }

    if (overGuest)
    {
        SetCursor (nullptr);
        result = DxuiMessageResult::Handled;
    }
    else if (hitTest == HTCLIENT && DeskSceneActive() && !m_d3dRenderer.IsFullscreen()
             && m_deskScene.MaxBezelTiltRad() > 0.0f
             && GetCursorPos (&pt) && ScreenToClient (m_hwnd, &pt))
    {
        // A HAND OVER THE TILT MARKS, because they are the one thing on the
        // monitor you can take hold of. Resolved through the same hit test
        // the press uses, so the cursor changes exactly where the drag would
        // actually start -- a hand offered anywhere else would be a promise
        // the press does not keep.
        SceneHitResult  hit = DeskSceneHit (pt.x, pt.y);

        if (hit.target == SceneHitResult::Target::BezelTilt || m_bezelTilting)
        {
            SetCursor (LoadCursorW (nullptr, IDC_HAND));
            result = DxuiMessageResult::Handled;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnMouseWheel
//
//  The wheel frames the desk scene, and WHICH WAY it frames it depends on
//  what sent it. Windows gives a touchpad and a mouse the same message, so
//  the message alone cannot say -- but the DELTA can. A wheel is detented and
//  reports whole WHEEL_DELTA notches; a precision touchpad reports the finger,
//  in fractions of one. So:
//
//    - a touchpad PINCH arrives as Ctrl+wheel, and zooms.
//    - a whole notch is a mouse wheel, and zooms -- a mouse has no pinch, and
//      taking its zoom away to gain a pan would be a poor trade.
//    - anything else is a two-finger slide, and PANS.
//
//  Which makes the touchpad behave like every map and every drawing program:
//  drag to move, pinch to scale. Horizontal wheel joins in for the same
//  reason it used to be ignored -- panning on one axis with no way to reach
//  the other is worse than not panning, and a precision touchpad sends both
//  axes, so the pair of them is a real pan and either alone is not.
//
//  The test is deliberately one-sided: only a delta that is NOT a whole notch
//  is treated as a touchpad. A touchpad whose driver rounds to 120 keeps
//  zooming, which is what it did before this -- no worse, just not better.
//
//  Touchscreen gestures do not come through here at all. They arrive as
//  WM_GESTURE and are handled in OnGesture, whose pinch and one-finger drag
//  are untouched by any of this.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMouseWheel (WPARAM wParam, LPARAM lParam, bool horizontal)
{
    int     delta   = GET_WHEEL_DELTA_WPARAM (wParam);
    POINT   pt      = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    bool    pinch   = (GET_KEYSTATE_WPARAM (wParam) & MK_CONTROL) != 0;
    bool    detent  = (delta % WHEEL_DELTA) == 0;
    float   notch   = 0.0f;
    float   factor  = 1.0f;



    // Paddle capture owns the pointer: it is hidden and confined to the
    // window because someone is playing a game with it. Nothing about a wheel
    // notch there is a request to reframe the scene, and having the desk zoom
    // out from under a game is the kind of thing that reads as a glitch.
    //
    // Mouse mode is NOT excluded. The guest mouse has no wheel to steal the
    // notch from, so zooming stays available while pointing.
    //
    // Fullscreen is: the picture owns the client and the desk is not on
    // screen, so there is no camera to move -- only hidden state to
    // scramble for the return to windowed.
    if (delta == 0 || !DeskSceneActive() || m_paddleCaptured ||
        m_d3dRenderer.IsFullscreen())
    {
        return DxuiMessageResult::NotHandled;
    }

    // Screen -> client: WM_MOUSEWHEEL packs the point in SCREEN coordinates,
    // unlike every button message, which is a reliable way to zoom toward the
    // wrong place on a window that is not at the origin.
    if (m_hwnd == nullptr || !ScreenToClient (m_hwnd, &pt))
    {
        return DxuiMessageResult::NotHandled;
    }

    // Fractional notches matter: a precision touchpad sends many small
    // deltas rather than one WHEEL_DELTA, and rounding them to whole notches
    // turns a smooth slide into a series of jumps.
    notch = (float) delta / (float) WHEEL_DELTA;

    // Shift turns the slide into an orbit -- the touchpad's spin-the-scene,
    // matching Shift+drag on the buttons. Content follows the fingers, and
    // Windows reports a downward slide negative and a rightward one positive
    // (see PanSceneByNotch), so both axes take the negative.
    //
    // THE KEYBOARD IS ASKED DIRECTLY, not the message. Shift+slide is the
    // gesture Windows itself repurposes into horizontal scrolling, and the
    // precision-touchpad path synthesizes those wheel messages WITHOUT
    // MK_SHIFT in their keystate -- so the one gesture this branch exists
    // for arrived flagless, fell through, and panned.
    if (((GET_KEYSTATE_WPARAM (wParam) & MK_SHIFT) != 0 ||
         (GetKeyState (VK_SHIFT) & 0x8000) != 0) && !pinch)
    {
        // BOTH SIGNS ARE DERIVED FROM THE PAN, not measured one gesture at
        // a time. The pan is the one slide mapping the user has validated:
        // panX -= notch reads as content-follows-fingers, which pins what
        // this hardware reports -- a rightward or upward slide arrives
        // NEGATIVE. The drag's bargain then fixes the orbit: drag right is
        // yaw negative and drag up is pitch negative, so a slide, carrying
        // a negative notch for the same motion, multiplies by POSITIVE
        // rates on both axes. The first flip fixed the vertical axis alone
        // and left horizontal inverted, which read as "backwards" the
        // moment the scene was spun side to side.
        if (horizontal)
        {
            OrbitSceneBy (notch * s_kOrbitRadPerNotch, 0.0f);
        }
        else
        {
            OrbitSceneBy (0.0f, notch * s_kOrbitRadPerNotch);
        }

        return DxuiMessageResult::Handled;
    }

    if (!pinch && !detent)
    {
        return PanSceneByNotch (notch, horizontal);
    }

    // A horizontal wheel that got this far is a tilt wheel, which has no
    // second axis to pair with and so still means nothing here.
    if (horizontal)
    {
        return DxuiMessageResult::NotHandled;
    }

    factor = std::pow (s_kSceneZoomStep, notch);

    ZoomSceneAt (pt, factor);

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnGesture
//
//  Touchscreen pinch and drag, framing the desk scene exactly as the wheel
//  and a mouse drag do.
//
//  Windows reports both gestures ABSOLUTELY -- a pinch as the current
//  separation between the fingers, a pan as the current point -- so each step
//  is the ratio or difference against the previous report, and GF_BEGIN
//  reseeds rather than measuring the first step of a new gesture against
//  wherever the last one ended.
//
//  A pinch is a RATIO, not a difference: fingers moving 20 px apart means
//  something quite different starting from 40 px apart than from 400, and
//  only the ratio matches what the hand is doing.
//
//  ptsLocation is in SCREEN coordinates like the wheel's point, not client
//  like the button messages -- the same trap, in a second place.
//
//  A single-finger drag pans without the zoom gate the mouse path applies.
//  On a mouse the press has to be shared with clicking, so panning waits
//  until there is something to pan to; a touch drag on the backdrop has no
//  competing meaning, and refusing to move would just read as broken.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnGesture (WPARAM wParam, LPARAM lParam)
{
    GESTUREINFO  info    = {};
    POINT        pt      = {};
    bool         handled = false;



    info.cbSize = sizeof (info);

    // Fullscreen shows the picture, not the desk: no gesture moves a camera
    // that is not on screen.
    if (!DeskSceneActive() || m_d3dRenderer.IsFullscreen() ||
        !GetGestureInfo (reinterpret_cast<HGESTUREINFO> (lParam), &info))
    {
        return DxuiMessageResult::NotHandled;
    }

    pt.x = info.ptsLocation.x;
    pt.y = info.ptsLocation.y;

    if (m_hwnd == nullptr || !ScreenToClient (m_hwnd, &pt))
    {
        return DxuiMessageResult::NotHandled;
    }

    switch (wParam)
    {
        case GID_ZOOM:
        {
            if ((info.dwFlags & GF_BEGIN) != 0 || m_gestureZoomLast == 0)
            {
                m_gestureZoomLast = info.ullArguments;
                handled           = true;
                break;
            }

            if (info.ullArguments > 0)
            {
                ZoomSceneAt (pt, (float) info.ullArguments / (float) m_gestureZoomLast);
                m_gestureZoomLast = info.ullArguments;
            }

            handled = true;
            break;
        }

        case GID_PAN:
        {
            RECT   box    = m_deskScene.Composition().viewportPx;
            float  width  = (float) (box.right - box.left);
            float  height = (float) (box.bottom - box.top);

            // TWO fingers dragging together orbit; one finger pans. Windows
            // reports the finger separation in ullArguments for a pan, and a
            // single finger reports zero -- which is the whole discriminator.
            // The two-finger form is claimed unconditionally: it has no
            // widget meaning to preserve and orbit works at any zoom.
            if (info.ullArguments > 0)
            {
                if ((info.dwFlags & GF_BEGIN) != 0)
                {
                    m_gesturePanLastPx = pt;
                    handled            = true;
                    break;
                }

                OrbitSceneBy (-(float) (pt.x - m_gesturePanLastPx.x) * OrbitRadPerPx(),
                              (float) (pt.y - m_gesturePanLastPx.y) * OrbitRadPerPx());

                m_gesturePanLastPx = pt;
                handled            = true;
                break;
            }

            // NOT CLAIMED when there is nothing to pan to. Windows promotes an
            // unhandled gesture to mouse input, so claiming a one-finger drag
            // at 1x would swallow it and leave touch unable to work the drive
            // widgets at all -- the scene would gain a pan it cannot use and
            // lose every touch drag that meant something else.
            //
            // Declined for the same reason while the guest mouse is live: a
            // one-finger drag then is someone pointing, and the promotion to
            // mouse input is exactly what has to keep happening.
            if (m_sceneView.zoom <= 1.0f || width <= 0.0f || height <= 0.0f ||
                IsGuestMouseLive())
            {
                break;
            }

            if ((info.dwFlags & GF_BEGIN) != 0)
            {
                m_gesturePanLastPx = pt;
                handled            = true;
                break;
            }

            m_sceneView.panX += ((float) (pt.x - m_gesturePanLastPx.x) / width)  * 2.0f;
            m_sceneView.panY -= ((float) (pt.y - m_gesturePanLastPx.y) / height) * 2.0f;

            ClampSceneView();
            InvalidateSceneComposition();

            m_gesturePanLastPx = pt;
            handled            = true;
            break;
        }

        default:
            break;
    }

    if ((info.dwFlags & GF_END) != 0)
    {
        m_gestureZoomLast = 0;
    }

    return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
//  Press half of the click pair. Unlike the release, this handler mostly
//  ARMS things -- pressed visuals, capture, dismissals -- and leaves the
//  acting to OnLButtonUp, which is what makes a press-then-drag-off cancel
//  the way a Windows button should.
//
//  Paddle capture short-circuits everything: the pointer is hidden and
//  confined, so the press is fire button 0 and no chrome may see it.
//
//  Otherwise the press is broadcast rather than routed. Every widget gets its
//  pressed state set, because they are hit-tested independently and only one
//  can match; there is no consumption chain to respect on the way down.
//
//  Two dismissal behaviors ride along:
//
//    focus ring   a click anywhere hands focus back to the pointer, so the
//                 painted keyboard-focus visual is dropped rather than left
//                 stranded on whatever the keyboard last selected
//    open menu    a press OUTSIDE the menu strip closes the menu. The popup
//                 takes no capture, so the owning window is the only thing
//                 positioned to notice a click-away
//
//  The UI shell's return is explicitly ignored: nothing later in this handler
//  varies on it, and the message is always reported as not fully handled so
//  the default processing still runs.
//
//  The guest mouse button is gated on IsGuestMouseLive rather than merely
//  active -- guest software must have turned the mouse ON -- so a click at a
//  BASIC prompt is not silently swallowed by a device nobody is reading.
//
//  A press on empty scene BACKGROUND arms a pan instead. It is armed last,
//  after every widget has had its say, so dragging can never steal a click
//  from something that wanted it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnLButtonDown (WPARAM wParam, LPARAM lParam)
{
    HRESULT            hr          = S_OK;
    DxuiMessageResult  result      = DxuiMessageResult::NotHandled;
    int                x           = ((int) (short) LOWORD (lParam));
    int                y           = ((int) (short) HIWORD (lParam));
    bool               consumed    = false;
    bool               toolbarTook = false;
    bool               chromeTook  = false;



    UNREFERENCED_PARAMETER (wParam);

    // While paddle-captured, the left button is fire button 0 and the
    // pointer is hidden/confined, so nothing else acts on the press.
    if (m_paddleCaptured)
    {
        PushPaddleButton (0, true);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_paddleCaptured, S_OK);

    SetCapture (m_hwnd);

    // A mouse press drops the keyboard chrome-focus ring: clicking anywhere
    // hands focus back to the pointer, so the painted focus visual should
    // not linger. A click that opens a menu is then tracked via IsOpen().
    if (m_chromeFocusIndex != s_kChromeFocusNone)
    {
        SetChromeFocusIndex (s_kChromeFocusNone);
    }

    // A press outside the menu strip dismisses any open menu. The strip
    // itself toggles / hover-switches via the menu bar's own mouse
    // handling, and the popup-backed dropdown receives row clicks
    // directly; the popup takes no capture, so the owner drives this.
    if (m_mainMenu.IsOpen())
    {
        RECT  strip = m_mainMenu.GetBounds();

        if (x < strip.left || x >= strip.right || y < strip.top || y >= strip.bottom)
        {
            m_mainMenu.Hide();
        }
    }

    //  Before the rest of the chrome: the bar sits in its own band and
    //  overlaps nothing, so an event inside it belongs to it and to nothing
    //  else.
    if (OfferMouseToChangeBanner (DxuiMouseEventKind::Down, x, y))
    {
        return DxuiMessageResult::Handled;
    }

    // Command toolbar press (button press states + slider drag start).
    toolbarTook = m_toolbar.OnToolbarLButtonDown (x, y);

    if (toolbarTook)
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    chromeTook = chromeTook || toolbarTook;

    if (MachineHasCaseSwitches())
    {
        Apple2cSwitchBar::Part  part = m_switchBar.GetPartAt (x, y);

        // The strip is painted only when a frame is presented, and a static
        // screen presents none; a key that went down asks for one.
        if (m_switchBar.SetPressedPart (part))
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }

        chromeTook = chromeTook || part != Apple2cSwitchBar::Part::None;
    }

    // The UI shell (debug panels, on-screen buttons) gets first crack at
    // the press. We still report the message as not fully handled, but its
    // verdict is not moot: a widget that took the press owns the release
    // too, and the scene gestures below must not arm over it.
    consumed   = m_uiShell.OnLButtonDown (x, y);
    chromeTook = chromeTook || consumed;

    // //c Mouse mode (non-capturing): a press over the emulator display is
    // the guest mouse button -- but only once guest software has turned the
    // mouse on, so clicks aren't silently swallowed at a BASIC prompt.
    // Chrome outside the display already had its chance above. With the
    // desk scene, "over the display" means over the curved glass itself
    // (release stays deliberately ungated, matching the 2D contract).
    if (IsGuestMouseLive())
    {
        bool  overDisplay = false;

        if (CrtMonitorActive())
        {
            SceneHitResult  hit = DeskSceneHit (x, y);

            overDisplay = hit.target == SceneHitResult::Target::Glass;
        }
        else
        {
            overDisplay = x >= m_viewportBoundsPx.left && x < m_viewportBoundsPx.right
                       && y >= m_viewportBoundsPx.top  && y < m_viewportBoundsPx.bottom;
        }

        if (overDisplay)
        {
            m_machine.GetMouse()->SetButton (true);
        }
    }

    // Pan arms LAST, and only on empty scene background: anything the user
    // could have meant to click has already claimed the press by here, so a
    // drag can never steal one. Zoomed all the way out there is nothing to
    // pan to, so it stays disarmed and an idle drag on the backdrop does
    // nothing rather than wobbling a scene that already fits.
    //
    // NOT WHILE THE GUEST MOUSE IS LIVE. //c Mouse mode maps the host pointer
    // absolutely, and it keeps doing so over the BACKGROUND -- so a drag out
    // there is very likely someone steering the guest cursor toward an edge,
    // and panning would both hijack that drag and freeze the guest pointer
    // for its duration. GuestMouseLive rather than Active on purpose: at a
    // BASIC prompt nothing is reading the mouse, so panning stays available.
    // Shift turns the press into an orbit -- the touchpad's road to it, where
    // a right-drag is awkward. Ahead of the pan arm, and regardless of zoom.
    // Never in fullscreen, where the desk is not on screen.
    if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen() &&
        (wParam & MK_SHIFT) != 0 && !m_mainMenu.IsOpen() &&
        PointInSceneRect (x, y) && !chromeTook)
    {
        BeginSceneOrbit (x, y);
        m_sceneOrbitLeftBtn = true;
        result = DxuiMessageResult::Handled;
        BAIL_OUT_IF (true, S_OK);
    }

    // The compass outranks everything on the scene: it is drawn on top,
    // so a press where it sits belongs to it.
    if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen() && !m_mainMenu.IsOpen() &&
        m_sceneCompass.OnPointerDown (x, y))
    {
        result = DxuiMessageResult::Handled;
        BAIL_OUT_IF (true, S_OK);
    }

    // Grabbing a tilt mark starts the bezel drag. Before the orbit, which is
    // the only other thing a press on the scene begins, and which would
    // otherwise swallow the gesture.
    if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen() && !m_mainMenu.IsOpen()
        && !IsGuestMouseLive() && m_deskScene.MaxBezelTiltRad() > 0.0f)
    {
        SceneHitResult  hit = DeskSceneHit (x, y);

        if (hit.target == SceneHitResult::Target::BezelTilt)
        {
            m_bezelTilting      = true;
            m_bezelTiltStartPx  = POINT { x, y };
            m_bezelTiltStartRad = m_deskScene.BezelTiltRad();

            result = DxuiMessageResult::Handled;
            BAIL_OUT_IF (true, S_OK);
        }
    }

    // A PLAIN DRAG ON THE SCENE TURNS IT, AND THE SCENE INCLUDES THE MACHINE.
    // It used to pan, and only when zoomed -- so at rest the most natural
    // gesture in the window did nothing at all. Turning is what people expect
    // of a 3D thing under the mouse; the pan still lives on the touchpad's
    // two-finger slide, beside the zoom on its pinch.
    //
    // ARMED OVER ANYTHING THE SCENE SHOWS, not only over the empty backdrop.
    // Requiring a target miss meant the picture and the drives' faces -- the
    // largest and most obvious surfaces in the window, the ones a hand
    // reaches for first -- were dead to the gesture, and a drag begun on the
    // machine did nothing while the same drag an inch to the left turned it.
    // A press there still MEANS what it meant; it is the release that decides
    // which, exactly as it does for a button or for the compass:
    //
    //     travel past the slop  ->  a turn, and the release ends it
    //     released inside it    ->  a click, and the chain below runs
    //
    // The two grab targets are excluded because a press on them already
    // begins a different drag or a command: the bezel's tilt marks (armed
    // above, which bails before reaching here) and a drive's door.
    //
    // ONLY ON THE SCENE'S OWN RECT, and only where no chrome took the press.
    // A scene hit of None is true of every pixel of the toolbar and the
    // status bar as well -- there is no machine out there to hit -- so arming
    // on that alone armed a turn under the command buttons, and the release
    // that would have fired them ended the turn instead.
    if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen() &&
        !m_mainMenu.IsOpen() && !IsGuestMouseLive() &&
        PointInSceneRect (x, y) && !chromeTook)
    {
        SceneHitResult  hit    = DeskSceneHit (x, y);
        bool            onDoor = hit.target == SceneHitResult::Target::Drive
                                 && hit.region == DriveWidgetRegion::Eject;

        if (!onDoor && hit.target != SceneHitResult::Target::BezelTilt)
        {
            BeginSceneOrbit (x, y);
            m_sceneOrbitLeftBtn = true;
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
//  Where clicks actually DO things. This is a strict priority chain -- the
//  first consumer to claim the release ends the walk -- and the order is the
//  contract:
//
//    paddle capture   fire button 0, capture retained
//    toolbar          button dispatch / mute / slider drop
//    //c switch strip reset, 80/40, keyboard
//    UI shell         debug panels and on-screen buttons
//    input-mode button
//    suppressed drop  (see below)
//    drive widgets    body browses, eject ejects then browses
//    viewport         paddle re-grab
//
//  Three of these are subtle enough to be worth stating outright.
//
//  The input-mode button routes through ToggleInputMappingMode -- the same
//  entry point as the Machine menu command -- rather than assigning the mode
//  directly, so leaving a mode still neutralizes its held arrow / X / Z
//  inputs. Setting the field would strand whatever was down at the time.
//
//  The suppressed-click check exists because a completed OLE drop onto a
//  drive widget is followed by a WM_LBUTTONUP from the OS that lands on that
//  same drive. Without swallowing it, dropping a disk image mounts it and
//  then immediately opens the file-open dialog on top of it.
//
//  The paddle re-grab predicate is captured BEFORE StartPaddleCapture runs,
//  because that call sets m_paddleCaptured -- re-testing afterwards reads
//  false and the bail below would not fire.
//
//  The guest mouse button release at the end is deliberately NOT viewport-
//  gated, unlike the press. A press inside the viewport released outside it
//  must still clear the button, or the guest is left with it stuck down.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    HRESULT                 hr            = S_OK;
    DxuiMessageResult       result        = DxuiMessageResult::NotHandled;
    int                     x             = ((int) (short) LOWORD (lParam));
    int                     y             = ((int) (short) HIWORD (lParam));
    DriveWidgetRegion       region        = DriveWidgetRegion::None;
    Apple2cSwitchBar::Part  switchPart    = Apple2cSwitchBar::Part::None;
    bool                    toolbarTook   = false;
    bool                    shellTook     = false;
    bool                    onSwitchPart  = false;
    bool                    wasSuppressed = false;
    bool                    driveTook     = false;
    bool                    canGrabPaddle = false;



    UNREFERENCED_PARAMETER (wParam);

    //  The release is what makes a button fire, so the bar has to see both
    //  halves of the click.
    if (OfferMouseToChangeBanner (DxuiMouseEventKind::Up, x, y))
    {
        return DxuiMessageResult::Handled;
    }

    // Ending a pan consumes the release. The press it began with never
    // reached a widget, so letting the release run the click chain would fire
    // whatever the cursor happened to land on after the drag.
    if (m_scenePanning)
    {
        m_scenePanning = false;
        ReleaseCapture();
        return DxuiMessageResult::Handled;
    }

    // The compass's release fires its click or ends its drag, and either
    // way the press never reached a widget, so the click chain stays out
    // of it.
    if (m_sceneCompass.OnPointerUp (x, y))
    {
        ReleaseCapture();
        return DxuiMessageResult::Handled;
    }

    // Likewise a drag orbit's release -- BUT ONLY IF IT TURNED. A press that
    // armed one and never travelled is a click, and swallowing its release
    // would make every press on the machine do nothing at all. Fall through
    // and let the chain below read it as the click it was.
    if (m_sceneOrbiting && m_sceneOrbitLeftBtn)
    {
        bool  turned = m_sceneOrbitMoved;

        m_sceneOrbiting   = false;
        m_sceneOrbitMoved = false;

        if (turned)
        {
            ReleaseCapture();
            return DxuiMessageResult::Handled;
        }
    }

    // ...and a bezel tilt's, which also writes where it came to rest. Saved
    // on release rather than on every step of the drag: the tilt is a
    // preference, not an animation, and a file rewritten per mouse-move is a
    // file rewritten a hundred times a second.
    if (m_bezelTilting)
    {
        m_bezelTilting = false;
        ReleaseCapture();
        PersistBezelTilt();
        return DxuiMessageResult::Handled;
    }

    // While paddle-captured, the left button is fire button 0; release it
    // and keep the capture (the transient click-capture path is bypassed).
    if (m_paddleCaptured)
    {
        PushPaddleButton (0, false);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_paddleCaptured, S_OK);

    ReleaseCapture();

    // Command toolbar release: click dispatch / mute toggle / slider drop.
    toolbarTook = m_toolbar.OnToolbarLButtonUp (x, y);

    if (toolbarTook)
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    BAIL_OUT_IF (toolbarTook, S_OK);

    // //c switch strip: latch the switch / fire the (Ctrl-gated) reset on
    // release over a part. Captured before the pressed-part is cleared.
    if (MachineHasCaseSwitches())
    {
        switchPart = m_switchBar.GetPartAt (x, y);

        if (m_switchBar.SetPressedPart (Apple2cSwitchBar::Part::None))
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }
    }

    shellTook = m_uiShell.OnLButtonUp (x, y);

    BAIL_OUT_IF (shellTook, S_OK);

    onSwitchPart = switchPart != Apple2cSwitchBar::Part::None;

    if (onSwitchPart)
    {
        // A latched switch is drawn sunk or proud on the next present, and
        // over a static screen this click is the only thing asking for one.
        HandleSwitchBarClick (switchPart);
        m_d3dRenderer.MarkRedrawNeeded();
    }

    BAIL_OUT_IF (onSwitchPart, S_OK);

    // If we just finished an OLE drop on a drive widget, the OS posts
    // a WM_LBUTTONUP that lands here on top of the drive. Swallow it
    // so the user doesn't see the file-open dialog pop up immediately
    // after the dropped image mounts.
    wasSuppressed = m_dragDropTarget.ConsumeSuppressedClick();

    BAIL_OUT_IF (wasSuppressed, S_OK);

    // Drive clicks. Scene active: the 3D drives resolve through the hit
    // tester with the same region semantics (slot = eject + browse, body =
    // browse); otherwise the 2D widget walk. Either way the actions route
    // through the identical handlers -- the scene only changes how hits are
    // found, never what they do.
    if (DeskSceneActive())
    {
        POINT           pt      = { x, y };
        bool            inStrip = m_d3dRenderer.IsFullscreen() &&
                                  m_stripRectPx.bottom > m_stripRectPx.top &&
                                  PtInRect (&m_stripRectPx, pt);
        SceneHitResult  sceneHit = inStrip ? StripHit (x, y) : DeskSceneHit (x, y);

        // ONLY THE DOOR ACTS. The body region stays for hover -- the
        // tooltip that names the disk -- but a click there does nothing:
        // opening a drive is done by its door, and a whole case that
        // browses on any touch turned every stray click into a dialog.
        if (sceneHit.target == SceneHitResult::Target::Drive &&
            sceneHit.region == DriveWidgetRegion::Eject)
        {
            Eject (6, sceneHit.driveIndex);

            // A browse opened from the strip pins it (the FSM must not
            // auto-hide under the dialog).
            m_stripBrowseOpen = inStrip;
            BrowseForDisk (sceneHit.driveIndex);
            m_stripBrowseOpen = false;

            driveTook = true;
        }
    }
    else
    {
        for (DriveWidget & drive : m_driveChrome)
        {
            region = drive.HitTest (x, y);

            if (region == DriveWidgetRegion::Body || region == DriveWidgetRegion::Eject)
            {
                if (region == DriveWidgetRegion::Eject)
                {
                    Eject (6, drive.GetDrive());
                }

                // In fullscreen the widget is riding the overlay strip, and a
                // browse opened from the strip pins it (the FSM must not
                // auto-hide under the dialog).
                m_stripBrowseOpen = m_d3dRenderer.IsFullscreen();
                BrowseForDisk (drive.GetDrive());
                m_stripBrowseOpen = false;

                driveTook = true;
                break;
            }
        }
    }

    BAIL_OUT_IF (driveTook, S_OK);

    // (The standalone printer indicator's click-to-open is retired: the
    // toolbar's Printer button dispatches IDM_PRINTER_PREVIEW instead, DCR-2.)

    // A bare left-click on the emulator screen (no chrome / widget / drive
    // hit) in Paddle mode re-grabs the pointer after an Esc release. The
    // predicate is captured BEFORE the call, because StartPaddleCapture sets
    // m_paddleCaptured and re-testing it afterwards would read false.
    canGrabPaddle = m_pointerMode == InputMappingMode::Paddle && !m_paddleCaptured;

    if (canGrabPaddle)
    {
        StartPaddleCapture();
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (canGrabPaddle, S_OK);

    // //c Mouse mode: any left-release drops the guest mouse button --
    // unconditionally (not viewport-gated), so a press inside the viewport
    // released outside it can never leave the guest button stuck.
    if (IsGuestMouseActive())
    {
        m_machine.GetMouse()->SetButton (false);
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRButtonDown / OnRButtonUp
//
//  In Paddle mode the right mouse button is fire button 1; otherwise the
//  message falls through to the default handler.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnRButtonDown (WPARAM wParam, LPARAM lParam)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    UNREFERENCED_PARAMETER (wParam);

    if (m_paddleCaptured)
    {
        PushPaddleButton (1, true);
        result = DxuiMessageResult::Handled;
    }
    else if (DeskSceneActive() && !m_d3dRenderer.IsFullscreen())
    {
        // Right-drag orbits the scene. Unconditionally on the scene -- unlike
        // the pan there is no widget interaction to share the button with,
        // and orbit is useful at any zoom. Not in fullscreen, where the desk
        // is not on screen and there is no camera to swing.
        SetCapture (m_hwnd);
        BeginSceneOrbit ((int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam));
        m_sceneOrbitLeftBtn = false;
        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRButtonUp
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnRButtonUp (WPARAM wParam, LPARAM lParam)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    UNREFERENCED_PARAMETER (wParam);

    if (m_sceneOrbiting && !m_sceneOrbitLeftBtn)
    {
        int      x     = (int) (short) LOWORD (lParam);
        int      y     = (int) (short) HIWORD (lParam);
        bool     still = std::abs (x - m_sceneOrbitStartPx.x) <= 3 &&
                         std::abs (y - m_sceneOrbitStartPx.y) <= 3;
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        m_sceneOrbiting = false;
        ReleaseCapture();

        // Two motionless right-clicks in double-click time reset the orbit
        // -- the pose home button, without stealing a key.
        if (still)
        {
            if (nowMs - m_sceneOrbitTapMs <= (int64_t) GetDoubleClickTime())
            {
                m_sceneView.orbitYawRad   = 0.0f;
                m_sceneView.orbitPitchRad = 0.0f;
                m_sceneOrbitTapMs         = 0;
                InvalidateSceneComposition();
            }
            else
            {
                m_sceneOrbitTapMs = nowMs;
            }
        }

        return DxuiMessageResult::Handled;
    }

    if (m_paddleCaptured)
    {
        PushPaddleButton (1, false);
        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseGuestKeys
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReleaseGuestKeys()
{
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::defer_lock);



    // The modifier and fire keys go through the mixer, which takes the lock
    // itself when it writes: the release reaches the machine now, or when a
    // rebuild in progress finishes.
    m_appleModifierContribution = GamePortContribution();
    m_gamePortMixer.ReleaseSource (GamePortSource::AppleModifierKeys);
    m_gamePortMixer.ReleaseSource (GamePortSource::FireKeys);

    // A machine switch holds the lock exclusively while it replaces the
    // devices; the new machine starts with every key up anyway.
    if (!lifetime.try_lock())
    {
        return;
    }

    if (m_machine.GetRefs().keyboard != nullptr)
    {
        m_machine.GetRefs().keyboard->SetKeyDown (false);
        m_machine.GetRefs().keyboard->BeginKeyRepeat (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleHostMetaShortcut
//
//  Consume host-meta keys that never reach the emulated //e keyboard: menu
//  mnemonic navigation, F10 menu focus, and Ctrl+V paste. Returns true when
//  the key was claimed. Every accelerator the menu advertises is dispatched
//  from the accelerator table instead, so nothing here duplicates one; an
//  unmatched Alt+key deliberately falls through, since Alt is the //e's
//  Open / Closed Apple and belongs to the guest.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::HandleHostMetaShortcut (WPARAM vk, bool ctrlHeld, bool altHeld)
{
    bool  claimed = true;



    // The mnemonic arm both TESTS and ACTS -- HandleAltKey opens the menu --
    // so it has to lead the ladder rather than fold into a predicate.
    if (altHeld && vk >= 0x20 && vk <= 0x7E && m_mainMenu.HandleAltKey ((wchar_t) vk))
    {
        // Claimed by the menu bar.
    }
    else if (vk == VK_F10 && !ctrlHeld && !altHeld)
    {
        // F10 enters the chrome keyboard-focus ring at the first menu title
        // (dropdown closed). Exiting the ring is handled inside
        // HandleChromeFocusKey, which intercepts F10 once the ring is active.
        SetChromeFocusIndex (s_kChromeFocusMenuFirst);
    }
    else if (vk == 'V' && ctrlHeld && !altHeld)
    {
        // Windows has already synthesized this combo's WM_CHAR (^V, 0x16);
        // without the swallow it lands in the guest keyboard latch AHEAD of
        // the pasted text, planting an invisible control byte in the input
        // line (the classic paste-then-SYNTAX-ERROR).
        m_swallowMetaChar = true;
        m_clipboardManager->PasteFromClipboard (m_hwnd);
    }
    else
    {
        claimed = false;
    }

    return claimed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyAppleModifierKeys
//
//  Submits the host modifier state to the game-port mixer as the //e modifier
//  keys' contribution: left Alt -> Open Apple (PB0, $C061), right Alt ->
//  Closed Apple (PB1, $C062), Shift -> Shift (PB2, $C063). GetKeyState gives
//  the canonical left/right state, so a modifier stays asserted while either
//  physical key is still down. A no-op on the ][/][+ where the keyboard is
//  not an Apple //e keyboard.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyAppleModifierKeys (WPARAM vk, bool keyDown)
{
    constexpr size_t  kOpenAppleButton  = 0;
    constexpr size_t  kSolidAppleButton = 1;
    constexpr size_t  kShiftButton      = 2;
    HRESULT           hr                = S_OK;
    auto            * iieKbd            = m_machine.GetRefs().iieKeyboard;
    bool              isAlt             = vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU;
    bool              isShift           = vk == VK_SHIFT;



    BAIL_OUT_IF (iieKbd == nullptr || !(isAlt || isShift), S_OK);

    if (isAlt)
    {
        m_appleModifierContribution.buttons.set (kOpenAppleButton,  (GetKeyState (VK_LMENU) & 0x8000) != 0);
        m_appleModifierContribution.buttons.set (kSolidAppleButton, (GetKeyState (VK_RMENU) & 0x8000) != 0);
    }
    else
    {
        m_appleModifierContribution.buttons.set (kShiftButton, keyDown);
    }

    m_gamePortMixer.Submit (GamePortSource::AppleModifierKeys, m_appleModifierContribution);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
//  Skims off every keystroke the SHELL owns, then hands the rest to the guest.
//
//  The pre-checks run in a fixed order, each an escape hatch that must not be
//  reachable from the guest's side:
//
//    Esc in paddle mode  releases the pointer capture and returns the mapping
//                        to Off. This is first because a captured pointer has
//                        hidden the cursor, and Esc is the only way out
//    chrome focus ring   while a menu title / button / drive holds keyboard
//                        focus (or any dropdown is open), the ring owns every
//                        keydown, so typed letters cannot leak into the //e
//                        while the user is arrowing through a menu
//    meta shortcuts      host-level chords
//
//  Whatever survives is by definition the guest's, and is delivered through
//  the VIEWPORT rather than straight to the machine's keyboard. That
//  indirection is
//  the point: the viewport is configured with SetConsumesInput and
//  SetWantsAllKeys, so it forwards everything -- Esc, Tab, arrows included --
//  back to OnViewportKey, and guest input stays on the single Dxui input path
//  (FR-034) instead of the shell reaching around the framework.
//
//  Always reports Handled, including on the bail paths: once a keystroke has
//  been classified as shell-owned it must not also reach default processing.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    // The keyboard device is read here on the UI thread; a machine switch
    // on the CPU thread holds the lifetime lock exclusively while it
    // replaces the devices, and a key that lands in that window is dropped.
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);
    HRESULT          hr        = S_OK;
    bool             consumed  = false;
    bool             ctrlHeld  = false;
    bool             altHeld   = false;
    bool             isRepeat  = (lParam & s_kPreviousKeyDownLParamBit) != 0;
    AppleKeyboard *  keyboard  = lifetime.owns_lock() ? m_machine.GetRefs().keyboard : nullptr;



    if (!lifetime.owns_lock())
    {
        return DxuiMessageResult::Handled;
    }

    // The swallow is a ONE-SHOT owned by this keydown, and clearing it here
    // is what keeps it one. Windows does not always follow a keydown with the
    // WM_CHAR its setters assume: an Alt-held key arrives as WM_SYSKEYDOWN
    // (routed here just like WM_KEYDOWN) and yields WM_SYSCHAR, which reaches
    // no handler at all. A flag left armed by one of those would eat the next
    // ordinary character the user typed. Windows queues a keydown's character
    // before the next keydown, so a swallow that is still wanted is always
    // consumed before this runs again.
    m_swallowMetaChar = false;

    // 0. Esc exits paddle mode: releases the mouse capture (cursor
    //    reappears) and returns the input mapping to Off, matching the
    //    "Esc to exit" hint on the widget.
    if (m_pointerMode == InputMappingMode::Paddle && vk == VK_ESCAPE)
    {
        SetPointerMapping (InputMappingMode::Off);
        BAIL_OUT_IF (true, S_OK);
    }

    // An open toolbar picker is modal in practice: it owns arrows, Enter and
    //    Escape so browsing the rows previews rather than typing into the //e.
    //    A flyout opened by keyboard owns them the same way.
    if (m_toolbar.OwnsKeyboard())
    {
        (void) m_toolbar.HandleKey (vk);
        BAIL_OUT_IF (true, S_OK);
    }

    // Chrome keyboard-focus ring. While a menu title / button / drive
    //    has keyboard focus (or a dropdown is open from any source), the
    //    ring owns every keydown so letters never leak through to the //e.
    if (m_chromeFocusIndex != s_kChromeFocusNone || m_mainMenu.IsOpen())
    {
        HandleChromeFocusKey (vk);
        BAIL_OUT_IF (true, S_OK);
    }

    CBR (keyboard != nullptr);

    ctrlHeld = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
    altHeld  = (GetKeyState (VK_MENU)    & 0x8000) != 0;

    consumed = HandleHostMetaShortcut (vk, ctrlHeld, altHeld);
    BAIL_OUT_IF (consumed, S_OK);

    // The chrome / settings / meta pre-checks above already skimmed off
    // every keystroke that belongs to the shell. Everything left is the
    // guest's: build a Down event and hand it to the viewport, which (with
    // SetConsumesInput + SetWantsAllKeys) forwards it to OnViewportKey for
    // the //e keyboard + game port. Routing through the viewport keeps a
    // single Dxui input path (FR-034) rather than the shell reaching into
    // the machine's keyboard directly.
    if (m_viewport != nullptr)
    {
        DxuiKeyEvent  ev;

        ev.kind   = DxuiKeyEventKind::Down;
        ev.vk     = vk;
        ev.repeat = isRepeat;
        ev.ctrl   = ctrlHeld;
        ev.alt    = altHeld;
        ev.shift  = (GetKeyState (VK_SHIFT) & 0x8000) != 0;

        (void) m_viewport->OnKey (ev);
    }

Error:
    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyUp
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnKeyUp (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (lParam);

    // A released Caps Lock is the user setting the latch, unless the press
    // was the one the latch itself sent; the latch tells the two apart.
    if (vk == VK_CAPITAL)
    {
        m_capsLockLatch.OnCapsLockKeyUp();
    }

    // Key-up is deliberately unconditional (no chrome / settings gate): a
    // release must always reach the //e so a modifier or repeat can never
    // stick when focus moved to the chrome mid-press. The viewport forwards
    // it to OnViewportKey, which performs the release.
    if (m_viewport != nullptr)
    {
        DxuiKeyEvent  ev;

        ev.kind = DxuiKeyEventKind::Up;
        ev.vk   = vk;

        (void) m_viewport->OnKey (ev);
    }

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostKeyboardLayoutIsDvorak
//
//  True when the host's active keyboard layout is a Dvorak variant. Probed
//  behaviorally rather than by KLID string, so it catches every Dvorak layout
//  (US, left/right-hand, third-party) without a hard-coded list: VkKeyScanEx
//  reports which physical key (VK code) produces 'o'. On QWERTY that is VK 'O';
//  on Dvorak 'o' lives on the physical 'S' key, so it reports VK 'S'. Unlike
//  ToUnicode this leaves no dead-key state behind. The //c keyboard switch only
//  needs to remap when the host is QWERTY -- see Apple2eKeyboard::MapTypedChar.
//
////////////////////////////////////////////////////////////////////////////////

static bool HostKeyboardLayoutIsDvorak()
{
    HKL    hkl = GetKeyboardLayout (0);
    SHORT  vk  = VkKeyScanExW (L'o', hkl);



    // vk == -1 means 'o' is unreachable on this layout -- assume QWERTY.
    return vk != -1 && LOBYTE (vk) == 'S';
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnViewportKey
//
//  Applies one guest keystroke to the Apple ][ keyboard latch and game port.
//  Everything arriving here has already been classified as the guest's.
//
//  Down, Up, and Char are three different jobs, not three cases of one:
//  Down handles control codes and joystick axes, Up disarms, and Char is the
//  only path that types a printable character.
//
//  Auto-repeat is the subtlest part. Control-code presses are gated on the
//  repeat bit so the HOST's repeat never reaches the latch -- a fresh press
//  arms the $C000 strobe once and registers the key with BeginKeyRepeat, and
//  the emulated //e then generates its own authentic repeat cadence in Tick.
//  Letting both repeats run would double the rate and sound wrong.
//
//  A key-up always calls BeginKeyRepeat(0). The //e latch holds exactly one
//  key, so a release necessarily ends the current repeat; clearing it also
//  stops a later non-character press (a bare modifier, say) from resurrecting
//  the previous character's repeat.
//
//  Joystick emulation overlays the arrows and X / Z, and only when the mode
//  is on AND a game-port paddle bank exists. Three details matter:
//
//    - driveJoystick is recomputed per event, so a mode change between a
//      press and its release is honored and nothing is left held
//    - arrows are WITHHELD from the keyboard latch in this mode; a held
//      direction would otherwise flood $C000 and starve a game's reads
//    - the fire buttons are re-resolved on EVERY key event, not just X / Z,
//      so an Alt press re-applies its Open / Closed-Apple mapping without
//      clobbering a still-held X, and a released X cannot leave a button
//      stuck down while Alt is still held
//
//  Opposing arrows resolve last-pressed-wins via the per-axis memory, which
//  is what makes a quick left-right reversal read as a reversal rather than
//  as centered.
//
//  The Char path feeds MapTypedChar so the //c keyboard switch can remap to
//  Dvorak. The host's own layout is probed live and pushed in, so a host that
//  is ALREADY Dvorak skips the remap instead of translating twice. Clipboard
//  paste bypasses this path entirely and calls PressKey directly -- pasted
//  text is never remapped, matching the hardware encoder.
//
//  Always returns true: nothing here bubbles back to the framework, since the
//  shell's escape routes live in the OnKeyDown pre-checks, not in the sink.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnViewportKey (const DxuiKeyEvent & ev)
{
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);
    bool                                 hasKeyboard = false;



    // A machine switch holds the lock exclusively while it replaces the
    // devices; a key that lands in that window is dropped.
    if (!lifetime.owns_lock())
    {
        return false;
    }

    // Arrow keys double as the emulated joystick axes / the X / Z keys as
    // fire buttons when "Map Arrows to Joystick" is on AND a game-port
    // paddle bank is present. Recomputed per event so a mode change between
    // press and release is always honored.
    bool  driveJoystick = m_arrowsJoystick &&
                          (m_machine.GetRefs().iieSoftSwitches != nullptr ||
                           m_machine.GetRefs().gamePort != nullptr);
    // The guest owns every key that reaches here either way; with no keyboard
    // device there is simply nothing to deliver it to.
    hasKeyboard = m_machine.GetRefs().keyboard != nullptr;

    if (hasKeyboard && ev.kind == DxuiKeyEventKind::Down)
    {
        WPARAM           vk         = ev.vk;
        Byte             appleCode  = 0;
        AppleSpecialKey  specialKey = AppleSpecialKey::Left;
        bool             isSpecial  = AppleKeyMapping::TryMapVkToSpecialKey (vk, specialKey);
        bool             hasTheKey  = !isSpecial ||
                                      m_machine.GetRefs().keyboard->MapSpecialKey (specialKey) != 0;

        // A named key the running machine's keyboard does not have was never
        // pressed as far as the guest is concerned, so it must not raise
        // any-key-down either -- $C010 reporting a key held while $C000 holds
        // nothing is a state the hardware cannot be in.
        if (hasTheKey)
        {
            m_machine.GetRefs().keyboard->SetKeyDown (true);
        }

        ApplyAppleModifierKeys (vk, true);

        // TAB and Escape reach us twice: once as this keydown, and again as
        // the WM_CHAR Windows manufactures from it. The key route below is
        // the authoritative one -- it is the only one that can tell the TAB
        // key from Ctrl+I, which sends the same $09 -- so the character is
        // swallowed. Set on repeats too, since each repeated keydown brings
        // its own character along. Nothing here depends on that character
        // actually arriving: OnKeyDown clears the flag on the way in, so an
        // Alt-held TAB or Escape, whose WM_SYSCHAR never reaches OnChar,
        // cannot leave it armed for the next key.
        if (isSpecial && AppleKeyMapping::DoesSpecialKeySynthesizeChar (specialKey))
        {
            m_swallowMetaChar = true;
        }

        // Arrows / TAB / Escape / DELETE are delivered as KEYS, and the
        // device decides both which code each sends and whether this machine
        // has it at all. Gated on the auto-repeat bit so the host OS repeat
        // never reaches the latch; a fresh press arms the $C000 strobe once
        // and registers the key for the emulator's own authentic //e
        // auto-repeat cadence (Tick). With "Map Arrows to Joystick" on (and a
        // game-port paddle bank present), arrow keys are withheld from the
        // keyboard latch so a held direction cannot flood $C000 and starve a
        // joystick game's reads.
        if (!ev.repeat && isSpecial && !(driveJoystick && AppleKeyMapping::IsArrowVk (vk)))
        {
            appleCode = m_machine.GetRefs().keyboard->PressSpecialKey (specialKey);

            if (appleCode != 0)
            {
                m_machine.GetRefs().keyboard->BeginKeyRepeat (appleCode);
            }
        }

        // Record the last-pressed direction per axis so opposing keys
        // resolve last-pressed-wins, then re-resolve both axes from the
        // current key state.
        if (driveJoystick && AppleKeyMapping::IsArrowVk (vk))
        {
            if (vk == VK_LEFT || vk == VK_RIGHT)
            {
                m_lastHorizontalArrowVk = vk;
            }
            else
            {
                m_lastVerticalArrowVk = vk;
            }

            UpdateJoystickAxesFromKeys();
        }

        // Re-resolve the joystick fire buttons on every key event in
        // joystick mode (not just on X / Z) so that an Alt press/release
        // re-applies its Open/Closed-Apple mapping without clobbering a
        // still-held X / Z, and a released X / Z can't leave a button stuck
        // while Alt is down. The matching X / Z WM_CHAR is suppressed in
        // OnChar so the letters don't also type into the //e keyboard latch.
        if (driveJoystick)
        {
            UpdateJoystickButtonsFromKeys();
        }
    }
    else if (hasKeyboard && ev.kind == DxuiKeyEventKind::Up)
    {
        WPARAM           vk         = ev.vk;
        AppleSpecialKey  specialKey = AppleSpecialKey::Left;
        bool             isSpecial  = AppleKeyMapping::TryMapVkToSpecialKey (vk, specialKey);
        bool             hasTheKey  = !isSpecial ||
                                      m_machine.GetRefs().keyboard->MapSpecialKey (specialKey) != 0;

        // The same question the press asked, asked again: a key this machine
        // does not have was never pressed, so its release must not undo the
        // state some other key is still holding. Gating only the press left
        // the up arrow on a ][+ able to clear any-key-down, and disarm the
        // repeat, while a real key was still down -- $C010 reporting nothing
        // held while $C000 holds a character.
        if (hasTheKey)
        {
            m_machine.GetRefs().keyboard->SetKeyDown (false);

            // Disarm auto-repeat on release. The //e latch holds a single
            // key, so a key-up always ends the current repeat; this also
            // clears any stale armed key so a later non-character press
            // (e.g. a bare modifier) can never resurrect the previous
            // character's repeat.
            m_machine.GetRefs().keyboard->BeginKeyRepeat (0);
        }

        // Release the //e Open/Closed-Apple and Shift modifiers as the host
        // releases the physical keys.
        ApplyAppleModifierKeys (vk, false);

        if (m_arrowsJoystick && AppleKeyMapping::IsArrowVk (vk))
        {
            UpdateJoystickAxesFromKeys();
        }

        if (m_arrowsJoystick)
        {
            UpdateJoystickButtonsFromKeys();
        }
    }
    else if (hasKeyboard)   // DxuiKeyEventKind::Char
    {
        WPARAM  ch = ev.vk;

        if (ch >= 1 && ch <= 127)
        {
            // //c keyboard switch: remap physical keystrokes to Dvorak when the
            // switch is engaged. A no-op on the //e, when the switch is out, and
            // when the HOST layout is already Dvorak (the shell feeds that live
            // so MapTypedChar can skip the remap and avoid double-translating).
            // Clipboard paste feeds PressKey directly (not this path), so pasted
            // text is never remapped -- matching the hardware encoder.
            Byte  code = static_cast<Byte> (ch);

            if (m_machine.GetRefs().iieKeyboard != nullptr)
            {
                m_machine.GetRefs().iieKeyboard->SetHostKeyboardDvorak (HostKeyboardLayoutIsDvorak());

                code = m_machine.GetRefs().iieKeyboard->MapTypedChar (code);
            }

            m_machine.GetRefs().keyboard->PressKey (code);
            m_machine.GetRefs().keyboard->BeginKeyRepeat (code);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnViewportMouse
//
//  IDxuiViewportInputSink. The Apple ][ has no viewport-rect mouse mapping
//  in the current build: paddle input is a captured relative-motion mode
//  driven directly from OnMouseMove (SetCapture snaps the cursor to
//  center), and the joystick maps to arrow keys -- neither fits the
//  viewport's absolute-rect forwarding. Returns false so any future
//  in-viewport click continues to bubble to the chrome hit-testing that
//  owns it today.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnViewportMouse (const DxuiMouseEvent & ev)
{
    UNREFERENCED_PARAMETER (ev);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateJoystickAxesFromKeys
//
//  Host UI thread. Resolves the four arrow keys into the two emulated
//  joystick axes and submits them to the game-port mixer as the arrow keys'
//  contribution. The mixer writes them to whichever game port the machine
//  has while the arrow keys own the axes; the PREAD timer ($C070 /
//  $C064-$C067) turns them into analog readings.
//
//  Reads real-time physical key state via GetAsyncKeyState rather than the
//  per-thread GetKeyState table, which can desync (and leave an axis stuck)
//  if a key-up is lost to a focus change. Opposing keys resolve
//  last-pressed-wins so a rolling reversal flips the axis instead of
//  canceling to center.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateJoystickAxesFromKeys()
{
    GamePortContribution  contribution;
    bool                  left         = false;
    bool                  right        = false;
    bool                  up           = false;
    bool                  down         = false;
    Byte                  x            = GamePortState::kPaddleCenter;
    Byte                  y            = GamePortState::kPaddleCenter;



    left  = (GetAsyncKeyState (VK_LEFT)  & 0x8000) != 0;
    right = (GetAsyncKeyState (VK_RIGHT) & 0x8000) != 0;
    up    = (GetAsyncKeyState (VK_UP)    & 0x8000) != 0;
    down  = (GetAsyncKeyState (VK_DOWN)  & 0x8000) != 0;

    if (left && right)
    {
        x = (m_lastHorizontalArrowVk == VK_RIGHT) ? s_kPaddleAxisMax : s_kPaddleAxisMin;
    }
    else if (left)
    {
        x = s_kPaddleAxisMin;
    }
    else if (right)
    {
        x = s_kPaddleAxisMax;
    }

    if (up && down)
    {
        y = (m_lastVerticalArrowVk == VK_DOWN) ? s_kPaddleAxisMax : s_kPaddleAxisMin;
    }
    else if (up)
    {
        y = s_kPaddleAxisMin;
    }
    else if (down)
    {
        y = s_kPaddleAxisMax;
    }

    contribution.paddle = std::array<Byte, 2> { x, y };
    m_gamePortMixer.Submit (GamePortSource::ArrowKeys, contribution);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateJoystickButtonsFromKeys
//
//  Host UI thread. Resolves the X / Z letter keys into the two emulated
//  joystick fire buttons and submits them to the game-port mixer as the fire
//  keys' contribution. The mixer writes PB0/PB1 to the //e keyboard's
//  Open/Closed-Apple lines or the ][/][+ game port's pushbuttons ($C061,
//  $C062), whichever the machine has.
//
//  Reads real-time physical key state via GetAsyncKeyState, matching the
//  axis helper so a key-up lost to a focus change can't wedge a button on.
//  The fire state is OR'd with the host left/right Alt keys so the X / Z
//  mapping coexists with the existing Alt->button mapping instead of
//  clobbering a held Alt.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateJoystickButtonsFromKeys()
{
    GamePortContribution  contribution;
    bool                  button0      = false;
    bool                  button1      = false;



    // Only read the physical keys while WE are the foreground app. The async
    // key state is global, so a held Alt during the Alt-Tab switcher (or any
    // time another app is active) would otherwise keep re-pressing Open-Apple /
    // button 0 in the guest every frame -- e.g. re-triggering a Print Shop
    // print on the way out. Foreground reads normally (no added latency); not
    // foreground leaves the buttons released. Matches the input gate used
    // elsewhere (GetForegroundWindow() != m_hwnd).
    if (GetForegroundWindow() == m_hwnd)
    {
        button0 = (GetAsyncKeyState (static_cast<int> (s_kJoystickButton0Vk)) & 0x8000) != 0 ||
                  (GetKeyState      (VK_LMENU)                                & 0x8000) != 0;
        button1 = (GetAsyncKeyState (static_cast<int> (s_kJoystickButton1Vk)) & 0x8000) != 0 ||
                  (GetKeyState      (VK_RMENU)                                & 0x8000) != 0;
    }

    contribution.buttons.set (0, button0);
    contribution.buttons.set (1, button1);
    m_gamePortMixer.Submit (GamePortSource::FireKeys, contribution);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInputMappingMode
//
//  Sets the host input mapping mode (Off / Joystick / Paddle) and persists
//  it. Leaving Paddle drops the mouse capture. Joystick resolves the axes
//  and fire buttons from the current key state so a held arrow / X / Z
//  takes effect immediately. Off and Paddle both neutralize the
//  key-derived stick and buttons so the game port reads neutral; Paddle
//  then centers the paddle and grabs the mouse.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetInputMappingMode (InputMappingMode mode)
{
    // Combined PRESET setter (button cycle + legacy callers): selects BOTH
    // axes of the split model. The Machine-menu items toggle the
    // axes independently via SetArrowsJoystick / SetPointerMapping, so
    // e.g. Joystick keys + Mouse pointer can coexist (disjoint game-port
    // lines); presets deliberately reset the other axis.
    switch (mode)
    {
        case InputMappingMode::Joystick:
            SetPointerMapping (InputMappingMode::Off);
            SetArrowsJoystick (true);
            break;

        case InputMappingMode::Paddle:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Paddle);
            break;

        case InputMappingMode::Mouse:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Mouse);
            break;

        case InputMappingMode::Off:
        default:
            SetArrowsJoystick (false);
            SetPointerMapping (InputMappingMode::Off);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetArrowsJoystick
//
//  Keys axis of the split input model: maps arrows + X/Z onto the joystick
//  axes / fire buttons. Independent of the Mouse pointer (disjoint hardware),
//  but mutually exclusive with Paddle -- both drive the game-port paddle
//  lines -- so enabling it drops an active Paddle. Turning it off neutralizes
//  the key-derived stick and buttons so a held key can't stay stuck (axes
//  left alone while Paddle owns them).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetArrowsJoystick (bool on)
{
    // Mirror of the rule in SetPointerMapping: the Keys axis drives PDL0/1,
    // so enabling it must drop an active Paddle (they fight over the same
    // game-port lines). Mouse uses a separate slot card and may coexist.
    if (on && m_pointerMode == InputMappingMode::Paddle)
    {
        SetPointerMapping (InputMappingMode::Off);
    }

    m_arrowsJoystick = on;
    SyncGamePortAxisOwner();
    SyncInputModeUi();

    if (on)
    {
        UpdateJoystickAxesFromKeys();
        UpdateJoystickButtonsFromKeys();
        return;
    }

    // The arrows drive nothing now: the axes go to whichever owner is left
    // (center when none) and the fire buttons release. Alt held as Open or
    // Solid-Apple is the modifier keys' own contribution, so it stays pressed.
    m_gamePortMixer.ReleaseSource (GamePortSource::ArrowKeys);
    m_gamePortMixer.ReleaseSource (GamePortSource::FireKeys);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPointerMapping
//
//  Pointer axis of the split input model: Off / Paddle (capturing) /
//  Mouse (non-capturing //c IOU mouse). Paddle and Mouse are mutually
//  exclusive by construction -- both claim the host pointer.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetPointerMapping (InputMappingMode pointer)
{
    InputMappingMode  prev = m_pointerMode;



    if (pointer == InputMappingMode::Joystick)   // not a pointer mode
    {
        pointer = InputMappingMode::Off;
    }

    // Paddle owns the game-port paddle axes (PDL0/1) -- the same lines the
    // arrows->joystick remap drives -- so entering Paddle must drop the Keys
    // axis. Mouse is a separate slot card (disjoint lines) and may coexist
    // with Joystick, so only Paddle clears it. Enforced here (not just in the
    // SetInputMappingMode presets) so the per-segment / menu toggle paths
    // honor the same paddle-vs-joystick exclusivity.
    if (pointer == InputMappingMode::Paddle && m_arrowsJoystick)
    {
        SetArrowsJoystick (false);
    }

    if (prev == InputMappingMode::Paddle && pointer != InputMappingMode::Paddle)
    {
        StopPaddleCapture();
    }

    // Leaving Mouse: release a held guest button so it can't stick, and
    // drop the absolute target so the guest mouse stops tracking.
    if (prev == InputMappingMode::Mouse && pointer != InputMappingMode::Mouse
        && m_machine.GetMouse() != nullptr)
    {
        m_machine.GetMouse()->SetButton (false);
        m_machine.GetMouse()->ClearHostTarget();
    }

    m_pointerMode = pointer;
    SyncGamePortAxisOwner();
    SyncInputModeUi();

    if (pointer == InputMappingMode::Paddle)
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        m_paddleAxisX = (float) s_kPaddleCenterByte;
        m_paddleAxisY = (float) s_kPaddleCenterByte;
        PushPaddlePosition();

        // THE HUD NOTICE SAYS THIS NOW, in both presentations. Entering
        // paddle mode used to force a tooltip up for eight seconds, because
        // the capture means the hover that would normally dismiss one never
        // fires -- so it had to time out instead. That put a panel over the
        // chrome it was anchored to, on top of whatever tooltip the pointer
        // had already summoned, and it said what the notice over the picture
        // now says for exactly as long as the capture lasts.
        //
        // AND WHATEVER IS ALREADY UP GOES NOW. The click that turns paddle
        // mode on is a click ON a control, so its tooltip is showing -- and
        // the capture that follows takes the pointer, so the move that would
        // dismiss it never comes. It would sit there until its lifetime ran
        // out, which is a long time to leave a balloon over a game.
        m_toolbarTooltip.HideImmediate();
        m_driveTooltip.HideImmediate();
        m_switchBarTooltip.HideImmediate();

        StartPaddleCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyAutomaticControllerSelection
//
//  UI thread. A controller the policy chose on its own still has to reach the
//  rest of the shell: the arrow keys and the paddle give up the axes, the
//  choice is written to the machine's prefs, and the user is told which
//  controller it was (FR-032).
//
//  An adoption says nothing. The user did not choose anything -- the same
//  controller came back on another port -- so announcing it would report a
//  change the user did not make.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyAutomaticControllerSelection (const std::wstring & description, bool isAdoption)
{
    InputModeRules::State  state;



    state.arrowsJoystick = m_arrowsJoystick;
    state.mousePaddle    = (m_pointerMode == InputMappingMode::Paddle);
    state               = InputModeRules::AfterSelectingController (state);

    if (m_arrowsJoystick && !state.arrowsJoystick)
    {
        SetArrowsJoystick (false);
    }

    if (!state.mousePaddle && m_pointerMode == InputMappingMode::Paddle)
    {
        SetPointerMapping (InputMappingMode::Off);
    }

    SyncGamePortAxisOwner();
    SyncInputModeUi();

    // Over the picture for a few seconds, never a dialog: the user did not
    // ask about this, so stopping the machine to have it acknowledged would
    // interrupt them to report something they may not care about.
    if (!isAdoption && !description.empty())
    {
        ShowTransientNotice (L"Controller selected: " + description);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncGamePortAxisOwner
//
//  Who owns PDL0/PDL1: a connected chosen controller, else paddle mode, else
//  arrows-to-joystick, else nothing, which rests the axes at center. The rule
//  itself is in InputModeRules so it can be asserted without a machine.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncGamePortAxisOwner()
{
    InputModeRules::State  state;



    state.arrowsJoystick = m_arrowsJoystick;
    state.mousePaddle    = (m_pointerMode == InputMappingMode::Paddle);

    if (m_controllerService != nullptr)
    {
        ControllerInputService::Snapshot  snapshot = m_controllerService->GetSnapshot();

        state.hasController        = snapshot.selection.has_value();
        state.isControllerAttached = snapshot.isSelectedConnected;
    }

    m_gamePortMixer.SetAxisOwner (InputModeRules::GetAxisOwner (state));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncInputModeUi
//
//  Common tail for the axis setters: refresh the toggle button's displayed
//  mode, then persist the pair into the current machine's prefs.
//
//  The two setters call each other to enforce paddle-vs-joystick exclusivity,
//  so one user action can land here twice. Both passes write the same live
//  state, and the second is what ends up on disk.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncInputModeUi()
{
    SyncSelectorState();
    PersistInputModeForMachine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncSelectorState
//
//  Pushes the split-model state (Keys, Pointer, mouse availability) into
//  the device selector.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSelectorState()
{
    bool  countChanged = m_inputCluster.SetInputState (m_arrowsJoystick, m_pointerMode,
                                                       m_machine.GetMouse() != nullptr && m_mouseConnected);
    RECT  bounds       = m_toolbar.GetBounds();



    // Whether the mouse exists decides HOW MANY segments there are, and the
    // segment rects belong to Layout -- so a state push that adds or drops
    // the mouse has to re-lay the entry, or the new segment keeps the empty
    // rect it was left with and never paints. A machine switch does exactly
    // that: it reflows the chrome first and syncs this state after. The
    // picker list is handed over again for the same reason.
    if (countChanged)
    {
        m_toolbar.SetDropDownItems (EmulatorCommands::kIdInput, m_inputCluster.GetPickerItems());

        if (bounds.right > bounds.left)
        {
            m_toolbar.Layout (bounds, m_scaler);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyDefaultPointerForMachine
//
//  A //c with its mouse connected and no pointer mapping chosen
//  defaults Pointer to Mouse -- a runtime nudge, not persisted, and
//  invisible until guest mouse software runs (firmware-live gate). Called
//  after the per-machine connected states are seeded at launch and on
//  machine switch.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyDefaultPointerForMachine()
{
    if (m_machine.GetMouse() != nullptr && m_mouseConnected
        && m_pointerMode == InputMappingMode::Off)
    {
        // State only -- NO chrome work here. This runs on the CPU thread
        // during SwitchMachine, and the selector sync measures text through
        // the Dxui renderer, which is UI-thread-only (DxuiAssertUiThread
        // fired on a //c -> //e switch). Both paths already sync on the UI
        // thread afterwards: a machine switch posts WM_APP_DXUI_UPDATE_TITLE,
        // whose handler runs ReflowChromeForMachineChange, and the launch
        // path lays out the chrome later in Initialize.
        m_pointerMode = InputMappingMode::Mouse;

        // SyncSelectorState touches Dxui (text measurement) and asserts the
        // UI thread. On a machine switch this runs
        // on the CPU thread, so defer the chrome reflection to the post-switch
        // handler on the UI thread (WM_APP_DXUI_UPDATE_TITLE, posted by the
        // UpdateWindowTitle at the end of SwitchMachine). On the UI thread
        // (launch, or before the window exists) reflect it immediately.
        bool  offUiThread = (m_hwnd != nullptr) &&
                            (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());

        if (offUiThread)
        {
            return;
        }

        SyncSelectorState();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CycleInputMappingMode
//
//  Advances the input mapping mode Off -> Joystick -> Paddle -> Off.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CycleInputMappingMode()
{
    InputMappingMode  next    = InputMappingMode::Off;
    InputMappingMode  current = GetDisplayInputMode();



    // Mouse (mouse-capable machines only) deliberately precedes Paddle:
    // entering Paddle CAPTURES the pointer (clicks become fire buttons),
    // so any mode placed after Paddle would be unreachable by clicking
    // the toggle. Mouse mode is non-capturing, so the toggle stays
    // clickable and Paddle remains reachable from it.
    switch (current)
    {
        case InputMappingMode::Off:
            next = InputMappingMode::Joystick;
            break;

        case InputMappingMode::Joystick:
            next = (m_machine.GetMouse() != nullptr && m_mouseConnected)
                       ? InputMappingMode::Mouse
                       : InputMappingMode::Paddle;
            break;

        case InputMappingMode::Mouse:
            next = InputMappingMode::Paddle;
            break;

        case InputMappingMode::Paddle:
        default:
            next = InputMappingMode::Off;
            break;
    }

    SetInputMappingMode (next);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleInputMappingMode
//
//  Radio-group selection for the Machine-menu Joystick / Paddle items:
//  picks `target`, or turns mapping Off when `target` is already active so
//  re-selecting the current mode clears it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ToggleInputMappingMode (InputMappingMode target)
{
    switch (target)
    {
        case InputMappingMode::Joystick:
            SetArrowsJoystick (!m_arrowsJoystick);
            break;

        case InputMappingMode::Paddle:
            SetPointerMapping (m_pointerMode == InputMappingMode::Paddle
                                   ? InputMappingMode::Off : InputMappingMode::Paddle);
            break;

        case InputMappingMode::Mouse:
            SetPointerMapping (m_pointerMode == InputMappingMode::Mouse
                                   ? InputMappingMode::Off : InputMappingMode::Mouse);
            break;

        default:
            SetInputMappingMode (InputMappingMode::Off);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClipPaddleCursorToClient
//
//  The pointer confined to the picture and parked in the middle -- the half of
//  "captured" the user can see. Kept apart from StartPaddleCapture because the
//  clip has to be laid on twice: once at the grab, and again when a cancel the
//  chrome's own layout raised has to be taken back (see OnCancelMode).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ClipPaddleCursorToClient()
{
    RECT   client  = {};
    POINT  topLeft = {};
    POINT  botRt   = {};
    RECT   clip    = {};
    POINT  center  = {};



    if (m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
    {
        return;
    }

    topLeft.x = client.left;
    topLeft.y = client.top;
    botRt.x   = client.right;
    botRt.y   = client.bottom;
    ClientToScreen (m_hwnd, &topLeft);
    ClientToScreen (m_hwnd, &botRt);

    clip.left   = topLeft.x;
    clip.top    = topLeft.y;
    clip.right  = botRt.x;
    clip.bottom = botRt.y;
    ClipCursor (&clip);

    center.x = (client.right  - client.left) / 2;
    center.y = (client.bottom - client.top)  / 2;
    ClientToScreen (m_hwnd, &center);
    SetCursorPos (center.x, center.y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartPaddleCapture
//
//  Hides and confines the cursor to the client area, parks it at center,
//  and begins relative tracking. No-op unless the mode is Paddle, the
//  window owns the foreground, and capture isn't already active. The
//  current (held) paddle position is pushed so a re-grab after an Esc
//  release resumes from where the dial was left.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StartPaddleCapture()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_pointerMode != InputMappingMode::Paddle, S_OK);
    BAIL_OUT_IF (m_paddleCaptured,                        S_OK);
    BAIL_OUT_IF (m_hwnd == nullptr,                       S_OK);
    BAIL_OUT_IF (GetForegroundWindow() != m_hwnd,         S_OK);

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
    m_captureReflowMs = 0;

    if (!m_d3dRenderer.IsFullscreen())
    {
        ReflowChromeForChangeBand();
        m_captureReflowMs = ChangeBannerNowMs();
    }

    SetCapture (m_hwnd);

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
//  StopPaddleCapture
//
//  Releases the mouse capture and cursor clip, restores the cursor, and
//  clears the fire buttons so a held mouse button doesn't stick. No-op
//  when not captured. Leaves the input mode unchanged, so the dial holds
//  its position for a later re-grab.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StopPaddleCapture()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_paddleCaptured, S_OK);

    m_paddleCaptured = false;

    ClipCursor (nullptr);

    if (GetCapture() == m_hwnd)
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
    if (!m_d3dRenderer.IsFullscreen())
    {
        ReflowChromeForChangeBand();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePaddleFromMouse
//
//  Maps one WM_MOUSEMOVE while paddle-captured: the motion relative to the
//  client center is scaled (s_kPaddleSweepInches of DPI-scaled travel =
//  full range) into the held paddle axes, then the cursor is snapped back
//  to center for unbounded relative motion. The zero-delta move our own
//  recenter generates is ignored.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdatePaddleFromMouse (int xClient, int yClient)
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

    GetClientRect (m_hwnd, &client);
    centerX = (client.right  - client.left) / 2;
    centerY = (client.bottom - client.top)  / 2;
    dx      = xClient - centerX;
    dy      = yClient - centerY;

    // The SetCursorPos recenter below re-enters here with a zero delta.
    BAIL_OUT_IF (dx == 0 && dy == 0, S_OK);

    dpi = GetDpiForWindow (m_hwnd);
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
    ClientToScreen (m_hwnd, &center);
    SetCursorPos (center.x, center.y);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushPaddlePosition
//
//  Submits the held paddle axes to the game-port mixer as the mouse paddle's
//  contribution, keeping its buttons as they are.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PushPaddlePosition()
{
    Byte  x = (Byte) (m_paddleAxisX + 0.5f);
    Byte  y = (Byte) (m_paddleAxisY + 0.5f);



    m_mousePaddleContribution.paddle = std::array<Byte, 2> { x, y };
    m_gamePortMixer.Submit (GamePortSource::MousePaddle, m_mousePaddleContribution);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushPaddleButton
//
//  Submits a paddle fire button (0 or 1) to the game-port mixer as part of
//  the mouse paddle's contribution, keeping its axes as they are.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PushPaddleButton (int index, bool pressed)
{
    constexpr int  kMouseButtonCount = 2;
    HRESULT        hr                = S_OK;



    CBRA (index >= 0 && index < kMouseButtonCount);

    m_mousePaddleContribution.buttons.set (static_cast<size_t> (index), pressed);
    m_gamePortMixer.Submit (GamePortSource::MousePaddle, m_mousePaddleContribution);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
//  Decides whether a synthesized WM_CHAR belongs to the guest, and drops it
//  otherwise. This handler is almost entirely suppression: Windows manufactures
//  a WM_CHAR from a WM_KEYDOWN whether or not anything consumed the keydown,
//  so every case OnKeyDown claimed has to be claimed again here.
//
//  Three things are swallowed:
//
//    overlay input   a letter typed while the settings panel, an open menu, or
//                    the chrome focus ring owns the keyboard would otherwise
//                    ALSO drop into the //e latch
//    fire keys       X / Z are joystick buttons in joystick mode and were
//                    already handled as key transitions, so their characters
//                    must not type as well -- mirroring how the arrows are
//                    withheld from the latch
//    OS auto-repeat  the host repeat rate would flood $C000 and confuse games
//                    that poll it; the emulated //e generates its own repeat
//                    in real time (AppleKeyboard::TickAutoRepeat) from the
//                    single latch
//
//  What survives goes through the viewport, not straight to the keyboard, so
//  characters travel the same Dxui path as the key transitions (FR-034).
//
//  Handled is returned either way. A character the shell deliberately
//  suppressed must not reach DefWindowProc any more than one the guest took.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnChar (WPARAM ch, LPARAM lParam)
{
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);
    bool                                 isRepeat = (lParam & s_kPreviousKeyDownLParamBit) != 0;



    // A machine switch holds the lock exclusively while it replaces the
    // devices; a character that lands in that window is dropped.
    if (!lifetime.owns_lock())
    {
        return DxuiMessageResult::Handled;
    }

    // A host-meta shortcut (Ctrl+V paste), or a special key already
    // delivered by name (TAB, Escape), claimed the keydown, but Windows
    // synthesized its control character anyway; swallow exactly that one
    // char so it never reaches the guest a second time.
    if (m_swallowMetaChar)
    {
        m_swallowMetaChar = false;
        return DxuiMessageResult::Handled;
    }



    // Suppress the WM_CHAR that Windows synthesizes from a WM_KEYDOWN
    // already consumed by overlay UI (settings panel / open menu) or by the
    // chrome keyboard-focus ring. Without this, a letter typed while a menu
    // title / button / drive is focused would also drop into the //e latch.
    bool  overlayOwnsIt = m_uiShell.IsCapturingInput() ||
                          m_chromeFocusIndex != s_kChromeFocusNone ||
                          m_toolbar.OwnsKeyboard();

    // In joystick mode the X / Z keys are fire buttons (handled in OnKeyDown
    // / OnKeyUp), so swallow their WM_CHAR to keep the letters from also
    // typing into the //e keyboard latch -- mirroring how arrow keys are
    // withheld from the latch.
    bool  isFireKey = m_arrowsJoystick &&
                      (m_machine.GetRefs().iieSoftSwitches != nullptr ||
                       m_machine.GetRefs().gamePort != nullptr) &&
                      (ch == L'x' || ch == L'X' || ch == L'z' || ch == L'Z');

    // What survives all of the above is the guest's. `isRepeat` drops Windows
    // OS auto-repeat: the host repeat rate would flood $C000 and confuse
    // real-time games that poll it. A fresh press is latched once and
    // registered for the emulator's own authentic //e auto-repeat cadence
    // (driven in real time by AppleKeyboard::TickAutoRepeat, so the emulation
    // speed does not move it).
    bool  isGuestChar = m_machine.GetRefs().keyboard != nullptr &&
                        !overlayOwnsIt &&
                        !isRepeat &&
                        !isFireKey;



    // Route through the viewport so the //e keyboard latch is fed on the same
    // Dxui path as the key transitions (FR-034).
    if (isGuestChar && m_viewport != nullptr)
    {
        DxuiKeyEvent  ev;

        ev.kind = DxuiKeyEventKind::Char;
        ev.vk   = ch;

        (void) m_viewport->OnKey (ev);
    }

    // Consumed either way: a character the shell suppressed must not fall
    // through to DefWindowProc any more than one the guest took.
    return DxuiMessageResult::Handled;
}
