#include "Pch.h"

#include "Shell/Components/ShellStandInBar.h"

#include "Shell/Components/ShellInput.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Ui/ThemeManager.h"
#include "Config/UserConfigStore.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellSerialCard.h"
#include "Shell/DiskManager.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/Components/ShellAudio.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
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
#include "Shell/Layout/ChromeBandLayout.h"
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
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)"
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Ui/ThemeManager.h"
#include "Shell/DiskManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellStandInBar::SyncStandInBanner
//
//  A persistent way OUT, for as long as the pointer is held.
//
//  Paddle mode takes the mouse: the cursor is hidden and clipped to the
//  window, and the only release is a key the user has to already know. The
//  joystick button says so -- and fullscreen hides the joystick button, which
//  leaves a captured pointer, no cursor, and nothing on screen to read. The
//  notice goes away the moment the capture does.
//
//  A MESSAGE BAR IN THE CHROME, NOT A CAPTION ON THE PICTURE. It was shadowed
//  text laid over the viewport, which reads as a caption only where there is
//  something photographic under it -- and even on the desk scene the only
//  places left to put it were on the drives, on their name strips, or in the
//  middle of the CRT. The bar says the same words in the one place that is
//  nobody else's: the chrome under the command strip, the same in every theme
//  and in fullscreen.
//
////////////////////////////////////////////////////////////////////////////////

