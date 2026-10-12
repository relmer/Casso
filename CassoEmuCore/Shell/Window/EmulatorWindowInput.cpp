#include "Pch.h"
#include "Shell/Components/ShellInput.h"

#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/ClipboardManager.h"
#include "Shell/Components/ShellWindow.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/Components/ShellAudio.h"
#include "Shell/Components/ShellUpdater.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Controllers/InputModeRules.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/PlayerModeRules.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
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
#include "Machines/Apple2/Common/SiriusJoyport.h"
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
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Shell/ControllerInputThread.h"





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
        m_chrome->m_mainMenu.SetFocusedMenu ((MainMenuId) index);
    }
    else
    {
        m_chrome->m_mainMenu.ClearFocus();
    }

    m_chrome->m_toolbar.SetFocusIndex ((index >= s_kChromeFocusToolbarFirst && index <= s_kChromeFocusToolbarLast)
                                           ? index - s_kChromeFocusToolbarFirst : -1);

    m_disks->GetDriveChrome()[0].SetFocused (index == s_kChromeFocusDrive0);
    m_disks->GetDriveChrome()[1].SetFocused (index == s_kChromeFocusDrive1);
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
    bool  menuIsOpen  = m_chrome->m_mainMenu.IsOpen();
    bool  onMenuTitle = index >= s_kChromeFocusMenuFirst && index <= s_kChromeFocusMenuLast;
    bool  onToolbar   = index >= s_kChromeFocusToolbarFirst && index <= s_kChromeFocusToolbarLast;



    // A toolbar picker or a keyboard-opened flyout owns navigation; Escape
    // closes it and leaves the entry focused.
    if (m_chrome->m_toolbar.OwnsKeyboard())
    {
        (void) m_chrome->m_toolbar.HandleKey (vk);
    }

    // An open dropdown owns navigation; delegate and reconcile the ring.
    else if (menuIsOpen)
    {
        bool  ringOwned = (m_chromeFocusIndex != s_kChromeFocusNone);
        int   openIdx   = (int) m_chrome->m_mainMenu.GetOpenMenu();

        m_chrome->m_mainMenu.HandleKey (vk);

        if (m_chrome->m_mainMenu.IsOpen())
        {
            // Still open: a ring-owned menu tracks the (possibly switched)
            // title. A menu opened outside the ring (Alt mnemonic / mouse)
            // stays un-owned so closing it returns to the guest rather than
            // stranding focus on a title the user never Tab'd to.
            if (ringOwned)
            {
                SetChromeFocusIndex ((int) m_chrome->m_mainMenu.GetOpenMenu());
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
        m_chrome->m_mainMenu.Open ((MainMenuId) index, true);
    }

    // A toolbar entry is focused: Enter, Space or Down activates it, which
    // for a picker opens its list and for the volume entry opens its flyout
    // with the slider taking the keys that follow.
    else if (onToolbar && (vk == VK_DOWN || vk == VK_RETURN || vk == VK_SPACE))
    {
        m_chrome->m_toolbar.ActivateFocused();
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
            m_disks->BrowseForDisk (m_disks->GetDriveChrome()[0].GetDrive());
        }
        else if (index == s_kChromeFocusDrive1)
        {
            m_disks->BrowseForDisk (m_disks->GetDriveChrome()[1].GetDrive());
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
    POINT              pointer      = { x, y };
    bool               leftDown     = (wParam & MK_LBUTTON) != 0;
    bool               shellHandled = false;
    DriveWidget *      wpDrive      = nullptr;
    DriveWidget *      infoDrive    = nullptr;
    int64_t            nowMs        = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                          std::chrono::steady_clock::now().time_since_epoch()).count();



    // The update indicator sits in the caption, above every band, so a move
    // over it is its own and nothing below sees it.
    if (!m_input->IsPaddleCaptured() && m_updater->OfferMouseToUpdateIndicator (DxuiMouseEventKind::Move, x, y))
    {
        return DxuiMessageResult::Handled;
    }

    // The compass sees every move: armed, it owns the gesture; idle, the
    // call is what keeps its hover highlight honest. Ahead of the drags
    // below because a press the compass took must never feed the orbit's
    // own anchor math as well.
    if (!m_input->IsPaddleCaptured() && m_scene->m_sceneCompass.OnPointerMove (x, y))
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
    if (m_scene->m_sceneOrbiting && !m_input->IsPaddleCaptured() &&
        ((m_scene->m_sceneOrbitLeftBtn && leftDown) ||
         (!m_scene->m_sceneOrbitLeftBtn && (wParam & MK_RBUTTON) != 0)))
    {
        // Under the slop this is still a click in the making, so the scene
        // must not stir: a picture that shifts a pixel under a press and
        // shifts back is worse than one that does not move at all. The
        // right-button orbit has no click to protect and turns at once.
        if (!m_scene->m_sceneOrbitMoved &&
            m_scene->m_sceneOrbitLeftBtn &&
            std::abs (x - m_scene->m_sceneOrbitStartPx.x) <= ShellDeskScene::s_kSceneOrbitSlopPx &&
            std::abs (y - m_scene->m_sceneOrbitStartPx.y) <= ShellDeskScene::s_kSceneOrbitSlopPx)
        {
            return DxuiMessageResult::Handled;
        }

        m_scene->m_sceneOrbitMoved = true;

        m_scene->UpdateSceneOrbit (x, y);
        return DxuiMessageResult::Handled;
    }

    // The recorder's volume wheel, held, follows the pointer across.
    if (m_scene->m_volumeDragging && leftDown && !m_input->IsPaddleCaptured())
    {
        m_scene->DragVolumeWheel (x, nowMs);

        return DxuiMessageResult::Handled;
    }

    // THE TILT FOLLOWS THE POINTER, not the mark. Dragging up tips the face
    // up and dragging down tips it down, whichever mark the gesture started
    // on -- the marks say which way the control goes, they are not two
    // separate handles that move in opposite senses. Screen y grows downward,
    // so the travel is negated to get "up is up".
    if (m_scene->m_bezelTilting && leftDown && !m_input->IsPaddleCaptured())
    {
        m_scene->m_deskScene.SetBezelTilt (m_scene->m_bezelTiltStartRad
                                           + ((float) (m_scene->m_bezelTiltStartPx.y - y)) * ShellDeskScene::kBezelTiltRadPerPx);
        m_scene->InvalidateSceneComposition();

        return DxuiMessageResult::Handled;
    }

    if (m_scene->m_scenePanning && leftDown && !m_input->IsPaddleCaptured())
    {
        RECT   box    = m_scene->m_deskScene.Composition().viewportPx;
        float  width  = (float) (box.right - box.left);
        float  height = (float) (box.bottom - box.top);

        if (width > 0.0f && height > 0.0f)
        {
            // A pixel of cursor travel is two NDC units across the whole
            // viewport, and NDC y runs opposite client y.
            m_scene->m_sceneView.panX = m_scene->m_scenePanStartX
                                      + ((float) (x - m_scene->m_scenePanStartPx.x) / width)  * 2.0f;
            m_scene->m_sceneView.panY = m_scene->m_scenePanStartY
                                      - ((float) (y - m_scene->m_scenePanStartPx.y) / height) * 2.0f;

            m_scene->ClampSceneView();
            m_scene->InvalidateSceneComposition();
        }

        return DxuiMessageResult::Handled;
    }

    // Paddle mode owns the pointer while captured: relative motion drives
    // the held paddle axes and the cursor is snapped back to center, so the
    // chrome never sees the move.
    if (m_input->IsPaddleCaptured())
    {
        m_input->UpdatePaddleFromMouse (x, y);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_input->IsPaddleCaptured(), S_OK);

    // //c Mouse mode (non-capturing): a move over the emulator viewport
    // drives the guest mouse via absolute mapping. Deliberately falls
    // through to normal routing — the viewport has no chrome, and moves
    // outside it (menu bar, drive band) behave exactly as before.
    if (m_input->IsGuestMouseActive())
    {
        m_input->UpdateGuestMouseFromHost (x, y);
    }

    // The desk's names scroll under the pointer too; a change re-hangs them so
    // the one that was scrolling comes back to its start.
    if (m_scene->UpdateSceneLabelHover (x, y, nowMs))
    {
        m_scene->SyncSceneDriveLabels();
        m_renderer->GetD3D().MarkRedrawNeeded();
    }

    // The desk recorder's key under the pointer shows its name over it, and
    // its volume wheel, held or under the pointer, how loud it is set -- in
    // the same label, which changes in place rather than reopening.
    {
        int    key   = -1;
        POINT  pt    = { x, y };
        RECT   wheel = m_scene->GetVolumeWheelRect();

        if (DeskSceneActive() && (m_scene->m_volumeDragging || PtInRect (&wheel, pt)))
        {
            key = ShellDeskScene::s_kVolumeWheelLabelKey;
        }
        else if (DeskSceneActive())
        {
            SceneHitResult  hit = m_scene->RecorderHit (x, y);

            key = (hit.target == SceneHitResult::Target::Recorder) ? hit.recorderKey : -1;
        }

        if (key != m_scene->m_recorderHoverKey)
        {
            m_scene->m_recorderHoverKey = key;
            m_renderer->GetD3D().MarkRedrawNeeded();
        }
    }

    // A fresh hover over a drive widget replays its basename marquee, so
    // the full filename can be re-read on demand. The same pass notes a
    // write-protected drive under the pointer so the WP tooltip can show.
    for (DriveWidget & drive : m_disks->GetDriveChrome())
    {
        RECT  outer    = drive.GetOuterRect();
        RECT  iconRect = drive.GetInfoIconRect();
        bool  inside   = x >= outer.left && x < outer.right &&
                         y >= outer.top  && y < outer.bottom;
        bool  onIcon   = drive.HasWozConflict() && PtInRect (&iconRect, pointer);

        if (drive.UpdateMarqueeHover (inside, onIcon, nowMs))
        {
            // The band's button treatment appeared or went away. A static
            // emulator picture presents no frames on its own, so without this
            // the highlight would land on whatever frame happened next.
            m_renderer->GetD3D().MarkRedrawNeeded();
        }

        if (inside && drive.IsWriteProtected())
        {
            wpDrive = &drive;
        }

        if (onIcon)
        {
            infoDrive = &drive;
        }
    }

    if (m_tapeDeck->GetWidget().UpdateHover (x, y))
    {
        m_renderer->GetD3D().MarkRedrawNeeded();
    }

    shellHandled = m_uiShell.OnMouseMove (x, y, leftDown);

    if (shellHandled)
    {
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (shellHandled, S_OK);


    // Command toolbar hover / slider drag (DCR-2). In icon-only mode the
    // hovered button's label surfaces as a tooltip (no labels on the strip).
    if (m_chrome->m_toolbar.OnToolbarMouseMove (x, y))
    {
        m_renderer->GetD3D().MarkRedrawNeeded();
    }

    {
        RECT             anchor = {};
        const wchar_t *  tip    = m_chrome->m_toolbar.GetTooltipAt (x, y, anchor);

        if (tip != nullptr)
        {
            m_chrome->m_toolbarTooltip.RequestShow (anchor, tip, nowMs);
        }
        else
        {
            m_chrome->m_toolbarTooltip.RequestHide (nowMs);
        }
    }

    // //c switch strip: hover state and a per-part tooltip (reset / 80/40 /
    // keyboard). Inert on non-//c machines (hidden).
    if (MachineHasCaseSwitches())
    {
        const wchar_t * tip          = m_chrome->m_switchBar.GetTooltipTextAt (x, y);
        bool            hoverChanged = m_chrome->m_switchBar.SetHovered (m_chrome->m_switchBar.HitTest (x, y));
        bool            partChanged  = m_chrome->m_switchBar.SetHoverPoint (x, y);

        // The strip is painted only when a frame is presented, and a static
        // screen presents none; a highlight that moved asks for one.
        if (hoverChanged || partChanged)
        {
            m_renderer->GetD3D().MarkRedrawNeeded();
        }

        if (tip != nullptr)
        {
            m_chrome->m_switchBarTooltip.RequestShow (m_chrome->m_switchBar.GetBounds(), tip, nowMs);
        }
        else
        {
            m_chrome->m_switchBarTooltip.RequestHide (nowMs);
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

        // The info icon answers first wherever it is drawn. Its target sits
        // inside the label and the widget, which explain other things, and
        // the pointer resting on the icon is asking about the icon.
        if (!DeskSceneActive() && infoDrive != nullptr)
        {
            anchor = infoDrive->GetInfoIconRect();
            tip    = m_disks->ComposeDriveInfoTooltip (infoDrive->GetDrive());
        }

        if (tip.empty() && DeskSceneActive())
        {
            for (int i = 0; i < (int) m_scene->m_sceneInfoIconRect.size(); i++)
            {
                if (m_disks->GetDriveWidgetState()[i].wozConflict && PtInRect (&m_scene->m_sceneInfoIconRect[i], pointer))
                {
                    anchor = m_scene->m_sceneInfoIconRect[i];
                    tip    = m_disks->ComposeDriveInfoTooltip (i);
                    break;
                }
            }
        }

        if (tip.empty() && DeskSceneActive())
        {
            // The name strip answers for the padlock in BOTH presentations:
            // the strip carries names and locks in fullscreen now, so the
            // lock explains itself there the same way it does on the desk.
            for (int i = 0; i < (int) m_scene->m_sceneDriveLabelRect.size(); i++)
            {
                POINT  lp = { x, y };

                if (m_scene->m_sceneDriveLabelRect[i].right > m_scene->m_sceneDriveLabelRect[i].left &&
                    PtInRect (&m_scene->m_sceneDriveLabelRect[i], lp) &&
                    m_disks->GetDriveWidgetState()[i].writeProtect.Any())
                {
                    anchor = m_scene->m_sceneDriveLabelRect[i];
                    tip    = ComposeWriteProtectTooltip (
                                 i + 1,
                                 std::filesystem::path (m_machine.GetDiskStore().GetSourcePath (6, i))
                                     .filename().wstring(),
                                 m_disks->GetDriveWidgetState()[i].writeProtect);
                    break;
                }
            }
        }

        if (tip.empty() && DeskSceneActive() && m_renderer->GetD3D().IsFullscreen())
        {
            POINT  pt = { x, y };

            if (m_scene->m_stripRectPx.bottom > m_scene->m_stripRectPx.top &&
                PtInRect (&m_scene->m_stripRectPx, pt))
            {
                SceneHitResult  sceneHit = m_scene->StripHit (x, y);

                if (sceneHit.target == SceneHitResult::Target::Drive)
                {
                    std::wstring  imageName = std::filesystem::path (
                        m_machine.GetDiskStore().GetSourcePath (6, sceneHit.driveIndex)).filename().wstring();

                    anchor = m_scene->m_stripComp.driveRectPx[sceneHit.driveIndex];
                    tip    = ComposeWriteProtectTooltip (
                                 sceneHit.driveIndex + 1, imageName,
                                 m_disks->GetDriveWidgetState()[sceneHit.driveIndex].writeProtect);

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
            m_chrome->m_driveTooltip.RequestShow (anchor, tip, nowMs);
        }
        else
        {
            m_chrome->m_driveTooltip.RequestHide (nowMs);
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

    m_updater->OnMouseLeave();

    // Drop drive marquee-hover state so re-entering the window re-triggers
    // the basename scroll.
    for (DriveWidget & drive : m_disks->GetDriveChrome())
    {
        drive.UpdateMarqueeHover (false, false, nowMs);
    }

    // Off every control, so the recorder's magnified controls ease back down
    // and the desk's scrolling name returns to its start.
    m_tapeDeck->GetWidget().UpdateHover (INT_MIN / 2, INT_MIN / 2);
    m_scene->m_recorderHoverKey = -1;

    if (m_scene->UpdateSceneLabelHover (INT_MIN / 2, INT_MIN / 2, nowMs))
    {
        m_scene->SyncSceneDriveLabels();
    }

    m_renderer->GetD3D().MarkRedrawNeeded();

    m_chrome->m_toolbar.OnToolbarMouseLeave();
    m_chrome->m_toolbarTooltip.RequestHide (nowMs);
    m_chrome->m_driveTooltip.RequestHide (nowMs);

    {
        bool  hoverChanged = m_chrome->m_switchBar.SetHovered (false);
        bool  pressChanged = m_chrome->m_switchBar.SetPressedPart (Apple2cSwitchBar::Part::None);

        if (hoverChanged || pressChanged)
        {
            m_renderer->GetD3D().MarkRedrawNeeded();
        }
    }

    m_chrome->m_switchBarTooltip.RequestHide (nowMs);

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
    RECT               wheel      = m_scene->GetVolumeWheelRect();



    // Only hide the cursor once guest software has turned the mouse on
    // (IsGuestMouseLive) -- over a BASIC prompt or a non-mouse game the guest
    // draws no pointer, so hiding the host cursor would just look broken.
    // Short-circuit order matters: the cursor is only read once that holds.
    overGuest = hitTest == HTCLIENT
                && m_input->IsGuestMouseLive()
                && GetCursorPos (&pt)
                && ScreenToClient (m_hwnd, &pt);

    if (overGuest && CrtMonitorActive())
    {
        // With the desk scene, "over the display" means over the curved
        // glass itself, not the bounding rect around it.
        SceneHitResult  hit = m_scene->DeskSceneHit (pt.x, pt.y);

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
    else if (hitTest == HTCLIENT && DeskSceneActive() &&
             (m_scene->m_volumeDragging || (GetCursorPos (&pt) && ScreenToClient (m_hwnd, &pt) &&
                                            PtInRect (&wheel, pt))))
    {
        // A hand over the volume wheel, which can be taken hold of.
        SetCursor (LoadCursorW (nullptr, IDC_HAND));
        result = DxuiMessageResult::Handled;
    }
    else if (hitTest == HTCLIENT && DeskSceneActive() && !m_renderer->GetD3D().IsFullscreen()
             && m_scene->m_deskScene.MaxBezelTiltRad() > 0.0f
             && GetCursorPos (&pt) && ScreenToClient (m_hwnd, &pt))
    {
        // A HAND OVER THE TILT MARKS, because they are the one thing on the
        // monitor you can take hold of. Resolved through the same hit test
        // the press uses, so the cursor changes exactly where the drag would
        // actually start -- a hand offered anywhere else would be a promise
        // the press does not keep.
        SceneHitResult  hit = m_scene->DeskSceneHit (pt.x, pt.y);

        if (hit.target == SceneHitResult::Target::BezelTilt || m_scene->m_bezelTilting)
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
//  The wheel frames the desk scene, except under paddle capture: the pointer
//  is then hidden and confined for a game, and a wheel notch there is not a
//  request to reframe the scene. Mouse mode is not excluded; the guest mouse
//  has no wheel, so zooming is available while pointing.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnMouseWheel (WPARAM wParam, LPARAM lParam, bool horizontal)
{
    if (m_input->IsPaddleCaptured())
    {
        return DxuiMessageResult::NotHandled;
    }

    return m_scene->OnMouseWheel (wParam, lParam, horizontal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnGesture
//
//  Touchscreen pinch and drag frame the desk scene. A one-finger drag is
//  pointing when the guest mouse is live, and the scene does not handle it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnGesture (WPARAM wParam, LPARAM lParam)
{
    return m_scene->OnGesture (wParam, lParam, m_input->IsGuestMouseLive());
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
    HRESULT                              hr          = S_OK;
    DxuiMessageResult                    result      = DxuiMessageResult::NotHandled;
    int                                  x           = ((int) (short) LOWORD (lParam));
    int                                  y           = ((int) (short) HIWORD (lParam));
    bool                                 consumed    = false;
    bool                                 chromeTook  = false;
    ScenePointerContext                  context;



    // While paddle-captured, the left button is fire button 0 and the
    // pointer is hidden/confined, so nothing else acts on the press.
    if (m_input->IsPaddleCaptured())
    {
        m_input->PushPaddleButton (0, true);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_input->IsPaddleCaptured(), S_OK);

    // The caption's update indicator, ahead of the capture and every band:
    // nothing else lives in the caption strip's client area.
    if (m_updater->OfferMouseToUpdateIndicator (DxuiMouseEventKind::Down, x, y))
    {
        return DxuiMessageResult::Handled;
    }

    SetCapture (m_hwnd);

    // A mouse press drops the keyboard chrome-focus ring: clicking anywhere
    // hands focus back to the pointer, so the painted focus visual should
    // not linger. A click that opens a menu is then tracked via IsOpen().
    if (m_chromeFocusIndex != s_kChromeFocusNone)
    {
        SetChromeFocusIndex (s_kChromeFocusNone);
    }

    // A press outside the menu strip closes an open menu.
    m_chrome->DismissMenuOutside (x, y);

    //  Before the rest of the chrome: the bar sits in its own band and
    //  overlaps nothing, so an event inside it belongs to it and to nothing
    //  else.
    if (m_disks->OfferMouseToChangeBanner (DxuiMouseEventKind::Down, x, y))
    {
        return DxuiMessageResult::Handled;
    }

    // The command toolbar and the //c switch bar.
    chromeTook = m_chrome->OnLButtonDown (x, y);

    // The UI shell (debug panels, on-screen buttons) gets first crack at
    // the press. We still report the message as not fully handled, but its
    // verdict is not moot: a widget that took the press owns the release
    // too, and the scene gestures below must not arm over it.
    consumed   = m_uiShell.OnLButtonDown (x, y);
    chromeTook = chromeTook || consumed;

    // //c Mouse mode: a press over the picture is the guest's mouse button.
    m_input->PressGuestMouseButton (x, y);

    context.isMenuOpen      = m_chrome->IsMenuOpen();
    context.isChromeTaken   = chromeTook;
    context.isGuestPointing = m_input->IsGuestMouseLive();

    result = m_scene->OnLButtonDown (x, y, wParam, context);

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
//  because that call sets m_input->IsPaddleCaptured() -- re-testing afterwards reads
//  false and the bail below would not fire.
//
//  The guest mouse button release at the end is deliberately NOT viewport-
//  gated, unlike the press. A press inside the viewport released outside it
//  must still clear the button, or the guest is left with it stuck down.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    HRESULT                              hr            = S_OK;
    DxuiMessageResult                    result        = DxuiMessageResult::NotHandled;
    int                                  x             = ((int) (short) LOWORD (lParam));
    int                                  y             = ((int) (short) HIWORD (lParam));
    DriveWidgetRegion                    region        = DriveWidgetRegion::None;
    TapeDeckRegion                       tapeRegion    = TapeDeckRegion::None;
    Apple2cSwitchBar::Part               switchPart    = Apple2cSwitchBar::Part::None;
    bool                                 toolbarTook   = false;
    bool                                 shellTook     = false;
    bool                                 onSwitchPart  = false;
    bool                                 wasSuppressed = false;
    bool                                 driveTook     = false;
    bool                                 canGrabPaddle = false;



    UNREFERENCED_PARAMETER (wParam);

    m_scene->ReleaseRecorderKey();

    //  THE CLICK-CAPTURE GOES BACK FIRST, whatever this release turns out to
    //  mean. OnLButtonDown takes it unconditionally, so every path out of
    //  here owes it back -- and the paths that returned early did not give
    //  it, leaving the pointer held by a window that had stopped expecting
    //  to hold it. The change banner's release was one: clicking Reload or
    //  Keep left every later click going to this window wherever it landed.
    //
    //  Released here rather than in each branch because the branches are the
    //  part that keeps growing, and a new early return should not have to
    //  know this.
    //
    //  PADDLE MODE IS THE EXCEPTION and holds the pointer on purpose, so it
    //  keeps it across the click; the button release below is its own.
    if (!m_input->IsPaddleCaptured())
    {
        ReleaseCapture();
    }

    //  The update indicator's own press ends here, and its click is this
    //  release landing on it.
    if (!m_input->IsPaddleCaptured() && m_updater->OfferMouseToUpdateIndicator (DxuiMouseEventKind::Up, x, y))
    {
        return DxuiMessageResult::Handled;
    }

    //  The release is what makes a button fire, so the bar has to see both
    //  halves of the click.
    if (m_disks->OfferMouseToChangeBanner (DxuiMouseEventKind::Up, x, y))
    {
        return DxuiMessageResult::Handled;
    }

    // A drag the press began on the scene ends with the release.
    if (m_scene->EndLButtonDrag (x, y))
    {
        return DxuiMessageResult::Handled;
    }

    // While paddle-captured, the left button is fire button 0; release it
    // and keep the capture (the transient click-capture path is bypassed).
    if (m_input->IsPaddleCaptured())
    {
        m_input->PushPaddleButton (0, false);
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (m_input->IsPaddleCaptured(), S_OK);

    // The command toolbar, and the //c switch bar's part under the release.
    toolbarTook = m_chrome->OnLButtonUp (x, y, switchPart);

    BAIL_OUT_IF (toolbarTook, S_OK);

    shellTook = m_uiShell.OnLButtonUp (x, y);

    BAIL_OUT_IF (shellTook, S_OK);

    onSwitchPart = switchPart != Apple2cSwitchBar::Part::None;

    if (onSwitchPart)
    {
        // A latched switch is drawn sunk or proud on the next present, and
        // over a static screen this click is the only thing asking for one.
        m_chrome->HandleSwitchBarClick (switchPart);
        m_renderer->GetD3D().MarkRedrawNeeded();
    }

    BAIL_OUT_IF (onSwitchPart, S_OK);

    // If we just finished an OLE drop on a drive widget, the OS posts
    // a WM_LBUTTONUP that lands here on top of the drive. Swallow it
    // so the user doesn't see the file-open dialog pop up immediately
    // after the dropped image mounts.
    wasSuppressed = m_window->ConsumeSuppressedDropClick();

    BAIL_OUT_IF (wasSuppressed, S_OK);

    // Drive clicks. Scene active: the 3D drives resolve through the hit
    // tester with the same region semantics (slot = eject + browse, body =
    // browse); otherwise the 2D widget walk. Either way the actions route
    // through the identical handlers -- the scene only changes how hits are
    // found, never what they do.
    if (DeskSceneActive())
    {
        driveTook = m_scene->OnLButtonUp (x, y);
    }
    else
    {
        for (DriveWidget & drive : m_disks->GetDriveChrome())
        {
            region = drive.HitTest (x, y);

            // The info icon only explains, so a click on it is taken and
            // nothing happens -- above all, the disk is not ejected.
            if (region == DriveWidgetRegion::Info)
            {
                driveTook = true;
                break;
            }

            if (region == DriveWidgetRegion::Body || region == DriveWidgetRegion::Eject)
            {
                if (region == DriveWidgetRegion::Eject)
                {
                    m_disks->Eject (6, drive.GetDrive());
                }

                // In fullscreen the widget is riding the overlay strip, and a
                // browse opened from the strip pins it (the FSM must not
                // auto-hide under the dialog).
                RECT  driveRect = drive.GetOuterRect();

                m_scene->SetStripBrowseOpen (m_renderer->GetD3D().IsFullscreen());
                m_disks->BrowseForDisk (drive.GetDrive(), &driveRect);
                m_scene->SetStripBrowseOpen (false);

                driveTook = true;
                break;
            }
        }

        tapeRegion = driveTook ? TapeDeckRegion::None : m_tapeDeck->GetWidget().HitTest (x, y);

        if (tapeRegion != TapeDeckRegion::None)
        {
            m_tapeDeck->HandleTapeClick (tapeRegion);
            driveTook = true;
        }
    }

    BAIL_OUT_IF (driveTook, S_OK);

    // (The standalone printer indicator's click-to-open is retired: the
    // toolbar's Printer button dispatches IDM_PRINTER_PREVIEW instead, DCR-2.)

    // A bare left-click on the emulator screen (no chrome / widget / drive
    // hit) in Paddle mode re-grabs the pointer after an Esc release. The
    // predicate is captured BEFORE the call, because StartPaddleCapture sets
    // m_input->IsPaddleCaptured() and re-testing it afterwards would read false.
    canGrabPaddle = m_input->GetPointerMode() == InputMappingMode::Paddle && !m_input->IsPaddleCaptured();

    if (canGrabPaddle)
    {
        m_input->StartPaddleCapture();
        result = DxuiMessageResult::Handled;
    }

    BAIL_OUT_IF (canGrabPaddle, S_OK);

    // //c Mouse mode: any left release drops the guest mouse button.
    m_input->ReleaseGuestMouseButton();

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

    if (m_input->IsPaddleCaptured())
    {
        m_input->PushPaddleButton (1, true);
        result = DxuiMessageResult::Handled;
    }
    else if (DeskSceneActive() && !m_renderer->GetD3D().IsFullscreen())
    {
        // Right-drag orbits the scene. Unconditionally on the scene -- unlike
        // the pan there is no widget interaction to share the button with,
        // and orbit is useful at any zoom. Not in fullscreen, where the desk
        // is not on screen and there is no camera to swing.
        SetCapture (m_hwnd);
        m_scene->BeginSceneOrbit ((int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam));
        m_scene->m_sceneOrbitLeftBtn = false;
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

    if (m_scene->m_sceneOrbiting && !m_scene->m_sceneOrbitLeftBtn)
    {
        int      x     = (int) (short) LOWORD (lParam);
        int      y     = (int) (short) HIWORD (lParam);
        bool     still = std::abs (x - m_scene->m_sceneOrbitStartPx.x) <= 3 &&
                         std::abs (y - m_scene->m_sceneOrbitStartPx.y) <= 3;
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        m_scene->m_sceneOrbiting = false;
        ReleaseCapture();

        // A motionless right-click on a drive or the recorder opens its
        // menu; a drag that began there still turned the scene.
        if (still && StorageDeviceAt (x, y) >= 0)
        {
            ShowStorageContextMenu (StorageDeviceAt (x, y), x, y);
            m_scene->m_sceneOrbitTapMs = 0;
            return DxuiMessageResult::Handled;
        }

        // Two motionless right-clicks in double-click time reset the orbit
        // -- the pose home button, without stealing a key.
        if (still)
        {
            if (nowMs - m_scene->m_sceneOrbitTapMs <= (int64_t) GetDoubleClickTime())
            {
                m_scene->m_sceneView.orbitYawRad   = 0.0f;
                m_scene->m_sceneView.orbitPitchRad = 0.0f;
                m_scene->m_sceneOrbitTapMs         = 0;
                m_scene->InvalidateSceneComposition();
            }
            else
            {
                m_scene->m_sceneOrbitTapMs = nowMs;
            }
        }

        return DxuiMessageResult::Handled;
    }

    if (m_input->IsPaddleCaptured())
    {
        m_input->PushPaddleButton (1, false);
        result = DxuiMessageResult::Handled;
    }
    else if (!m_chrome->m_mainMenu.IsOpen() && !m_input->IsGuestMouseLive())
    {
        // The drive band and the fullscreen strip have no orbit to share the
        // button with: a right-click there is the device's menu.
        int  x      = (int) (short) LOWORD (lParam);
        int  y      = (int) (short) HIWORD (lParam);
        int  device = StorageDeviceAt (x, y);

        if (device >= 0)
        {
            ShowStorageContextMenu (device, x, y);
            result = DxuiMessageResult::Handled;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StorageDeviceAt
//
//  Which storage device is under a client point, as the left-click chain
//  finds it: in the desk scene (or on the fullscreen strip) its drives, the
//  recorder and the recorder's labels; otherwise the flat drive widgets and
//  the flat tape deck. 0 or 1 for a drive, kStorageMenuRecorder for the
//  recorder, -1 for neither.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::StorageDeviceAt (int x, int y) const
{
    POINT  pt = { x, y };



    if (DeskSceneActive())
    {
        bool            inStrip  = m_renderer->GetD3D().IsFullscreen() &&
                                   m_scene->m_stripRectPx.bottom > m_scene->m_stripRectPx.top &&
                                   PtInRect (&m_scene->m_stripRectPx, pt);
        SceneHitResult  sceneHit = inStrip ? m_scene->StripHit (x, y) : m_scene->DeskSceneHit (x, y);

        if (sceneHit.target == SceneHitResult::Target::Drive)
        {
            return sceneHit.driveIndex;
        }

        // The recorder has its own hit test, which checks whether it is on
        // the strip or the desk; the drives' test does not include it.
        if (m_scene->RecorderHit (x, y).target == SceneHitResult::Target::Recorder ||
            PtInRect (&m_scene->m_sceneTapeCounterRect, pt) || PtInRect (&m_scene->m_sceneTapeNameRect, pt))
        {
            return m_tapeDeck->IsTapeRecorderShown() ? kStorageMenuRecorder : -1;
        }

        return -1;
    }

    for (const DriveWidget & drive : m_disks->GetDriveChrome())
    {
        if (drive.IsVisible() && drive.HitTest (x, y) != DriveWidgetRegion::None)
        {
            return drive.GetDrive();
        }
    }

    if (m_tapeDeck->IsTapeRecorderShown() && m_tapeDeck->GetWidget().HitTest (x, y) != TapeDeckRegion::None)
    {
        return kStorageMenuRecorder;
    }

    return -1;
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
    if (altHeld && vk >= 0x20 && vk <= 0x7E && m_chrome->m_mainMenu.HandleAltKey ((wchar_t) vk))
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
        m_input->PasteClipboardText();
    }
    else
    {
        claimed = false;
    }

    return claimed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetKeyRoutingState
//
//  The chrome state a keydown's owner is decided over, gathered in one place
//  so the classifier stays a function of plain values rather than reaching
//  back into the shell.
//
////////////////////////////////////////////////////////////////////////////////

ShellKeyRouting::State EmulatorShell::GetKeyRoutingState() const
{
    ShellKeyRouting::State  state;



    state.pointerMode         = m_input->GetPointerMode();
    state.toolbarOwnsKeyboard = m_chrome->m_toolbar.OwnsKeyboard();
    state.isChromeFocused     = m_chromeFocusIndex != s_kChromeFocusNone;
    state.isMenuOpen          = m_chrome->m_mainMenu.IsOpen();

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchShellKey
//
//  Hands a claimed keydown to whichever part of the chrome owns it. Each arm
//  is the action alone: the test that selected it has already run, and the
//  character has already been claimed by the caller, so nothing here can
//  deliver a key twice by forgetting either.
//
//  Guest is not an arm. Reaching here with it is the caller calling wrongly.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DispatchShellKey (
    ShellKeyOwner  owner,
    WPARAM         vk)
{
    switch (owner)
    {
    case ShellKeyOwner::PaddleExit:
        // The pointer is captured and hidden, so Esc is the only way out and
        // has to work whatever else is up.
        m_input->SetPointerMapping (InputMappingMode::Off);
        break;

    case ShellKeyOwner::Toolbar:
        // An open picker is modal in practice: it owns arrows, Enter and
        // Escape, so browsing the rows previews rather than typing into
        // the //e. A flyout opened by keyboard owns them the same way.
        (void) m_chrome->m_toolbar.HandleKey (vk);
        break;

    case ShellKeyOwner::Chrome:
        // While a menu title / button / drive has keyboard focus, or a menu
        // is open from any source, the ring owns every keydown.
        (void) HandleChromeFocusKey (vk);
        break;

    case ShellKeyOwner::Guest:
        ASSERT (false);
        break;
    }
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
//  Alt+F4 is the one exception, left unclaimed so Windows closes the window.
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
    ShellKeyOwner    owner     = ShellKeyOwner::Guest;
    AppleKeyboard *  keyboard  = lifetime.owns_lock() ? m_machine.GetRefs().keyboard : nullptr;



    // Any key down ends or starts an Alt tap; see OnKeyUp.
    (void) m_chrome->m_mainMenu.TrackAltTap (DxuiKeyEventKind::Down, vk);

    // Alt+F4 belongs to Windows, not to the guest or the chrome. Leaving it
    // unclaimed lets DefWindowProc turn it into SC_CLOSE; claiming it, as
    // every other keydown is, left the main window unable to close that way.
    if (vk == VK_F4 && (lParam & s_kAltContextLParamBit) != 0)
    {
        return DxuiMessageResult::NotHandled;
    }

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

    // A Caps Lock press here hands the emulated key over to the host's, the
    // first time, whatever else holds the keyboard. The host has already
    // toggled by the time its key-down is read, so GetKeyState reports the
    // state the press left. A held key repeats without toggling, so only a
    // fresh press counts. Caps Lock is not part of the //e keyboard matrix, so
    // the guest never sees the press.
    if (vk == VK_CAPITAL)
    {
        if (!isRepeat && m_input->GetCapsLock().OnCapsLockPressed ((GetKeyState (VK_CAPITAL) & 1) != 0))
        {
            m_chrome->ShowNotice (CapsLockTracker::kpszNowFollowingHostNotice);
        }

        BAIL_OUT_IF (true, S_OK);
    }

    // 0. Esc exits paddle mode: releases the mouse capture (cursor
    //    reappears) and returns the input mapping to Off, matching the
    //    "Esc to exit" hint on the widget.
    // Who owns this keydown is decided ONCE, here, and carried forward. The
    // character Windows manufactures from it cannot be classified on its own
    // -- it carries a character, not a key -- and re-asking this question when
    // it arrives gives the wrong answer whenever the key CHANGED the state,
    // which Escape over an open picker does by closing the picker.
    owner = m_keyOwnerFn (GetKeyRoutingState(), vk);

    if (owner != ShellKeyOwner::Guest)
    {
        // The single site that arms the swallow. It follows from the key not
        // being the guest's rather than from each arm remembering, so a claim
        // added later cannot deliver its character by forgetting.
        m_swallowMetaChar = ShellKeyRouting::DoesOwnerSwallowChar (owner);

        DispatchShellKey (owner, vk);
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

    // A tap of Alt on its own toggles the menu's access-key underlines, and a
    // stopped machine draws no frame to show them, so the window repaints.
    // The release still reaches the guest below, since Alt is also Open Apple.
    if (m_chrome->m_mainMenu.TrackAltTap (DxuiKeyEventKind::Up, vk))
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }

    // Caps Lock is not part of the //e keyboard matrix, and OnKeyDown kept
    // its press from the guest. Its release must not reach the guest either,
    // or it would end the auto-repeat of a letter still held down.
    if (vk == VK_CAPITAL)
    {
        return DxuiMessageResult::Handled;
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
//  OnChar
//
//  Decides whether a synthesized WM_CHAR belongs to the guest, and drops it
//  otherwise. This handler is almost entirely suppression: Windows manufactures
//  a WM_CHAR from a WM_KEYDOWN whether or not anything consumed the keydown,
//  so a key the shell claimed arrives a second time as a character.
//
//  It does NOT decide who owns the keystroke. OnKeyDown did that once, through
//  ShellKeyRouting, and armed m_swallowMetaChar; this reads that answer. The two
//  used to ask separately and drifted, which is how Esc leaving paddle mode
//  ended up claimed as a key and delivered as a character.
//
//  Three things are swallowed:
//
//    a claimed key   whatever OnKeyDown took for the chrome -- an open picker,
//                    an open menu, the focus ring, Esc leaving paddle mode, a
//                    host-meta shortcut -- would otherwise ALSO drop into the
//                    //e latch as its character
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



    // Nothing here re-asks who owns the keyboard. OnKeyDown decided that for
    // this press and armed m_swallowMetaChar above, which the bail at the top
    // has already consumed. Re-asking would be wrong as well as duplicated:
    // the claimed key often CHANGES the state it would be re-tested against,
    // so Escape over an open picker closes the picker and the recomputed
    // answer says nobody owns the keyboard.

    // In joystick mode the X / Z keys are fire buttons (handled in OnKeyDown
    // / OnKeyUp), so swallow their WM_CHAR to keep the letters from also
    // typing into the //e keyboard latch -- mirroring how arrow keys are
    // withheld from the latch.
    bool  isFireKey = m_input->IsArrowsJoystick() &&
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





////////////////////////////////////////////////////////////////////////////////
//
//  OnViewportKey
//
//  The viewport's keys, once the chrome has passed on them, are the guest's.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnViewportKey (const DxuiKeyEvent & ev)
{
    return m_input->HandleViewportKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnViewportMouse
//
//  The viewport's mouse, likewise.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::OnViewportMouse (const DxuiMouseEvent & ev)
{
    return m_input->HandleViewportMouse (ev);
}
