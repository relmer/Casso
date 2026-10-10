#include "Pch.h"

#include "Shell/Components/ShellChrome.h"
#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Ui/ThemeManager.h"
#include "Config/UserConfigStore.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/DiskManager.h"
#include "Shell/Components/ShellTapeDeck.h"
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
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome
//
////////////////////////////////////////////////////////////////////////////////

ShellChrome::ShellChrome (EmulatorShell & shell)
    : m_shell (shell)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellChrome
//
////////////////////////////////////////////////////////////////////////////////

ShellChrome::~ShellChrome() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  Window placement helpers
//
//  Geometry for the bottom command bar's occupants -- the drive widgets and
//  the joystick-mode button -- plus the window-size reconciliation that runs
//  once the frame has materialized.
//
//  Desk-scene zoom is applied by folding the scene scale into the EFFECTIVE
//  DPI rather than by scaling rects afterwards. Widget geometry, fonts, and
//  the inter-widget gaps then zoom together for free, because every one of
//  them already derives from DPI.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::LayoutDriveWidgetsInCommandBar (
    std::array<DriveWidget, 2>  & driveChrome,
    int                           bottomInsetPx,
    int                           clientW,
    int                           clientH,
    UINT                          dpi,
    float                         sceneScale,
    int                           visibleCount)
{
    int            bottomInset   = 0;
    int            commandBarTop = 0;
    int            gap           = 0;
    int            bottomGap     = 0;
    RECT           probe         = {};
    int            widgetW       = 0;
    int            widgetH       = 0;
    int            x             = 0;
    int            y             = 0;
    size_t         i             = 0;
    DxuiDpiScaler  scaler;
    RECT           anchor        = {};
    bool           showTape      = false;
    int            tapeW         = 0;
    int            tapeH         = 0;
    int            rowH          = 0;



    // Desk-scene zoom: the drives scale with the monitor. Fold the scale
    // into the effective DPI so widget geometry, fonts, and the inter-
    // widget gaps all zoom together.
    dpi = (UINT) lroundf ((float) dpi * sceneScale);

    bottomInset = bottomInsetPx;
    commandBarTop = std::max (0, clientH - bottomInset);
    gap = MulDiv (s_kCompactDriveWidgetGapDp, static_cast<int> (dpi), s_kBaseDpi);



    scaler.SetDpi (dpi);
    anchor = { 0, 0, 0, 0 };
    driveChrome[0].Layout (anchor, scaler);
    probe   = driveChrome[0].GetOuterRect();
    widgetW = probe.right  - probe.left;
    widgetH = probe.bottom - probe.top;
    // Centered on the drives that will actually SHOW, not on the array. A //c
    // with its external drive unconnected lays out both and hides the second
    // right after this, so centering on two left the single visible drive
    // sitting left of center by half a widget and a gap.
    visibleCount = std::clamp (visibleCount, 1, static_cast<int> (driveChrome.size()));
    x            = DriveRowLayout::ComputeRowOriginX (clientW, widgetW, gap, visibleCount);

    // The recorder is added to the row, to the right of the drives, and the
    // row is centered as one unit with it. Measured at this DPI like the drives.
    showTape = m_shell.m_tapeDeck->IsTapeRecorderShown();

    if (showTape)
    {
        RECT  tapeProbe = {};

        m_shell.m_tapeDeck->GetWidget().Layout (RECT {}, scaler);
        tapeProbe = m_shell.m_tapeDeck->GetWidget().GetOuterRect();
        tapeW     = tapeProbe.right  - tapeProbe.left;
        tapeH     = tapeProbe.bottom - tapeProbe.top;
        x         = std::max (0, x - (gap + tapeW) / 2);
    }

    // A LONE drive centers on the part that carries the weight -- the disk
    // name and its head bar -- not on the whole widget. The 2D widget hangs
    // its "DRIVE 1" caption off to the left, so centering the outer box put
    // the name and bar half a caption column right of center and the row
    // looked hung off to one side.
    //
    // The offset is measured off the widget rather than assumed, so it follows
    // the caption column's width. Two drives keep centering on the pair: the
    // caption then reads as part of a repeating unit rather than as a tail on
    // a single object.
    if (!showTape)
    {
        int  captionLead = driveChrome[0].GetBodyRect().left - probe.left;

        x = DriveRowLayout::ApplyLoneDriveCaptionOffset (x, captionLead, visibleCount);
    }

    // Anchor the widget to the bottom so the margin between the
    // basename label and the window edge mirrors the gap between
    // the drive body and the label (s_kLabelStripGapPx, scaled).
    //
    // The row is as tall as its tallest member, and every member hangs from
    // its top, so the drives line up with the recorder rather than with the
    // band's bottom edge.
    bottomGap = MulDiv (s_kLabelBottomGapDp, static_cast<int> (dpi), s_kBaseDpi);
    rowH      = std::max (widgetH, tapeH);
    y         = std::max (commandBarTop, clientH - rowH - bottomGap);

    for (i = 0; i < driveChrome.size(); i++)
    {
        int   widgetX      = DriveRowLayout::ComputeWidgetX (x, static_cast<int> (i), widgetW, gap);
        RECT  widgetAnchor = { widgetX, y, widgetX, y };

        // Visible again: the desk scene turns these off rather than just
        // collapsing them, and this is the one path that brings the flat
        // widgets back, so it is where they earn their visibility.
        driveChrome[i].SetVisible (true);
        driveChrome[i].Layout (widgetAnchor, scaler);
    }

    // SyncTapeChrome lays the recorder out every frame from this anchor.
    if (showTape)
    {
        int  tapeX = DriveRowLayout::ComputeWidgetX (x, visibleCount, widgetW, gap);

        m_shell.m_tapeDeck->SetAnchor ({ tapeX, y, tapeX, y }, dpi);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SetChromeHiddenForFullscreenScene
//
//  Visibility, not bounds: the adopted chrome controls paint from their own
//  cached layouts, so an empty rect is not a reliable hidden state --
//  SetVisible is. The host caption gets its explicit switch. Symmetric: the
//  windowed layout path calls this with `hidden = false` every pass, so
//  leaving fullscreen restores everything.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SetChromeHiddenForFullscreenScene (bool hidden)
{
    // A borderless-fullscreen window fills the monitor and has nothing to
    // resize TO: its edges ARE the screen's, and leaving the resize borders
    // armed lets a drag at a corner pull the picture down to a fraction of
    // the screen with no caption left to put it right with.
    m_shell.m_host->SetResizable (!hidden);

    m_shell.m_host->SetCaptionVisible (!hidden);
    m_mainMenu.SetVisible (!hidden);
    // The menu bar and toolbar come back on their own in fullscreen, summoned
    // by the top edge -- so hiding the chrome parks them and
    // TickFullscreenTopChrome owns them from there.
    m_toolbar.SetVisible (!hidden);

    if (hidden)
    {
        m_fsTopChromeShown  = false;
        m_fsTopChromeLeftMs = 0;
    }

    // The band surface only exists for the 2D chrome; under the desk scene
    // the drives paint from the scene and the band would read as a leftover
    // bar along the window's bottom edge.
    m_driveBandSurface.SetVisible (!hidden && !m_shell.DeskSceneActive());
    m_switchBar.SetVisible (!hidden);

    // Leaving fullscreen must not hand the flat widgets back to a scene that
    // has already retired them.
    m_shell.m_disks->GetDriveChrome()[0].SetVisible (!hidden && !m_shell.DeskSceneActive());
    m_shell.m_disks->GetDriveChrome()[1].SetVisible (!hidden && !m_shell.DeskSceneActive());

    if (hidden)
    {
        m_shell.m_disks->GetDriveChrome()[0].Hide();
        m_shell.m_disks->GetDriveChrome()[1].Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SyncStandInBanner
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

void ShellChrome::SyncStandInBanner()
{
    RECT   client = {};
    RECT   rc     = {};
    RECT   strip  = {};
    LONG   top    = 0;
    float  width  = 0.0f;



    m_standInBar.SetText (m_shell.GetStandInBannerText());

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
            && m_standInBand.GetBounds().bottom > m_standInBand.GetBounds().top)
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
    if (m_shell.m_paddleCaptured && GetCapture() != m_shell.m_hwnd && GetForegroundWindow() == m_shell.m_hwnd)
    {
        SetCapture (m_shell.m_hwnd);
        m_shell.ClipPaddleCursorToClient();
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
        && m_standInBand.GetBounds().bottom <= m_standInBand.GetBounds().top)
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
        rc = m_standInBand.GetBounds();
    }
    else
    {
        strip = m_toolbar.GetBounds();
        top   = (m_toolbar.IsVisible() && strip.bottom > client.top) ? strip.bottom
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
//  ShellChrome::SyncFrameRateReadout
//
//  The frame rate over the picture, in the top-left corner.
//
//  COUNTED AT THE PRESENT, not here: DxuiHwndSource ticks its counter when
//  a frame actually reaches the screen, so a paint the shell skipped is not
//  a dropped frame and a present that waited on vsync reports the interval
//  the user saw. This only reads the figure and places it.
//
//  One decimal, because whether the scene holds sixty is the question and a
//  rounded integer answers it ambiguously at the boundary.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SyncFrameRateReadout()
{
    RECT     client   = {};
    RECT     rc       = {};
    wchar_t  text[32] = {};



    if (!m_shell.m_settings->GetPrefs().showFrameRate || m_shell.m_host == nullptr
        || m_shell.m_hwnd == nullptr || !GetClientRect (m_shell.m_hwnd, &client))
    {
        m_fpsReadout.SetVisible (false);
        return;
    }

    // ABOVE THE BOTTOM CHROME, not on it: hung from the client edge the
    // readout straddles the switch bar, half over the scene and half over a
    // shell band, reading as neither.
    //
    // MEASURED OFF THE CHROME, NEVER OFF THE SCENE. Anchored to the toolbar
    // band this drifted up and down while the scene was being orbited: the
    // bands report different bounds as the composition changes under them,
    // so a readout hung off one wanders with the thing it is measuring. The
    // switch bar does not move, and the client edge behind it does not
    // either.
    {
        RECT  bar    = m_switchBand.GetBounds();
        LONG  bottom = (!m_shell.m_renderer->GetD3D().IsFullscreen() && bar.bottom > bar.top)
                     ? bar.top : client.bottom;

        rc.left   = client.left + m_shell.m_scaler.ToPx (s_kFrameRateInsetDp);
        rc.right  = rc.left + m_shell.m_scaler.ToPx (s_kFrameRateWidthDp);
        rc.bottom = bottom - m_shell.m_scaler.ToPx (s_kFrameRateInsetDp);
        rc.top    = rc.bottom - m_shell.m_scaler.ToPx (s_kFrameRateHeightDp);
    }

    swprintf_s (text, L"%.1f fps", m_shell.m_host->GetFramesPerSecond());

    m_fpsReadout.SetText        (text);
    m_fpsReadout.SetFontSizeDip (DxuiShadowedText::kFontDip);
    m_fpsReadout.SetAlign       (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_fpsReadout.SetDpi         (m_shell.m_scaler.GetDpi());
    m_fpsReadout.Layout         (rc, m_shell.m_scaler);
    m_fpsReadout.SetVisible     (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MirroredSlideStart
//
//  A slide start time that PRESERVES the current position when the direction
//  reverses: a band caught half-way out and sent back should leave from where
//  it is, not jump to the far end and crawl. The elapsed time is mirrored
//  about the animation length, which is the same trick the drive strip's FSM
//  plays on itself.
//
////////////////////////////////////////////////////////////////////////////////

static int64_t MirroredSlideStart (int64_t nowMs, int64_t animStartMs)
{
    int64_t  elapsed = nowMs - animStartMs;



    if (elapsed >= FullscreenStripState::kSlideMs)
    {
        return nowMs;
    }

    return nowMs - (FullscreenStripState::kSlideMs - elapsed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::TickFullscreenTopChrome
//
//  The menu bar and the command toolbar on the same bargain the drive strip
//  has at the bottom: the pointer at the top edge slides them down, leaving
//  slides them away. They travel as one band, menu above toolbar, in the
//  order the windowed chrome stacks them.
//
//  Laid out here rather than by the chrome dock, because in fullscreen there
//  are no bands -- the scene owns the whole client, and this is an overlay
//  across its top rather than a strip the viewport makes room for.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::TickFullscreenTopChrome()
{
    RECT     client    = {};
    POINT    cursor    = {};
    int      menuH     = 0;
    int      toolbarH  = 0;
    int      bandH     = 0;
    bool     want      = false;
    int64_t  nowMs     = 0;



    if (!m_shell.m_renderer->GetD3D().IsFullscreen() || m_shell.m_hwnd == nullptr || !GetClientRect (m_shell.m_hwnd, &client))
    {
        if (m_fsTopChromeShown)
        {
            m_fsTopChromeShown = false;
            m_mainMenu.SetVisible (false);
            m_toolbar.SetVisible  (false);
        }

        return;
    }

    nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                std::chrono::steady_clock::now().time_since_epoch()).count();

    m_toolbar.PlanForWidth (client.right - client.left, m_shell.m_scaler);
    menuH    = DxuiMenuBar::GetStripHeightPx (m_shell.m_scaler.GetDpi());
    toolbarH = m_shell.m_scaler.ToPx (m_toolbar.GetBandDp());
    bandH    = menuH + toolbarH;

    if (GetCursorPos (&cursor) && ScreenToClient (m_shell.m_hwnd, &cursor) && PtInRect (&client, cursor))
    {
        // The edge zone summons; the whole band holds it open, so the
        // pointer can travel down onto the buttons without dismissing them.
        want = m_fsTopChromeShown ? (cursor.y <= bandH)
                                  : (cursor.y <= m_shell.m_scaler.ToPx (s_kStripEdgeZoneDp));
    }

    // An open menu keeps the band up regardless of where the pointer
    // wandered to reach the dropdown's items, which hang below it.
    want = want || m_mainMenu.IsOpen();

    if (want)
    {
        m_fsTopChromeLeftMs = 0;
    }
    else if (m_fsTopChromeShown && m_fsTopChromeLeftMs == 0)
    {
        m_fsTopChromeLeftMs = nowMs;
    }

    if (!want && m_fsTopChromeShown &&
        nowMs - m_fsTopChromeLeftMs >= FullscreenStripState::kAutoHideGraceMs)
    {
        m_fsTopChromeShown  = false;
        m_fsTopChromeAnimMs = MirroredSlideStart (nowMs, m_fsTopChromeAnimMs);
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }
    else if (want && !m_fsTopChromeShown)
    {
        m_fsTopChromeShown  = true;
        m_fsTopChromeAnimMs = MirroredSlideStart (nowMs, m_fsTopChromeAnimMs);
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }

    // The band SLIDES: it hangs off the top by the part of itself that has
    // not arrived, so it enters and leaves the way the drive strip does
    // rather than blinking into place. Reversing mid-slide keeps the current
    // position (see MirroredSlideStart) instead of snapping to the far end.
    {
        float  t        = std::clamp ((float) (nowMs - m_fsTopChromeAnimMs) /
                                      (float) FullscreenStripState::kSlideMs, 0.0f, 1.0f);
        float  progress = m_fsTopChromeShown ? t : 1.0f - t;
        int    top      = client.top - (int) ((1.0f - progress) * (float) bandH);

        if (progress <= 0.0f)
        {
            m_mainMenu.SetVisible (false);
            m_toolbar.SetVisible  (false);
            return;
        }

        m_mainMenu.Layout     (RECT{ client.left, top, client.right, top + menuH }, m_shell.m_scaler);
        m_mainMenu.SetVisible (true);

        m_toolbar.Layout     (RECT{ client.left, top + menuH, client.right, top + bandH }, m_shell.m_scaler);
        m_toolbar.SetVisible (true);

        if (t < 1.0f)
        {
            m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshToolbarThemeList
//
//  Fills the toolbar's theme picker from the discovered catalog and keeps the
//  parallel id vector that turns a picked row back into a theme id.
//
//  The rows are REPLACED only when the catalog itself changed. This runs from
//  the theme-change listener, which also fires for every live preview -- and
//  replacing the rows resets the selection, which would destroy the row the
//  picker has to snap back to when the list is dismissed. An unchanged
//  catalog therefore only moves the selection, which the toolbar in turn
//  drops while its list is open.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::RefreshToolbarThemeList()
{
    std::vector<std::wstring>  displayNames;
    std::vector<std::string>   ids;
    std::string                activeName;
    int                        activeIndex = -1;
    int                        row         = 0;



    if (m_shell.m_settings->GetThemeManager() == nullptr)
    {
        return;
    }

    activeName = m_shell.m_settings->GetThemeManager()->GetActiveThemeName();

    for (const LoadedTheme & theme : m_shell.m_settings->GetThemeManager()->GetAvailableThemes())
    {
        if (theme.name == activeName)
        {
            activeIndex = row;
        }

        ids.push_back (theme.name);
        displayNames.emplace_back (theme.name.begin(), theme.name.end());
        row++;
    }

    // An OPEN picker is mid-preview and owns the index, so a sync from here is
    // dropped rather than fighting the highlight the user is moving -- the
    // preview itself arrives back here as a sync.
    if (ids != m_toolbarThemeIds)
    {
        m_toolbarThemeIds = std::move (ids);
        m_mainMenu.GetCommands().SetThemeNames (displayNames);
        m_mainMenu.GetCommands().SetThemeIndex (activeIndex);
        m_toolbar.SetDropDownItems (EmulatorCommands::kIdTheme, m_mainMenu.GetCommands().GetThemeItems());
    }
    else if (activeIndex >= 0 && !m_toolbar.IsMenuOpen())
    {
        m_mainMenu.GetCommands().SetThemeIndex (activeIndex);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncToolbarState
//
//  Pushes the state the toolbar mirrors rather than owns: which machine the
//  Reset / Power tips talk about, which way the fullscreen button points, and
//  where the two pickers sit. The pickers can be moved from the menu, from
//  Settings and from a machine switch, so the sync runs every UI frame; the
//  toolbar drops it while a list is open, since an open list is mid-preview
//  and owns its own value until it closes.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SyncToolbarState()
{
    ColorMode  mode       = m_shell.m_renderer->GetColorMode();
    RECT       client     = {};
    int        colorIndex = 0;
    int        themeIndex = -1;
    int        row        = 0;



    switch (mode)
    {
        case ColorMode::GreenMono: colorIndex = 1; break;
        case ColorMode::AmberMono: colorIndex = 2; break;
        case ColorMode::WhiteMono: colorIndex = 3; break;
        default:                   colorIndex = 0; break;
    }

    if (m_shell.m_settings->GetThemeManager() != nullptr)
    {
        const std::string &  activeName = m_shell.m_settings->GetThemeManager()->GetActiveThemeName();

        for (const std::string & id : m_toolbarThemeIds)
        {
            if (id == activeName)
            {
                themeIndex = row;
            }

            row++;
        }
    }

    if (m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &client))
    {
        m_toolbar.SetHostClientRect (client);
    }

    m_mainMenu.GetCommands().SetMachineDisplayName (std::wstring (m_shell.m_machine.GetConfig().name.begin(), m_shell.m_machine.GetConfig().name.end()));
    m_switchBar.SetMachineDisplayName (std::wstring (m_shell.m_machine.GetConfig().name.begin(), m_shell.m_machine.GetConfig().name.end()));
    m_mainMenu.GetCommands().SetFullscreen (m_shell.m_renderer->GetD3D().IsFullscreen());

    // An OPEN picker is mid-preview and owns its index; see RefreshToolbarThemeList.
    if (!m_toolbar.IsMenuOpen())
    {
        m_mainMenu.GetCommands().SetMonitorColorIndex (colorIndex);

        if (themeIndex >= 0)
        {
            m_mainMenu.GetCommands().SetThemeIndex (themeIndex);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateViewportLayout
//
//  Computes the Apple ][ viewport rectangle from the current client
//  width / height via the chrome-band DxuiDockLayout (top + bottom
//  insets), then invokes DxuiViewport::Layout on the host root panel's
//  viewport child. The viewport's bounds-changed callback fires when
//  the rectangle differs from the last value reported, forwarding
//  the new rect to D3DRenderer::SetTargetBounds via
//  OnViewportBoundsChanged.
//
//  Skipped silently when the viewport has not yet been wired (early
//  init paths, or when the host root panel was torn down).
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::UpdateViewportLayout (int widthPx, int heightPx)
{
    HRESULT  hr           = S_OK;
    RECT     center       = {};
    RECT     viewportRect = {};



    BAIL_OUT_IF (m_shell.m_viewport == nullptr, S_OK);

    // 3D desk scene (spec 018): the composition is computed for the center
    // rect (drives included -- they are scene objects now), the viewport
    // (the CRT target) becomes the projected glass rect, and the bottom band
    // collapses to the joystick row via SyncChromeBands' scene branch. The
    // settle loop is retained for the band's dock feedback.
    if (m_shell.CrtMonitorActive() && m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        // Fullscreen presentation (FR-014): the glass fills the monitor with
        // a straight-on camera, every chrome band hidden -- the whole client
        // is the scene. The drive overlay strip presents the drives.
        HRESULT               hrLayout = S_OK;
        DeskSceneComposition  comp;
        RECT                  full     = { 0, 0, widthPx, heightPx };

        hrLayout = DeskSceneLayout::ComputeGlassFill (full, m_shell.m_scaler.GetDpi(),
                                                      kFramebufferWidth, kFramebufferHeight,
                                                      m_shell.m_scene->GetScene().Metrics(), comp);
        BAIL_OUT_IF (hrLayout != S_OK, S_OK);

        m_shell.m_scene->GetScene().SetComposition (comp);
        m_chromeSceneScale = comp.sceneScale * s_kDeskDriveScale;
        viewportRect       = full;

        m_shell.m_scene->SyncSceneDriveChrome();
    }
    else if (m_shell.CrtMonitorActive())
    {
        // The basename strip under the drive row is chrome, not scene, so the
        // composition is solved into a center rect short by its height and
        // the labels hang in what is left.
        int  labelStripPx = m_shell.m_scaler.ToPx (s_kSceneDriveLabelStripDp + s_kSceneDriveLabelGapDp);

        for (int pass = 0; pass < s_kSceneScaleSettlePasses; pass++)
        {
            HRESULT               hrLayout = S_OK;
            DeskSceneComposition  comp;
            RECT                  sceneBox = {};

            center            = ComputeViewportRect (widthPx, heightPx);
            sceneBox          = center;
            sceneBox.bottom   = std::max (center.top, center.bottom - labelStripPx);

            hrLayout = DeskSceneLayout::Compute (sceneBox, m_shell.m_scaler.GetDpi(), m_shell.m_scene->DeskSceneDriveCount(),
                                                 m_shell.m_scene->GetScene().Metrics(), comp,
                                                 m_shell.m_scaler.ToPx (s_kSceneDriveGapDp + s_kStripEdgeZoneDp),
                                                 m_shell.m_scene->GetView());
            BAIL_OUT_IF (hrLayout != S_OK, S_OK);

            m_shell.m_scene->GetScene().SetComposition (comp);
            m_chromeSceneScale = comp.sceneScale * s_kDeskDriveScale;

        }

        viewportRect = m_shell.m_scene->GetScene().Composition().glassRectPx;

        m_shell.m_scene->SyncSceneDriveChrome();
    }
    else if (m_shell.DeskSceneActive() && m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        // Monitor opted out, fullscreen: still the immersive presentation --
        // every chrome band hidden and the picture filling the client (the
        // renderer letterboxes inside the target bounds), just without the
        // curved glass. The drives come from the overlay strip exactly as
        // they do with the monitor on, so the main composition holds nothing.
        m_chromeSceneScale = 1.0f;
        viewportRect       = { 0, 0, widthPx, heightPx };

        m_shell.m_scene->GetScene().SetComposition (DeskSceneComposition{});

        m_shell.m_scene->SyncSceneDriveChrome();
    }
    else if (m_shell.DeskSceneActive())
    {
        // Monitor opted out: the picture goes back on a flat rect at classic
        // sizes, but the drives are NOT optional -- they compose as a 3D row
        // in the bottom band, through the same drives-only solve (its own
        // contained camera over the band, so FR-016 still holds within it)
        // the fullscreen overlay strip uses. The band keeps its classic
        // thickness, so the window geometry matches the flat chrome it
        // replaces.
        DeskSceneComposition  comp;
        RECT                  band     = {};
        RECT                  driveRow = {};
        bool                  composed = false;
        int                   pad      = m_shell.m_scaler.ToPx (s_kSceneDriveRowPadDp);

        m_chromeSceneScale = 1.0f;
        center             = ComputeViewportRect (widthPx, heightPx);
        viewportRect       = center;

        band     = m_driveBand.GetBounds();
        driveRow = { pad, band.top + pad / 2, widthPx - pad,
                     std::max (band.bottom - pad - m_shell.m_scaler.ToPx (s_kSceneDriveLabelStripDp +
                                                                          s_kSceneDriveLabelGapDp),
                                       (LONG) band.top) };

        // A machine with no Disk ][ controller composes no row at all, and a
        // band too small to solve leaves the scene empty rather than stale.
        if (m_shell.m_scene->DeskSceneDriveCount() > 0)
        {
            composed = DeskSceneLayout::ComputeStrip (driveRow, m_shell.m_scaler.GetDpi(), m_shell.m_scene->DeskSceneDriveCount(),
                                                      m_shell.m_scene->GetScene().Metrics(), comp,
                                                      DeskSceneLayout::kDriveBandGazeDownRad) == S_OK;
        }

        m_shell.m_scene->GetScene().SetComposition (composed ? comp : DeskSceneComposition{});

        m_shell.m_scene->SyncSceneDriveChrome();
    }
    else if (m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        // No scene, fullscreen: the same bargain the desk scene makes. Every
        // chrome band is hidden, the picture fills the client (the renderer
        // letterboxes inside the target bounds), and the drives come back as
        // the flat widgets riding the overlay strip.
        m_chromeSceneScale = 1.0f;
        viewportRect       = { 0, 0, widthPx, heightPx };
    }
    else
    {
        // No scene at all (compact theme, or the models never loaded): the
        // bare display fills the center at classic sizes over the 2D drive
        // band.
        //
        // The compass has to be put away HERE. Every other arm reaches it
        // through SyncSceneDriveChrome, which this one has no reason to call,
        // so switching from a skeuo theme to a compact one left the arrows
        // painted over the flat display -- a control for turning a scene that
        // is no longer on screen.
        m_chromeSceneScale = 1.0f;
        center             = ComputeViewportRect (widthPx, heightPx);
        viewportRect       = center;

        m_shell.m_scene->LayoutSceneCompass();
    }

    m_shell.m_viewport->Layout (viewportRect, m_shell.m_scaler);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SyncChromeBands
//
//  Stamps each chrome band's GetBounds() height with its current DPI-scaled
//  pixel thickness so DxuiDockLayout reads the right slab extents. Only
//  the docked axis (height, for the Top/Bottom bands) is meaningful; the
//  bands are never painted.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SyncChromeBands()
{
    ChromeBandInputs     inputs;
    ChromeBandHeightsPx  px;



    // Which bands exist, and how tall, is ChromeBandLayout's to decide from
    // what the machine has and how it is shown; this reads those facts off
    // the shell and stamps the answers onto the bands' docked heights.
    inputs.hasDiskController   = (m_shell.m_disks->GetManager() != nullptr) && m_shell.m_disks->GetManager()->HasSlot6Controller();
    inputs.crtMonitorActive    = m_shell.CrtMonitorActive();
    inputs.hasCaseSwitches     = m_shell.MachineHasCaseSwitches();
    inputs.driveBarThicknessDp = m_driveBarThicknessDp;
    inputs.chromeSceneScale    = m_chromeSceneScale;
    inputs.toolbarBandDp       = m_toolbar.GetBandDp();

    // Measured against the CLIENT width, which is what the band will be given.
    // Measuring against the viewport is what put the text off the edge: the
    // picture keeps its own aspect and can be wider than the window.
    inputs.changeBandPx        = m_shell.m_disks->GetChangeBandThicknessPx (m_lastClientWidthPx);
    inputs.captureBandPx       = GetCaptureBandThicknessPx (m_lastClientWidthPx);

    px = ChromeBandLayout::Compute (inputs, m_shell.m_scaler);

    m_titleBand.SetBounds   (RECT{ 0, 0, 0, px.title });
    m_navBand.SetBounds     (RECT{ 0, 0, 0, px.nav });
    m_toolbarBand.SetBounds (RECT{ 0, 0, 0, px.toolbar });
    m_changeBand.SetBounds  (RECT{ 0, 0, 0, px.change });
    m_standInBand.SetBounds (RECT{ 0, 0, 0, px.capture });
    m_driveBand.SetBounds   (RECT{ 0, 0, 0, px.drive });
    m_switchBand.SetBounds  (RECT{ 0, 0, 0, px.switches });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::CollectDockedBands
//
//  Every band that peels an edge off the client area before the emulator
//  viewport gets what is left.
//
//  Both directions read this: the pass that docks them, and the inverse that
//  works out which client size leaves a given viewport. They used to carry a
//  copy each, and the copies disagreed -- the inverse was missing the two
//  notice bands, so with either notice up, every size it answered was short
//  by that notice's height and the viewport it exists to preserve shrank by
//  exactly that much.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::CollectDockedBands (IDxuiControl * (& outBands)[kDockedBandCount])
{
    outBands[0] = &m_titleBand;
    outBands[1] = &m_navBand;
    outBands[2] = &m_toolbarBand;
    outBands[3] = &m_changeBand;
    outBands[4] = &m_standInBand;
    outBands[5] = &m_driveBand;
    outBands[6] = &m_switchBand;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::ComputeViewportRect
//
//  Docks the chrome bands (title + nav on top, drive on the bottom)
//  around a Fill center over the client rect and returns the center
//  (emulator viewport) rect the dock leaves in the middle.
//
////////////////////////////////////////////////////////////////////////////////

RECT ShellChrome::ComputeViewportRect (int widthPx, int heightPx)
{
    //  The edge bands plus the fill they surround. One list, shared with
    //  GetClientSizeForCenterPx, so the inverse cannot fall out of step with
    //  this pass.
    IDxuiControl *  docked[kDockedBandCount]    = {};
    IDxuiControl *  kids[kDockedBandCount + 1] = {};



    CollectDockedBands (docked);
    std::copy (std::begin (docked), std::end (docked), std::begin (kids));
    kids[kDockedBandCount] = &m_centerBand;



    // The toolbar's band thickness depends on its responsive mode (icon+label
    // / ribbon / icon-only), which depends on the width -- plan it BEFORE the
    // bands dock so the strip gets the right height for this window size.
    m_toolbar.PlanForWidth (widthPx, m_shell.m_scaler);

    //  The notice's height depends on the width it is about to be given, and
    //  SyncChromeBands is where every band's thickness is decided -- so the
    //  width has to be known before it runs.
    m_lastClientWidthPx = widthPx;

    SyncChromeBands();
    m_chromeDock.Arrange (RECT{ 0, 0, widthPx, heightPx }, m_shell.m_scaler, kids);

    // The command toolbar rides its band: re-lay it every viewport pass so a
    // resize / DPI change reflows the buttons with the strip.
    //
    // EXCEPT IN FULLSCREEN, where the reveal overlay owns it. Both were
    // laying it out -- the dock into a band the fullscreen viewport does not
    // show, the overlay across the top -- so whichever ran last won, and the
    // buttons painted at one height while their hit rects sat at the other.
    // A single owner per presentation, or they disagree.
    if (!m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        m_toolbar.Layout (m_toolbarBand.GetBounds(), m_shell.m_scaler);
    }

    //  The notice rides its band the way the toolbar rides its own, so a
    //  resize or a DPI change reflows it with everything else.
    m_shell.m_disks->LayoutChangeBanner (m_changeBand.GetBounds());

    //  AND SO DOES THE INPUT-MODE BAR. It was laid out only from the present
    //  path, which runs on the frame's cadence rather than the resize's, so
    //  while the toolbar and the picture followed the drag the bar arrived
    //  behind them, a step at a time. Everything that rides a band is laid
    //  out here, in the pass that gives the bands their rects.
    SyncStandInBanner();

    return m_centerBand.GetBounds();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::ReflowChromeForMachineChange
//
//  A machine switch may add or remove the Disk ][ controller, which changes the
//  drive-band thickness (Phase D), the drive-widget visibility, and the hit-test
//  map. When disk presence changes, grow/shrink the WINDOW by the band delta so
//  the emulator viewport keeps its size and the top-left corner stays put -- NOT
//  hold the window size and re-center the viewport. The resulting WM_SIZE drives
//  OnSize, which re-lays the bands / widgets / hit rects. When presence is
//  unchanged (e.g. a swap between two controller-equipped machines) there is no
//  band delta, so just re-run OnSize at the current size to refresh the widgets.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::ReflowChromeForMachineChange()
{
    RECT  rcWindow      = {};
    bool  haveWindow    = false;
    bool  newHasDisk    = false;
    bool  newIsApple2c  = false;
    bool  layoutChanged = false;
    bool  didResize     = false;



    DXUI_ASSERT_UI_THREAD();   // chrome layout: never from the CPU thread

    haveWindow = m_shell.m_hwnd != nullptr && GetWindowRect (m_shell.m_hwnd, &rcWindow);

    if (haveWindow)
    {
        newHasDisk    = (m_shell.m_disks->GetManager() != nullptr) && m_shell.m_disks->GetManager()->HasSlot6Controller();
        newIsApple2c  = m_shell.MachineHasCaseSwitches();
        layoutChanged = (newHasDisk != m_chromeSizedForHasDisk) ||
                        (newIsApple2c != m_chromeSizedForApple2c);
    }

    // The desk wears what the machine wore, so crossing the //c boundary
    // swaps both models. Reloading rebuilds every cached mesh, so the
    // scene's own state is pushed again right after. Switching to or from a
    // machine with cassette jacks adds or removes the recorder, which is a
    // reload too. Attaching or detaching the recorder is not: its model stays
    // loaded while the machine has the jacks, and is only shown or hidden.
    if (m_shell.m_scene->IsReady() &&
        (m_shell.MachineHasCaseSwitches() != m_shell.m_scene->IsMachineC() ||
         m_shell.m_tapeDeck->MachineHasCassettePort() != m_shell.m_scene->GetScene().IsRecorderLoaded()))
    {
        HRESULT  hrModels = m_shell.m_scene->LoadDeskSceneModelsForMachine();

        if (SUCCEEDED (hrModels))
        {
            m_shell.m_scene->GetScene().SetPowerLampOn (true);
        }

        IGNORE_RETURN_VALUE (hrModels, S_OK);
    }

    if (m_shell.m_scene->IsReady())
    {
        m_shell.m_scene->GetScene().SetRecorderShown (m_shell.m_tapeDeck->IsTapeRecorderShown());
        m_shell.m_scene->InvalidateSceneComposition();
    }

    // Resize the window by the total bottom-band delta -- the drive band
    // (disk-presence) plus the //c switch band -- but not for min/max/fullscreen
    // windows, where the user explicitly chose the size (mirrors
    // ApplyThemeToChrome). Those just relayout inside the fixed frame.
    if (haveWindow && layoutChanged &&
        !IsIconic (m_shell.m_hwnd) && !IsZoomed (m_shell.m_hwnd) && !m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        int  oldDriveDp  = m_chromeSizedForHasDisk ? m_driveBarThicknessDp : 0;
        int  newDriveDp  = newHasDisk              ? m_driveBarThicknessDp : 0;
        int  oldSwitchDp = m_chromeSizedForApple2c ? ChromeBandLayout::kSwitchBandDp : 0;
        int  newSwitchDp = newIsApple2c            ? ChromeBandLayout::kSwitchBandDp : 0;
        int  deltaPx     = (m_shell.m_scaler.ToPx (newDriveDp)  - m_shell.m_scaler.ToPx (oldDriveDp)) +
                           (m_shell.m_scaler.ToPx (newSwitchDp) - m_shell.m_scaler.ToPx (oldSwitchDp));

        m_chromeSizedForHasDisk = newHasDisk;
        m_chromeSizedForApple2c = newIsApple2c;

        // The bands are bottom-docked full-width, so only the height moves.
        // SWP_NOMOVE pins the top-left corner; the WM_SIZE it generates drives
        // OnSize to re-lay the bands, widgets, and hit-test map.
        SetWindowPos (m_shell.m_hwnd, nullptr, 0, 0,
                      rcWindow.right  - rcWindow.left,
                      (rcWindow.bottom - rcWindow.top) + deltaPx,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        // The WM_SIZE above already re-lays everything, so the relayout below
        // must not also run.
        didResize = true;
    }
    else if (haveWindow)
    {
        m_chromeSizedForHasDisk = newHasDisk;
        m_chromeSizedForApple2c = newIsApple2c;
    }

    // No band delta (unchanged presence, or a fixed-state window): relayout at
    // the current client size so widget visibility + hit rects still refresh.
    if (haveWindow && !didResize)
    {
        RECT  rcClient = {};

        if (GetClientRect (m_shell.m_hwnd, &rcClient) &&
            rcClient.right > rcClient.left && rcClient.bottom > rcClient.top)
        {
            (void) m_shell.OnSize (static_cast<UINT> (rcClient.right  - rcClient.left),
                                   static_cast<UINT> (rcClient.bottom - rcClient.top));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::ApplyThemeToChrome
//
//  Push freshly-activated theme into the chrome regions whose layout
//  depends on theme state. Currently that's the drive bar:
//      * Compact themes shrink the bottom inset and switch the per-
//        drive widget to the small flat card paint path.
//      * Skeuomorphic restores the full 192dp inset and the
//        Apple ][-style realistic widgets.
//  When the bottom inset changes, the HWND is resized by the delta so
//  the emulator pixel grid is preserved across the theme swap (i.e.
//  the user's window grows or shrinks instead of the framebuffer
//  pillarboxing). The actual painter re-layout happens inside OnResize
//  via the existing WM_SIZE path.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::ApplyThemeToChrome (const CassoTheme & theme)
{
    // Bottom drive-bar thickness, full and compact. Full is the desk scene's
    // drive band, which SyncChromeBands scales by m_chromeSceneScale
    // (s_kDeskDriveScale at 100%) so it hugs the scaled 3D drives without a
    // separate constant. Compact holds the flat drive widget, bottom-anchored
    // under an 8 dp gap.
    constexpr int  s_kFullDriveBarDp    = 190;
    constexpr int  s_kCompactDriveBarDp = 70;



    int   desiredThicknessDp = theme.compactDrives ? s_kCompactDriveBarDp : s_kFullDriveBarDp;
    int   priorThicknessDp   = m_driveBarThicknessDp;
    RECT  rcClient           = {};
    RECT  rcWindow           = {};
    int   centerW            = 0;
    int   centerH            = 0;
    bool  canResize          = false;



    // The device selector's glyph style follows the drive style --
    // full skeuomorphic themes get the 3/4 perspective peripherals, compact
    // (DarkModern / retro) themes the top-down glyphs.

    // The strip continues the menu bar's themed surface (navStrip) and its
    // labels take the bar's ink; neither is a color the generic theme
    // mapping carries.
    m_toolbar.SetStripColors (theme.navStrip, theme.navItemText);

    // Push the nav/dropdown palette onto the menu bar so both the
    // in-window strip and the popup-backed dropdown render with chrome
    // colors (the old per-frame apply path is dead post-T129).
    m_mainMenu.ApplyChromeColors (theme);

    // Tooltips cache their surface colors instead of reading the theme at
    // paint time -- the popup path hands its background to the popup host at
    // Show, before any painter exists -- so a theme swap has to re-seed them
    // here. Without this the balloons kept the palette that was live when the
    // window was built, leaving skeuomorphic blue tips over a green
    // RetroTerminal chrome.
    m_toolbarTooltip.SetTheme   (theme);
    m_switchBarTooltip.SetTheme (theme);
    m_driveTooltip.SetTheme     (theme);
    m_captionTooltip.SetTheme   (theme);

    // A balloon that is already up was sized and cleared with the outgoing
    // colors, and nothing repaints its background. Take it down; the next
    // hover raises it in the new palette.
    m_toolbarTooltip.HideImmediate();
    m_switchBarTooltip.HideImmediate();
    m_driveTooltip.HideImmediate();
    m_captionTooltip.HideImmediate();

    // Every path applies the new thickness; only the window resize is
    // conditional. Min/max/fullscreen windows are skipped because the user
    // explicitly chose that state and should not see the window resize from
    // under them on a theme swap -- the thickness still lands, so the next
    // normal-state resize uses the right math. Short-circuit order matters:
    // the Is* / Get* calls must not run on a null HWND.
    canResize = m_shell.m_hwnd != nullptr
                && desiredThicknessDp != priorThicknessDp
                && !IsIconic (m_shell.m_hwnd)
                && !IsZoomed (m_shell.m_hwnd)
                && !m_shell.m_renderer->GetD3D().IsFullscreen()
                && GetClientRect (m_shell.m_hwnd, &rcClient)
                && GetWindowRect (m_shell.m_hwnd, &rcWindow);

    if (canResize)
    {
        // Capture the current center (emulator viewport) size BEFORE
        // mutating the drive-bar thickness -- ComputeViewportRect reads it.
        // The user may have resized the window manually since boot;
        // preserving "the emu viewport stays the same size, the drive bar
        // grows/shrinks around it" is the intuitive contract on a theme swap.
        RECT  before = ComputeViewportRect (rcClient.right  - rcClient.left,
                                            rcClient.bottom - rcClient.top);

        centerW = before.right  - before.left;
        centerH = before.bottom - before.top;
    }

    // Sits between the two blocks on purpose: the capture above needs the old
    // thickness, the sizing below needs the new one.
    m_driveBarThicknessDp = desiredThicknessDp;

    if (canResize)
    {
        SIZE  newClient   = GetClientSizeForCenterPx (centerW, centerH);
        int   ncOverheadH = (rcWindow.bottom - rcWindow.top) - (rcClient.bottom - rcClient.top);
        int   ncOverheadW = (rcWindow.right  - rcWindow.left) - (rcClient.right  - rcClient.left);
        int   newWindowW  = (int) newClient.cx + ncOverheadW;
        int   newWindowH  = (int) newClient.cy + ncOverheadH;

        SetWindowPos (m_shell.m_hwnd, nullptr, 0, 0, newWindowW, newWindowH,
                      SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    }

    // Re-run the authoritative layout unconditionally. The resize above only
    // produces a WM_SIZE when the window size actually CHANGES -- a swap
    // whose band delta nets out (compact bar vs the scene's joystick-only
    // bar), a maximized window, or fullscreen all skip it, and the 3D desk
    // scene depends on this pass: its composition only computes here, so
    // skipping it leaves a stale camera (drives off-screen) and the
    // outgoing theme's widgets still laid out. Idempotent when WM_SIZE
    // already ran it.
    if (m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &rcClient))
    {
        (void) m_shell.OnSize ((UINT) (rcClient.right - rcClient.left),
                               (UINT) (rcClient.bottom - rcClient.top));
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SetCrtMonitorEnabled
//
//  Settings > Theme opt in/out for the CRT monitor -- the escape hatch back to
//  the flat picture at classic sizes (the 3D drives stay either way).
//  Persists the choice, then re-runs the authoritative OnSize layout at the
//  current client size so the monitor appears or disappears in place -- the
//  window itself does not resize; Ctrl+0 reaches the mode's 100% default.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SetCrtMonitorEnabled (bool enabled)
{
    HRESULT  hr       = S_OK;
    RECT     rcClient = {};



    BAIL_OUT_IF (m_shell.m_settings->GetPrefs().crtMonitor == enabled, S_OK);

    m_shell.m_settings->GetPrefs().crtMonitor = enabled;

    if (m_shell.m_settings->GetConfigStore() != nullptr)
    {
        hr = m_shell.m_settings->GetConfigStore()->SaveAll (m_shell.m_settings->GetPrefs(), m_shell.m_settings->GetFileSystem());
    }
    else
    {
        hr = m_shell.m_settings->GetPrefs().Save (m_shell.m_machine.GetAssetBaseDir(), m_shell.m_settings->GetFileSystem());
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &rcClient))
    {
        (void) m_shell.OnSize ((UINT) (rcClient.right - rcClient.left),
                               (UINT) (rcClient.bottom - rcClient.top));
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::LayoutSwitchBar
//
//  Positions the //c case-switch strip over its chrome band. On any other
//  machine the band is zero-height, so the strip is hidden (and un-hit-tested).
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::LayoutSwitchBar (UINT dpi)
{
    DxuiDpiScaler  scaler;



    if (!m_shell.MachineHasCaseSwitches())
    {
        m_switchBar.Hide();
        return;
    }

    scaler.SetDpi (dpi);
    SyncSwitchBarState();
    m_switchBar.Layout (m_switchBand.GetBounds(), scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SyncSwitchBarState
//
//  Pushes the live //c switch + indicator state onto the strip: the two
//  latching switches are read back from the keyboard device (single source of
//  truth), the disk-use LED tracks slot-6 drive activity, power is always lit
//  while the machine runs, and the reset button is "armed" only while Ctrl is
//  held (real Control-Reset).
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SyncSwitchBarState()
{
    Apple2eKeyboard *  iieKbd = m_shell.m_machine.GetRefs().iieKeyboard;
    bool               diskOn = false;



    if (iieKbd != nullptr)
    {
        m_switchBar.SetEightyFortyIn (iieKbd->IsEightyColumnSwitchIn());
        m_switchBar.SetKeyboardIn    (iieKbd->IsKeyboardSwitchDvorak());
    }

    for (const DriveWidget & drive : m_shell.m_disks->GetDriveChrome())
    {
        diskOn = diskOn || (drive.GetLed() == LedState::Active);
    }

    m_switchBar.SetDiskActive (diskOn);
    m_switchBar.SetPowerOn    (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::HandleSwitchBarClick
//
//  Actions a left-button release over one of the //c switch-strip parts. The
//  reset button is inert unless Ctrl is held (the real //c key does nothing on
//  its own); Open-Apple / Closed-Apple, mapped from the held Alt keys, ride the
//  reset into the firmware for a cold boot / diagnostics. The 80/40 and
//  keyboard switches latch: each click flips the switch state on the keyboard
//  device, which the strip re-reads on the next SyncSwitchBarState.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::HandleSwitchBarClick (Apple2cSwitchBar::Part part)
{
    // The devices are read here on the UI thread; a machine switch on the
    // CPU thread holds the lifetime lock exclusively while it replaces
    // them, and a click that lands in that window is dropped.
    std::shared_lock<std::shared_mutex>  lifetime (m_shell.m_machine.GetLifetimeLock(), std::try_to_lock);
    Apple2eKeyboard *                    iieKbd = nullptr;



    if (!lifetime.owns_lock())
    {
        return;
    }

    iieKbd = m_shell.m_machine.GetRefs().iieKeyboard;

    switch (part)
    {
        case Apple2cSwitchBar::Part::Reset:
            // Only a modifier-qualified press resets, matching the case key.
            if ((GetKeyState (VK_CONTROL) & 0x8000) != 0)
            {
                if (m_shell.m_machine.GetRefs().keyboard != nullptr)
                {
                    m_shell.m_machine.GetRefs().keyboard->SetKeyDown (false);
                }

                m_shell.RequestReset();
            }

            break;

        case Apple2cSwitchBar::Part::EightyForty:
            if (iieKbd != nullptr)
            {
                bool  newIn = !iieKbd->IsEightyColumnSwitchIn();

                iieKbd->SetEightyColumnSwitchIn (newIn);
                m_shell.PersistSwitchState ("eightyColumnSwitch", newIn);
            }

            break;

        case Apple2cSwitchBar::Part::Keyboard:
            if (iieKbd != nullptr)
            {
                bool  newDvorak = !iieKbd->IsKeyboardSwitchDvorak();

                iieKbd->SetKeyboardSwitchDvorak (newDvorak);
                m_shell.PersistSwitchState ("keyboardDvorak", newDvorak);
            }

            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::GetCaptureBandThicknessPx
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

int ShellChrome::GetCaptureBandThicknessPx (int clientWidthPx) const
{
    if (m_shell.m_renderer->GetD3D().IsFullscreen() || clientWidthPx <= 0)
    {
        return 0;
    }

    return GetStandInBarHeightPx ((float) clientWidthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::GetStandInBarHeightPx
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

int ShellChrome::GetStandInBarHeightPx (float widthPx) const
{
    IDxuiTextRenderer *  text = (m_shell.m_host != nullptr) ? m_shell.m_host->GetTextRenderer() : nullptr;
    std::wstring         line = m_shell.GetStandInBannerText();
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
//  ShellChrome::ReflowChromeForChangeBand
//
//  Re-docks everything after the notice appears or goes.
//
//  THE WINDOW KEEPS ITS SIZE. The machine-change reflow beside this one grows
//  and shrinks the window, because a machine with no disk drives genuinely
//  needs less of it and the user keeps that size for the session. A notice is
//  transient: the picture gives up the height while it is up and takes it back
//  when it goes, which is what makes the strip read as sliding in over the
//  scene rather than shoving the window about.
//
//  RUN THROUGH OnSize, which is the one authoritative layout pass. A second
//  path that re-docked some of the chrome would be a second answer to where
//  everything goes.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::ReflowChromeForChangeBand()
{
    RECT  client = {};



    DXUI_ASSERT_UI_THREAD();   // chrome layout: never from the CPU thread

    //  NEVER FROM INSIDE THE PASS IT RUNS. Losing the pointer capture re-docks,
    //  and the capture is dropped from OnCancelMode / OnKillFocus, which a
    //  resize itself can raise -- so the layout would call itself.
    if (m_inChromeLayout || m_shell.m_hwnd == nullptr || !GetClientRect (m_shell.m_hwnd, &client))
    {
        return;
    }

    {
        DxuiMessageResult  sized = m_shell.OnSize (client.right - client.left,
                                                   client.bottom - client.top);

        IGNORE_RETURN_VALUE (sized, DxuiMessageResult::Handled);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowNotice
//
//  Show a notice over the picture for a few seconds. UI thread only.
//
//  The text arrives already composed -- by CaptureOutcome::DescribeResult or
//  WriteProtectChange::DescribeResult -- which is deliberate: every branch of
//  what to say is decided in core where a test can reach it, and this
//  function chooses no wording.
//
//  A notice already up keeps its full time; this one goes below it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::ShowNotice (const std::wstring & text)
{
    int64_t   nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                          std::chrono::steady_clock::now().time_since_epoch()).count();



    m_notices.Push (text, nowMs);

    SyncNotice();

    m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostNotice
//
//  Hand a notice to the window from any thread. The notice is Dxui and Dxui
//  asserts UI-thread affinity, so a caller on the CPU thread cannot show it
//  directly. With no window there is nothing to show it over, and the notice
//  is dropped: it only confirms a change the indicators already show.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::PostNotice (const std::wstring & text)
{
    wstring *  carried = nullptr;



    if (m_shell.m_hwnd == nullptr)
    {
        return;
    }

    carried = new (std::nothrow) wstring (text);

    if (carried != nullptr && !PostMessageW (m_shell.m_hwnd, WM_APP_SHOW_NOTICE, 0,
                                             reinterpret_cast<LPARAM> (carried)))
    {
        delete carried;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncNotice
//
//  Lay the notice out while it is live, and drop it once it expires.
//
//  ACROSS THE TOP, UNDER EVERYTHING DOCKED THERE. The chrome at the top of
//  the window is where this window puts what it has to say about itself, and
//  a screenshot's filename is exactly that -- the one part of a capture that
//  is about the application rather than about the machine. Put over the
//  picture it lands on whatever the user just photographed, and read as a
//  caption on it.
//
//  IT OVERLAYS RATHER THAN DOCKS, which is the one way it differs from the
//  pointer-capture bar beside it. That bar tracks a state and is worth the
//  height it takes from the picture; this one is up for four seconds, and a
//  band that appears and vanishes on a timer would reflow the machine twice
//  for every screenshot. So it hangs UNDER the last docked band and covers a
//  strip of picture, dimmed by a scrim rather than hidden behind a panel.
//
//  Separate from the pointer-capture bar, not a reuse of it: a screenshot
//  taken with the paddle captured must not replace the words telling the user
//  how to get their cursor back.
//
//  AN EXPIRY OR A SLIDE ASKS FOR ITS OWN FRAMES. A notice leaving, and the
//  ones below it moving up, change the picture with nothing else asking for
//  a present; a paused machine would otherwise leave a stale notice up until
//  something unrelated repainted. WaitForFrameOrMessage wakes for the next
//  expiry for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SyncNotice()
{
    RECT                 client   = {};
    RECT                 rc       = {};
    IDxuiTextRenderer *  text     = (m_shell.m_host != nullptr) ? m_shell.m_host->GetTextRenderer() : nullptr;
    float                width    = 0.0f;
    size_t               countWas = m_notices.GetCount();
    int64_t              nowMs    = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                        std::chrono::steady_clock::now().time_since_epoch()).count();



    //  The system's animation setting is read here and passed in, so the
    //  stack moves notices at once when the user has turned animations off.
    m_notices.SetAnimationsEnabled (DxuiSystemSettings::Instance().AreMenuAnimationsEnabled());
    m_notices.Tick (nowMs);

    if (m_notices.GetCount() != countWas || m_notices.IsAnimating (nowMs))
    {
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }

    if (!m_notices.IsShowing (nowMs) || m_shell.m_hwnd == nullptr || !GetClientRect (m_shell.m_hwnd, &client))
    {
        m_notices.SetVisible (false);
        return;
    }

    width = (float) (client.right - client.left);

    rc.left  = client.left;
    rc.right = client.right;
    rc.top   = ComputeTopOverlayEdgePx (client);

    //  Measured where there is a renderer to ask; the estimate is the
    //  fallback for the frames before the renderer exists.
    m_notices.SetDpi (m_shell.m_scaler.GetDpi());

    rc.bottom = rc.top + (LONG) m_notices.MeasureHeightPx (text, width, m_shell.m_scaler);

    m_notices.Layout     (rc, m_shell.m_scaler);
    m_notices.SetVisible (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeTopOverlayEdgePx
//
//  Where the picture starts, for something that wants to hang over the top of
//  it without landing on the chrome.
//
//  IT ASKS THE BANDS RATHER THAN ADDING THEM UP. Which bands are at the top,
//  and which of those are showing, varies by theme, by fullscreen and by
//  whether the mouse is currently captured -- a count kept here would be a
//  second copy of the dock's arithmetic, and the copy is the one that goes
//  wrong. The lowest bottom edge among the bands that are up IS the answer,
//  and it stays the answer when a band is added.
//
//  Fullscreen has no bands at all: the toolbar reveals itself over the
//  picture and the pointer-capture bar hangs beneath it, and both are in the
//  list for exactly that case.
//
////////////////////////////////////////////////////////////////////////////////

LONG ShellChrome::ComputeTopOverlayEdgePx (const RECT & client) const
{
    const IDxuiControl * const  bands[] = { &m_mainMenu,
                                            &m_toolbar,
                                            &m_shell.m_disks->GetChangeBanner(),
                                            &m_standInBarSurface,
                                            &m_standInBar };
    LONG                        top     = client.top;
    RECT                        rc      = {};



    for (const IDxuiControl * band : bands)
    {
        rc = band->GetBounds();

        if (band->IsVisible() && rc.bottom > rc.top
            && rc.top < client.bottom && rc.bottom > top)
        {
            top = rc.bottom;
        }
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetStandInOverlaysHidden
//
//  HIDE WHAT DESCRIBES THE APPLICATION; CAPTURE WHAT DESCRIBES THE MACHINE.
//
//  The compass is a control, the two readouts are diagnostics, and the
//  pointer-capture bar is a transient piece of state -- none of them are part
//  of the machine on the desk, and each can sit inside the viewport where a
//  Scene capture would otherwise collect it.
//
//  A useful side effect: a scene capture no longer depends on which
//  diagnostics happen to be switched on, so two captures of the same view are
//  the same image.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SetStandInOverlaysHidden (bool hidden)
{
    if (hidden)
    {
        m_shell.m_scene->GetCompass().SetVisible     (false);
        m_shell.m_scene->GetCompassHint().SetVisible      (false);
        m_fpsReadout.SetVisible       (false);
        m_shell.m_scene->GetViewReadout().SetVisible (false);
        //  The pointer-capture bar is docked chrome in a window, and a
        //  Scene capture takes the viewport, so there it is already out of
        //  frame. In FULLSCREEN there are no bands and the bar hangs off the
        //  top edge, inside the picture -- which is the case this covers.
        m_standInBar.SetVisible        (false);
        m_standInBarSurface.SetVisible (false);

        //  Including the notices. Two captures inside a notice's few seconds
        //  would otherwise photograph the first one's filename.
        m_notices.SetVisible (false);
    }
    else
    {
        //  Restored by the layout pass that owns each one, rather than by
        //  remembering four booleans here -- the pose readout and the frame
        //  rate are driven by prefs, the compass by whether a scene is up,
        //  and the banner by whether the mouse is captured. Re-deriving is
        //  what keeps this from disagreeing with them.
        m_shell.m_scene->LayoutSceneCompass();
        SyncFrameRateReadout();
        m_shell.m_scene->SyncSceneViewReadout();
        SyncStandInBanner();
        SyncNotice();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::GetClientSizeForCenterPx
//
//  Inverse of ComputeViewportRect: given a desired center (emulator
//  viewport) size in physical pixels, return the client size that hosts
//  it with the current chrome-band thicknesses.
//
////////////////////////////////////////////////////////////////////////////////

SIZE ShellChrome::GetClientSizeForCenterPx (int centerWidthPx, int centerHeightPx)
{
    //  THE SAME BANDS ComputeViewportRect DOCKS, or this is not its inverse
    //  -- so it reads the same list rather than restating it. The two notice
    //  bands were once missing from the copy that lived here: with either
    //  one up, every client size answered here (the minimum tracking size,
    //  the window a machine or theme change resizes to) came out short by
    //  the notice's height, and the viewport it exists to preserve shrank by
    //  exactly that.
    IDxuiControl *  bands[ShellChrome::kDockedBandCount] = {};



    CollectDockedBands (bands);



    SyncChromeBands();

    return m_chromeDock.GetContainerSizeForFill (SIZE{ centerWidthPx, centerHeightPx }, bands);
}