void ShellStandInBar::SyncStandInBanner()
{
    RECT   client = {};
    RECT   rc     = {};
    RECT   strip  = {};
    LONG   top    = 0;
    float  width  = 0.0f;



    m_standInBar.SetText (m_shell.m_input->GetStandInBannerText());

    if (m_standInBar.GetText().empty() || m_shell.m_hwnd == nullptr || !GetClientRect (m_shell.m_hwnd, &client))
    {
        m_standInBar.SetVisible        (false);
        m_standInBarSurface.SetVisible (false);

        //  A BAND LEFT BEHIND, NOTED FOR THE NEXT FRAME. The height is claimed
        //  and released where the capture starts and ends, but the capture is
        //  also dropped from OnCancelMode / OnKillFocus, which can fire from
        //  INSIDE a layout pass -- and a re-dock asked for from in there is
        //  refused, because the pass would be calling itself. Without this the
        //  picture would stay short, under an empty strip, until some
        //  unrelated resize came along.
        //
        //  FLAGGED, NOT DONE HERE: this runs inside the frame the re-dock
        //  would repaint, and a layout pass drives a synchronous WM_PAINT.
        //  TryPresentUiFrame acts on it at the top of the next frame, where
        //  the change band's own expiry already re-docks from.
        if (m_shell.m_hwnd != nullptr && !m_shell.m_renderer->GetD3D().IsFullscreen()
            && m_chrome.m_standInBand.GetBounds().bottom > m_chrome.m_standInBand.GetBounds().top)
        {
            m_standInBandStale = true;
        }

        return;
    }

    //  THE GRAB, PUT BACK IF THE OS TOOK IT WITHOUT SAYING SO. Between the
    //  band docking and any cancel the window manager decides to send, the
    //  capture can go while everything the shell knows says it is still held.
    //  OnCancelMode covers the cancel it can identify; this covers the rest,
    //  by simply asking.
    //
    //  ONLY WHILE THE PADDLE HOLDS THE POINTER. This belongs to the capture,
    //  not to the bar: the two were the same condition when the bar existed
    //  only during a capture, and they stopped being the same when the bar
    //  took on the arrow keys. Without the test it grabs the pointer and
    //  clips the cursor to the client in keys mode, where nothing asked for
    //  the mouse at all.
    if (m_shell.m_input->IsPaddleCaptured() && GetCapture() != m_shell.m_hwnd && GetForegroundWindow() == m_shell.m_hwnd)
    {
        SetCapture (m_shell.m_hwnd);
        m_shell.m_input->ClipPaddleCursorToClient();
    }

    //  A BAND THAT HAS NOT BEEN CLAIMED YET, ASKED FOR. The mirror of the
    //  release above, and the case that only exists now that the bar is not
    //  the capture's alone: entering a capture re-docked on its way in, so
    //  the band was always there by the time this ran. The arrow keys turn
    //  the bar on without any such moment -- restoring the saved input mode
    //  at startup is the ordinary one -- and a bar laid into a band of no
    //  height paints its border and its badge with the text clipped away,
    //  which is a line across the chrome with a lone glyph sitting on it.
    //
    //  FLAGGED, NOT DONE HERE, for the same reason the release is: this runs
    //  inside the frame a re-dock would repaint.
    if (!m_shell.m_renderer->GetD3D().IsFullscreen()
        && m_chrome.m_standInBand.GetBounds().bottom <= m_chrome.m_standInBand.GetBounds().top)
    {
        m_standInBandStale = true;
    }

    //  A DOCKED BAND WHEN THERE ARE BANDS: the dock gives it the client width
    //  under the command strip and the picture gives up the height, the same
    //  bargain the external-change notice makes. Taking the band's rect rather
    //  than computing a second one is what keeps the bar and the picture from
    //  disagreeing about where the chrome ends.
    //
    //  IN FULLSCREEN THERE ARE NO BANDS -- the picture owns the whole client --
    //  so the bar hangs off the top edge instead, following the toolbar's
    //  reveal down and back up so it stays under the command strip wherever
    //  that strip currently is.
    if (!m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        rc = m_chrome.m_standInBand.GetBounds();
    }
    else
    {
        strip = m_chrome.m_toolbar.GetBounds();
        top   = (m_chrome.m_toolbar.IsVisible() && strip.bottom > client.top) ? strip.bottom
                                                                     : client.top;
        width = (float) (client.right - client.left);

        rc.left   = client.left;
        rc.right  = client.right;
        rc.top    = top;
        rc.bottom = top + (LONG) m_standInBar.GetPreferredHeightPx (width, m_shell.m_scaler);
    }

    //  A RECT THAT HAS NOT BEEN LAID OUT YET IS SKIPPED, and nothing else is.
    //  A band carries its thickness on the docked axis alone and is given its
    //  width by the dock pass, so between a resize and that pass its rect is
    //  a slab of NO WIDTH -- and a text box no wider than a glyph wraps every
    //  character onto a line of its own, which is the bar's sentence running
    //  down the edge of the window, one letter at a time.
    //
    //  NARROW IS NOT A REASON TO DISAPPEAR. A narrow bar is a taller bar: the
    //  text wraps and the band grows to hold it, which is what the shared
    //  measurement above is for. The only rect refused here is one with no
    //  width at all, which is not a width the bar has to cope with -- it is
    //  the dock not having run.
    if (rc.right - rc.left <= 0 || rc.bottom - rc.top <= 0)
    {
        m_standInBar.SetVisible        (false);
        m_standInBarSurface.SetVisible (false);

        //  Ask for the dock the rect is waiting on, the same way the band's
        //  own absence is asked for above.
        if (m_shell.m_hwnd != nullptr && !m_shell.m_renderer->GetD3D().IsFullscreen())
        {
            m_standInBandStale = true;
        }

        return;
    }

    //  The band is short of what the text needs at this width: re-dock and
    //  paint at the height there is, rather than showing nothing. A band
    //  reserved from the same measurement cannot be short, so this is the
    //  frame after a width change and no more than that.
    if (rc.bottom - rc.top < GetStandInBarHeightPx ((float) (rc.right - rc.left))
        && m_shell.m_hwnd != nullptr && !m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        m_standInBandStale = true;
    }

    m_standInBarSurface.Layout     (rc, m_shell.m_scaler);
    m_standInBarSurface.SetVisible (true);

    m_standInBar.SetDpi     (m_shell.m_scaler.GetDpi());
    m_standInBar.Layout     (rc, m_shell.m_scaler);
    m_standInBar.SetVisible (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellStandInBar::GetCaptureBandThicknessPx
//
//  How tall the input-mode bar's band is.
//
//  ZERO WHILE A CONTROLLER OR NOTHING DRIVES THE GAME PORT, so every session
//  that never falls back to the keys or the mouse is laid out as before.
//
//  MEASURED FROM A BANNER OF ITS OWN rather than the docked one. This is a
//  const query and the docked bar is given its text by the layout pass; a
//  height taken before that pass would measure whatever the last mode left
//  behind. The two share the text and the DPI, so they measure alike.
//
//  ZERO IN FULLSCREEN TOO, where there are no bands at all: the picture owns
//  the whole client and the bar hangs off the top edge under the toolbar
//  reveal instead.
//
////////////////////////////////////////////////////////////////////////////////

int ShellStandInBar::GetCaptureBandThicknessPx (int clientWidthPx) const
{
    if (m_shell.m_renderer->GetD3D().IsFullscreen() || clientWidthPx <= 0)
    {
        return 0;
    }

    return GetStandInBarHeightPx ((float) clientWidthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellStandInBar::GetStandInBarHeightPx
//
//  How tall the input-mode bar is at a given width, and THE ONLY PLACE THAT
//  ANSWERS IT. The band reserves the height and the paint checks it, and the
//  two asking separately is how the bar came to vanish at particular widths:
//  the reserve measured, the check estimated, and GetPreferredHeightPx is
//  deliberately generous -- an average glyph width, rounded up so text never
//  clips. Wherever the estimate wanted a line the measurement did not, the
//  check called a perfectly good band too short.
//
//  MEASURED WHEN THERE IS A RENDERER TO ASK, because the bar centers its text
//  and a centered banner picks its line width from that measurement. The
//  estimate is the fallback for the moments before the renderer exists.
//
//  Zero only when there is no bar. A narrow bar is a TALLER bar -- the text
//  wraps and the band grows -- never an absent one.
//
////////////////////////////////////////////////////////////////////////////////

int ShellStandInBar::GetStandInBarHeightPx (float widthPx) const
{
    IDxuiTextRenderer *  text = (m_shell.m_host != nullptr) ? m_shell.m_host->GetTextRenderer() : nullptr;
    std::wstring         line = m_shell.m_input->GetStandInBannerText();
    DxuiInfoBanner       measure (line);



    if (line.empty() || widthPx <= 0.0f)
    {
        return 0;
    }

    measure.SetCentered (true);
    measure.SetDpi      (m_shell.m_scaler.GetDpi());

    if (text != nullptr)
    {
        return (int) measure.GetMeasuredHeightPx (*text, widthPx, m_shell.m_scaler);
    }

    return (int) measure.GetPreferredHeightPx (widthPx, m_shell.m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellStandInBar::TakeStandInBandStale
//
//  True, once, when SyncStandInBanner found a capture band standing with no
//  capture behind it. The frame loop docks again at the top of the next frame,
//  because docking again repaints and nothing has been composed yet.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellStandInBar::TakeStandInBandStale()
{
    bool  isStale = m_standInBandStale;



    m_standInBandStale = false;

    return isStale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellStandInBar::TakeCaptureReflowEcho
//
//  True for a cancel within s_kCaptureReflowEchoMs of the capture bar's dock,
//  which then clears, so exactly one cancel is ignored.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellStandInBar::TakeCaptureReflowEcho (int64_t nowMs)
{
    bool  isEcho = m_captureReflowMs != 0 && nowMs - m_captureReflowMs <= ShellChrome::s_kCaptureReflowEchoMs;



    if (isEcho)
    {
        m_captureReflowMs = 0;
    }

    return isEcho;
}
