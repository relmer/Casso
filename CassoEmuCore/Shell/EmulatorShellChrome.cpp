#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
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
//  Window placement helpers
//
//  Geometry for the bottom command bar's occupants -- the drive widgets and
//  the joystick-mode button -- plus the window-size reconciliation that runs
//  once the frame has materialized.
//
//  Two ideas recur through this block.
//
//  Desk-scene zoom is applied by folding the scene scale into the EFFECTIVE
//  DPI rather than by scaling rects afterwards. Widget geometry, fonts, and
//  the inter-widget gaps then zoom together for free, because every one of
//  them already derives from DPI.
//
//  The drives are laid out as objects on a desk, not as flat controls. Each
//  widget is skewed toward a shared vanishing point at the client center by a
//  factor matching the case-top depth ratio in DriveWidget, so two drives side
//  by side read as sitting on the same surface under the same monitor rather
//  than as two identical sprites.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutDriveWidgetsInCommandBar (
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



    // Desk-scene zoom: the drives scale with the monitor. Fold the scale
    // into the effective DPI so widget geometry, fonts, and the inter-
    // widget gaps all zoom together.
    dpi = (UINT) lroundf ((float) dpi * sceneScale);

    bottomInset = bottomInsetPx;
    commandBarTop = std::max (0, clientH - bottomInset);
    gap = MulDiv (driveChrome[0].IsCompact() ? s_kCompactDriveWidgetGapDp : s_kDriveWidgetGapDp,
                  static_cast<int> (dpi), s_kBaseDpi);



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

    // A LONE drive centers on the part that carries the weight -- the disk
    // name and its head bar -- not on the whole widget. The 2D widget hangs
    // its "DRIVE 1" caption off to the left, so centering the outer box put
    // the name and bar half a caption column right of center and the row
    // looked hung off to one side.
    //
    // The offset is measured off the widget rather than assumed, so the full
    // skeuomorphic drive, whose body starts at its own left edge, subtracts
    // nothing and is unaffected. Two drives keep centering on the pair: the
    // caption then reads as part of a repeating unit rather than as a tail on
    // a single object.
    {
        int  captionLead = driveChrome[0].GetBodyRect().left - probe.left;

        x = DriveRowLayout::ApplyLoneDriveCaptionOffset (x, captionLead, visibleCount);
    }

    // Anchor the widget to the bottom so the margin between the
    // basename label and the window edge mirrors the gap between
    // the drive body and the label (s_kLabelStripGapPx, scaled).
    bottomGap = MulDiv (s_kLabelBottomGapDp, static_cast<int> (dpi), s_kBaseDpi);
    y         = std::max (commandBarTop, clientH - widgetH - bottomGap);

    for (i = 0; i < driveChrome.size(); i++)
    {
        int   widgetX      = DriveRowLayout::ComputeWidgetX (x, static_cast<int> (i), widgetW, gap);
        int   skewPx       = DriveRowLayout::ComputePerspectiveSkewPx (clientW, widgetX, widgetW);
        RECT  widgetAnchor = { widgetX, y, widgetX, y };

        // Visible again: the desk scene turns these off rather than just
        // collapsing them, and this is the one path that brings the flat
        // widgets back, so it is where they earn their visibility.
        driveChrome[i].SetVisible (true);
        driveChrome[i].SetPerspectiveSkewPx (skewPx);
        driveChrome[i].Layout (widgetAnchor, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetChromeHiddenForFullscreenScene
//
//  Visibility, not bounds: the adopted chrome controls paint from their own
//  cached layouts, so an empty rect is not a reliable hidden state --
//  SetVisible is. The host caption gets its explicit switch. Symmetric: the
//  windowed layout path calls this with `hidden = false` every pass, so
//  leaving fullscreen restores everything.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetChromeHiddenForFullscreenScene (bool hidden)
{
    // A borderless-fullscreen window fills the monitor and has nothing to
    // resize TO: its edges ARE the screen's, and leaving the resize borders
    // armed lets a drag at a corner pull the picture down to a fraction of
    // the screen with no caption left to put it right with.
    m_host->SetResizable (!hidden);

    m_host->SetCaptionVisible (!hidden);
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
    m_driveBandSurface.SetVisible (!hidden && !DeskSceneActive());
    m_switchBar.SetVisible (!hidden);

    // Leaving fullscreen must not hand the flat widgets back to a scene that
    // has already retired them.
    m_driveChrome[0].SetVisible (!hidden && !DeskSceneActive());
    m_driveChrome[1].SetVisible (!hidden && !DeskSceneActive());

    if (hidden)
    {
        m_driveChrome[0].Hide();
        m_driveChrome[1].Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncStandInBanner
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

void EmulatorShell::SyncStandInBanner()
{
    RECT   client = {};
    RECT   rc     = {};
    RECT   strip  = {};
    LONG   top    = 0;
    float  width  = 0.0f;



    m_standInBar.SetText (GetStandInBannerText());

    if (m_standInBar.GetText().empty() || m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
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
        if (m_hwnd != nullptr && !m_d3dRenderer.IsFullscreen()
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
    if (m_paddleCaptured && GetCapture() != m_hwnd && GetForegroundWindow() == m_hwnd)
    {
        SetCapture (m_hwnd);
        ClipPaddleCursorToClient();
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
    if (!m_d3dRenderer.IsFullscreen()
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
    if (!m_d3dRenderer.IsFullscreen())
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
        rc.bottom = top + (LONG) m_standInBar.GetPreferredHeightPx (width, m_scaler);
    }

    //  Nothing is painted into a band too short to hold the text. One frame
    //  of no bar reads as the bar arriving; one frame of a clipped bar reads
    //  as a rendering fault.
    if (rc.bottom - rc.top < (LONG) m_standInBar.GetPreferredHeightPx (
                                        (float) (rc.right - rc.left), m_scaler))
    {
        m_standInBar.SetVisible        (false);
        m_standInBarSurface.SetVisible (false);

        return;
    }

    m_standInBarSurface.Layout     (rc, m_scaler);
    m_standInBarSurface.SetVisible (true);

    m_standInBar.SetDpi     (m_scaler.GetDpi());
    m_standInBar.Layout     (rc, m_scaler);
    m_standInBar.SetVisible (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncFrameRateReadout
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

void EmulatorShell::SyncFrameRateReadout()
{
    RECT     client   = {};
    RECT     rc       = {};
    wchar_t  text[32] = {};



    if (!m_globalPrefs.showFrameRate || m_host == nullptr
        || m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
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
        LONG  bottom = (!m_d3dRenderer.IsFullscreen() && bar.bottom > bar.top)
                     ? bar.top : client.bottom;

        rc.left   = client.left + m_scaler.ToPx (s_kFrameRateInsetDp);
        rc.right  = rc.left + m_scaler.ToPx (s_kFrameRateWidthDp);
        rc.bottom = bottom - m_scaler.ToPx (s_kFrameRateInsetDp);
        rc.top    = rc.bottom - m_scaler.ToPx (s_kFrameRateHeightDp);
    }

    swprintf_s (text, L"%.1f fps", m_host->GetFramesPerSecond());

    m_fpsReadout.SetText        (text);
    m_fpsReadout.SetFontSizeDip (DxuiShadowedText::kFontDip);
    m_fpsReadout.SetAlign       (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_fpsReadout.SetDpi         (m_scaler.GetDpi());
    m_fpsReadout.Layout         (rc, m_scaler);
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
//  EmulatorShell::TickFullscreenTopChrome
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

void EmulatorShell::TickFullscreenTopChrome()
{
    RECT     client    = {};
    POINT    cursor    = {};
    int      menuH     = 0;
    int      toolbarH  = 0;
    int      bandH     = 0;
    bool     want      = false;
    int64_t  nowMs     = 0;



    if (!m_d3dRenderer.IsFullscreen() || m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
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

    m_toolbar.PlanForWidth (client.right - client.left, m_scaler);
    menuH    = DxuiMenuBar::GetStripHeightPx (m_scaler.GetDpi());
    toolbarH = m_scaler.ToPx (m_toolbar.GetBandDp());
    bandH    = menuH + toolbarH;

    if (GetCursorPos (&cursor) && ScreenToClient (m_hwnd, &cursor) && PtInRect (&client, cursor))
    {
        // The edge zone summons; the whole band holds it open, so the
        // pointer can travel down onto the buttons without dismissing them.
        want = m_fsTopChromeShown ? (cursor.y <= bandH)
                                  : (cursor.y <= m_scaler.ToPx (s_kStripEdgeZoneDp));
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
        m_d3dRenderer.MarkRedrawNeeded();
    }
    else if (want && !m_fsTopChromeShown)
    {
        m_fsTopChromeShown  = true;
        m_fsTopChromeAnimMs = MirroredSlideStart (nowMs, m_fsTopChromeAnimMs);
        m_d3dRenderer.MarkRedrawNeeded();
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

        m_mainMenu.Layout     (RECT{ client.left, top, client.right, top + menuH }, m_scaler);
        m_mainMenu.SetVisible (true);

        m_toolbar.Layout     (RECT{ client.left, top + menuH, client.right, top + bandH }, m_scaler);
        m_toolbar.SetVisible (true);

        if (t < 1.0f)
        {
            m_d3dRenderer.MarkRedrawNeeded();
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

void EmulatorShell::RefreshToolbarThemeList()
{
    std::vector<std::wstring>  displayNames;
    std::vector<std::string>   ids;
    std::string                activeName;
    int                        activeIndex = -1;
    int                        row         = 0;



    if (m_themeManager == nullptr)
    {
        return;
    }

    activeName = m_themeManager->GetActiveThemeName();

    for (const LoadedTheme & theme : m_themeManager->GetAvailableThemes())
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

void EmulatorShell::SyncToolbarState()
{
    ColorMode  mode       = m_colorMode.load (std::memory_order_acquire);
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

    if (m_themeManager != nullptr)
    {
        const std::string &  activeName = m_themeManager->GetActiveThemeName();

        for (const std::string & id : m_toolbarThemeIds)
        {
            if (id == activeName)
            {
                themeIndex = row;
            }

            row++;
        }
    }

    if (m_hwnd != nullptr && GetClientRect (m_hwnd, &client))
    {
        m_toolbar.SetHostClientRect (client);
    }

    m_mainMenu.GetCommands().SetMachineDisplayName (std::wstring (m_machine.GetConfig().name.begin(), m_machine.GetConfig().name.end()));
    m_switchBar.SetMachineDisplayName (std::wstring (m_machine.GetConfig().name.begin(), m_machine.GetConfig().name.end()));
    m_mainMenu.GetCommands().SetFullscreen (m_d3dRenderer.IsFullscreen());

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

void EmulatorShell::UpdateViewportLayout (int widthPx, int heightPx)
{
    HRESULT  hr           = S_OK;
    RECT     center       = {};
    RECT     viewportRect = {};



    BAIL_OUT_IF (m_viewport == nullptr, S_OK);

    // 3D desk scene (spec 018): the composition is computed for the center
    // rect (drives included -- they are scene objects now), the viewport
    // (the CRT target) becomes the projected glass rect, and the bottom band
    // collapses to the joystick row via SyncChromeBands' scene branch. The
    // settle loop is retained for the band's dock feedback.
    if (CrtMonitorActive() && m_d3dRenderer.IsFullscreen())
    {
        // Fullscreen presentation (FR-014): the glass fills the monitor with
        // a straight-on camera, every chrome band hidden -- the whole client
        // is the scene. The drive overlay strip presents the drives.
        HRESULT               hrLayout = S_OK;
        DeskSceneComposition  comp;
        RECT                  full     = { 0, 0, widthPx, heightPx };

        hrLayout = DeskSceneLayout::ComputeGlassFill (full, m_scaler.GetDpi(),
                                                      kFramebufferWidth, kFramebufferHeight,
                                                      m_deskScene.Metrics(), comp);
        BAIL_OUT_IF (hrLayout != S_OK, S_OK);

        m_deskScene.SetComposition (comp);
        m_chromeSceneScale = comp.sceneScale * s_kDeskDriveScale;
        viewportRect       = full;

        SyncSceneDriveChrome();
    }
    else if (CrtMonitorActive())
    {
        // The basename strip under the drive row is chrome, not scene, so the
        // composition is solved into a center rect short by its height and
        // the labels hang in what is left.
        int  labelStripPx = m_scaler.ToPx (s_kSceneDriveLabelStripDp + s_kSceneDriveLabelGapDp);

        for (int pass = 0; pass < s_kSceneScaleSettlePasses; pass++)
        {
            HRESULT               hrLayout = S_OK;
            DeskSceneComposition  comp;
            RECT                  sceneBox = {};

            center            = ComputeViewportRect (widthPx, heightPx);
            sceneBox          = center;
            sceneBox.bottom   = std::max (center.top, center.bottom - labelStripPx);

            hrLayout = DeskSceneLayout::Compute (sceneBox, m_scaler.GetDpi(), DeskSceneDriveCount(),
                                                 m_deskScene.Metrics(), comp,
                                                 m_scaler.ToPx (s_kSceneDriveGapDp + s_kStripEdgeZoneDp),
                                                 m_sceneView);
            BAIL_OUT_IF (hrLayout != S_OK, S_OK);

            m_deskScene.SetComposition (comp);
            m_chromeSceneScale = comp.sceneScale * s_kDeskDriveScale;

        }

        viewportRect = m_deskScene.Composition().glassRectPx;

        SyncSceneDriveChrome();
    }
    else if (DeskSceneActive() && m_d3dRenderer.IsFullscreen())
    {
        // Monitor opted out, fullscreen: still the immersive presentation --
        // every chrome band hidden and the picture filling the client (the
        // renderer letterboxes inside the target bounds), just without the
        // curved glass. The drives come from the overlay strip exactly as
        // they do with the monitor on, so the main composition holds nothing.
        m_chromeSceneScale = 1.0f;
        viewportRect       = { 0, 0, widthPx, heightPx };

        m_deskScene.SetComposition (DeskSceneComposition{});

        SyncSceneDriveChrome();
    }
    else if (DeskSceneActive())
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
        int                   pad      = m_scaler.ToPx (s_kSceneDriveRowPadDp);

        m_chromeSceneScale = 1.0f;
        center             = ComputeViewportRect (widthPx, heightPx);
        viewportRect       = center;

        band     = m_driveBand.GetBounds();
        driveRow = { pad, band.top + pad / 2, widthPx - pad,
                     std::max (band.bottom - pad - m_scaler.ToPx (s_kSceneDriveLabelStripDp +
                                                                  s_kSceneDriveLabelGapDp),
                               (LONG) band.top) };

        // A machine with no Disk ][ controller composes no row at all, and a
        // band too small to solve leaves the scene empty rather than stale.
        if (DeskSceneDriveCount() > 0)
        {
            composed = DeskSceneLayout::ComputeStrip (driveRow, m_scaler.GetDpi(), DeskSceneDriveCount(),
                                                      m_deskScene.Metrics(), comp,
                                                      DeskSceneLayout::kDriveBandGazeDownRad) == S_OK;
        }

        m_deskScene.SetComposition (composed ? comp : DeskSceneComposition{});

        SyncSceneDriveChrome();
    }
    else if (m_d3dRenderer.IsFullscreen())
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

        LayoutSceneCompass();
    }

    m_viewport->Layout (viewportRect, m_scaler);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncChromeBands
//
//  Stamps each chrome band's GetBounds() height with its current DPI-scaled
//  pixel thickness so DxuiDockLayout reads the right slab extents. Only
//  the docked axis (height, for the Top/Bottom bands) is meaningful; the
//  bands are never painted.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncChromeBands()
{
    ChromeBandInputs     inputs;
    ChromeBandHeightsPx  px;



    // Which bands exist, and how tall, is ChromeBandLayout's to decide from
    // what the machine has and how it is shown; this reads those facts off
    // the shell and stamps the answers onto the bands' docked heights.
    inputs.hasDiskController   = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();
    inputs.crtMonitorActive    = CrtMonitorActive();
    inputs.hasCaseSwitches     = MachineHasCaseSwitches();
    inputs.driveBarThicknessDp = m_driveBarThicknessDp;
    inputs.chromeSceneScale    = m_chromeSceneScale;
    inputs.toolbarBandDp       = m_toolbar.GetBandDp();

    // Measured against the CLIENT width, which is what the band will be given.
    // Measuring against the viewport is what put the text off the edge: the
    // picture keeps its own aspect and can be wider than the window.
    inputs.changeBandPx        = GetChangeBandThicknessPx (m_lastClientWidthPx);
    inputs.captureBandPx       = GetCaptureBandThicknessPx (m_lastClientWidthPx);

    px = ChromeBandLayout::Compute (inputs, m_scaler);

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
//  EmulatorShell::CollectDockedBands
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

void EmulatorShell::CollectDockedBands (IDxuiControl * (& outBands)[kDockedBandCount])
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
//  EmulatorShell::ComputeViewportRect
//
//  Docks the chrome bands (title + nav on top, drive on the bottom)
//  around a Fill center over the client rect and returns the center
//  (emulator viewport) rect the dock leaves in the middle.
//
////////////////////////////////////////////////////////////////////////////////

RECT EmulatorShell::ComputeViewportRect (int widthPx, int heightPx)
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
    m_toolbar.PlanForWidth (widthPx, m_scaler);

    //  The notice's height depends on the width it is about to be given, and
    //  SyncChromeBands is where every band's thickness is decided -- so the
    //  width has to be known before it runs.
    m_lastClientWidthPx = widthPx;

    SyncChromeBands();
    m_chromeDock.Arrange (RECT{ 0, 0, widthPx, heightPx }, m_scaler, kids);

    // The command toolbar rides its band: re-lay it every viewport pass so a
    // resize / DPI change reflows the buttons with the strip.
    //
    // EXCEPT IN FULLSCREEN, where the reveal overlay owns it. Both were
    // laying it out -- the dock into a band the fullscreen viewport does not
    // show, the overlay across the top -- so whichever ran last won, and the
    // buttons painted at one height while their hit rects sat at the other.
    // A single owner per presentation, or they disagree.
    if (!m_d3dRenderer.IsFullscreen())
    {
        m_toolbar.Layout (m_toolbarBand.GetBounds(), m_scaler);
    }

    //  The notice rides its band the way the toolbar rides its own, so a
    //  resize or a DPI change reflows it with everything else.
    LayoutChangeBanner();

    return m_centerBand.GetBounds();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReflowChromeForMachineChange
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

void EmulatorShell::ReflowChromeForMachineChange()
{
    RECT  rcWindow      = {};
    bool  haveWindow    = false;
    bool  newHasDisk    = false;
    bool  newIsApple2c  = false;
    bool  layoutChanged = false;
    bool  didResize     = false;



    DXUI_ASSERT_UI_THREAD();   // chrome layout: never from the CPU thread

    haveWindow = m_hwnd != nullptr && GetWindowRect (m_hwnd, &rcWindow);

    if (haveWindow)
    {
        newHasDisk    = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();
        newIsApple2c  = MachineHasCaseSwitches();
        layoutChanged = (newHasDisk != m_chromeSizedForHasDisk) ||
                        (newIsApple2c != m_chromeSizedForApple2c);
    }

    // The desk wears what the machine wore, so crossing the //c boundary
    // swaps both models. Reloading rebuilds every cached mesh, so the
    // scene's own state is pushed again right after.
    if (m_deskSceneReady && MachineHasCaseSwitches() != m_deskSceneMachineIsC)
    {
        HRESULT  hrModels = LoadDeskSceneModelsForMachine();

        if (SUCCEEDED (hrModels))
        {
            m_deskScene.SetPowerLampOn (true);
        }

        IGNORE_RETURN_VALUE (hrModels, S_OK);
    }

    // Resize the window by the total bottom-band delta -- the drive band
    // (disk-presence) plus the //c switch band -- but not for min/max/fullscreen
    // windows, where the user explicitly chose the size (mirrors
    // ApplyThemeToChrome). Those just relayout inside the fixed frame.
    if (haveWindow && layoutChanged &&
        !IsIconic (m_hwnd) && !IsZoomed (m_hwnd) && !m_d3dRenderer.IsFullscreen())
    {
        int  oldDriveDp  = m_chromeSizedForHasDisk ? m_driveBarThicknessDp : 0;
        int  newDriveDp  = newHasDisk              ? m_driveBarThicknessDp : 0;
        int  oldSwitchDp = m_chromeSizedForApple2c ? ChromeBandLayout::kSwitchBandDp : 0;
        int  newSwitchDp = newIsApple2c            ? ChromeBandLayout::kSwitchBandDp : 0;
        int  deltaPx     = (m_scaler.ToPx (newDriveDp)  - m_scaler.ToPx (oldDriveDp)) +
                           (m_scaler.ToPx (newSwitchDp) - m_scaler.ToPx (oldSwitchDp));

        m_chromeSizedForHasDisk = newHasDisk;
        m_chromeSizedForApple2c = newIsApple2c;

        // The bands are bottom-docked full-width, so only the height moves.
        // SWP_NOMOVE pins the top-left corner; the WM_SIZE it generates drives
        // OnSize to re-lay the bands, widgets, and hit-test map.
        SetWindowPos (m_hwnd, nullptr, 0, 0,
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

        if (GetClientRect (m_hwnd, &rcClient) &&
            rcClient.right > rcClient.left && rcClient.bottom > rcClient.top)
        {
            (void) OnSize (static_cast<UINT> (rcClient.right  - rcClient.left),
                           static_cast<UINT> (rcClient.bottom - rcClient.top));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShouldShowExternalDrive
//
//  The //c's second drive is an optional external unit that plugs into the
//  disk port, so it appears only when the user has marked it connected
//  (Hardware tab toggle -> $cassoUiPrefs.externalDriveConnected). The //c is
//  the only machine with a banked system ROM, so romBankSize is the
//  discriminator -- the same signal that gates the built-in IWM drive.
//
//  Everywhere else the second drive is whatever is attached to the Disk ][
//  card's second connector. That used to be unconditionally true, on the
//  reasoning that the card is two-drive hardware -- but the CARD having two
//  connectors was never the same claim as both of them having a drive on the
//  end, and this is the question the 2D widgets and the desk scene both ask,
//  so answering it from the config is what keeps them agreeing.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::ShouldShowExternalDrive() const
{
    bool  externalIsOptional = (m_machine.GetConfig().systemRom.romBankSize != 0);



    if (externalIsOptional)
    {
        return m_externalDriveConnected;
    }

    return m_machine.GetConfig().AttachedDiskIiDriveCount() >= kDiskIiPortCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplyThemeToChrome
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

void EmulatorShell::ApplyThemeToChrome (const CassoTheme & theme)
{
    // Bottom drive-bar thickness, full and compact. Layout: drive widget
    // (body + label strip + 2 dp bottom margin) bottom-anchored under an
    // 8 dp gap. Drive widget total height is body 160 + label-strip gap 2 +
    // label strip 18 = 180 dp (full) / 60 dp (compact). With the desk scene
    // on, SyncChromeBands scales the band by m_chromeSceneScale
    // (s_kDeskDriveScale at 100%), so it hugs the scaled drives without a
    // separate constant.
    constexpr int  s_kFullDriveBarDp    = 190;
    constexpr int  s_kCompactDriveBarDp = 70;



    int   desiredThicknessDp = theme.compactDrives ? s_kCompactDriveBarDp : s_kFullDriveBarDp;
    int   priorThicknessDp   = m_driveBarThicknessDp;
    RECT  rcClient           = {};
    RECT  rcWindow           = {};
    int   centerW            = 0;
    int   centerH            = 0;
    bool  canResize          = false;



    m_driveChrome[0].SetCompact (theme.compactDrives);
    m_driveChrome[1].SetCompact (theme.compactDrives);

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

    // A balloon that is already up was sized and cleared with the outgoing
    // colors, and nothing repaints its background. Take it down; the next
    // hover raises it in the new palette.
    m_toolbarTooltip.HideImmediate();
    m_switchBarTooltip.HideImmediate();
    m_driveTooltip.HideImmediate();

    // Every path applies the new thickness; only the window resize is
    // conditional. Min/max/fullscreen windows are skipped because the user
    // explicitly chose that state and should not see the window resize from
    // under them on a theme swap -- the thickness still lands, so the next
    // normal-state resize uses the right math. Short-circuit order matters:
    // the Is* / Get* calls must not run on a null HWND.
    canResize = m_hwnd != nullptr
                && desiredThicknessDp != priorThicknessDp
                && !IsIconic (m_hwnd)
                && !IsZoomed (m_hwnd)
                && !m_d3dRenderer.IsFullscreen()
                && GetClientRect (m_hwnd, &rcClient)
                && GetWindowRect (m_hwnd, &rcWindow);

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

        SetWindowPos (m_hwnd, nullptr, 0, 0, newWindowW, newWindowH,
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
    if (m_hwnd != nullptr && GetClientRect (m_hwnd, &rcClient))
    {
        (void) OnSize ((UINT) (rcClient.right - rcClient.left),
                       (UINT) (rcClient.bottom - rcClient.top));
        m_d3dRenderer.MarkRedrawNeeded();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetCrtMonitorEnabled
//
//  Settings > Theme opt in/out for the CRT monitor -- the escape hatch back to
//  the flat picture at classic sizes (the 3D drives stay either way).
//  Persists the choice, then re-runs the authoritative OnSize layout at the
//  current client size so the monitor appears or disappears in place -- the
//  window itself does not resize; Ctrl+0 reaches the mode's 100% default.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetCrtMonitorEnabled (bool enabled)
{
    HRESULT  hr       = S_OK;
    RECT     rcClient = {};



    BAIL_OUT_IF (m_globalPrefs.crtMonitor == enabled, S_OK);

    m_globalPrefs.crtMonitor = enabled;

    if (m_userConfigStore != nullptr)
    {
        hr = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);
    }
    else
    {
        hr = m_globalPrefs.Save (m_machine.GetAssetBaseDir(), m_uiFs);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_hwnd != nullptr && GetClientRect (m_hwnd, &rcClient))
    {
        (void) OnSize ((UINT) (rcClient.right - rcClient.left),
                       (UINT) (rcClient.bottom - rcClient.top));
        m_d3dRenderer.MarkRedrawNeeded();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LayoutSwitchBar
//
//  Positions the //c case-switch strip over its chrome band. On any other
//  machine the band is zero-height, so the strip is hidden (and un-hit-tested).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutSwitchBar (UINT dpi)
{
    DxuiDpiScaler  scaler;



    if (!MachineHasCaseSwitches())
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
//  EmulatorShell::SyncSwitchBarState
//
//  Pushes the live //c switch + indicator state onto the strip: the two
//  latching switches are read back from the keyboard device (single source of
//  truth), the disk-use LED tracks slot-6 drive activity, power is always lit
//  while the machine runs, and the reset button is "armed" only while Ctrl is
//  held (real Control-Reset).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSwitchBarState()
{
    Apple2eKeyboard *  iieKbd = m_machine.GetRefs().iieKeyboard;
    bool               diskOn = false;



    if (iieKbd != nullptr)
    {
        m_switchBar.SetEightyFortyIn (iieKbd->IsEightyColumnSwitchIn());
        m_switchBar.SetKeyboardIn    (iieKbd->IsKeyboardSwitchDvorak());
    }

    for (const DriveWidget & drive : m_driveChrome)
    {
        diskOn = diskOn || (drive.GetLed() == LedState::Active);
    }

    m_switchBar.SetDiskActive (diskOn);
    m_switchBar.SetPowerOn    (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HandleSwitchBarClick
//
//  Actions a left-button release over one of the //c switch-strip parts. The
//  reset button is inert unless Ctrl is held (the real //c key does nothing on
//  its own); Open-Apple / Closed-Apple, mapped from the held Alt keys, ride the
//  reset into the firmware for a cold boot / diagnostics. The 80/40 and
//  keyboard switches latch: each click flips the switch state on the keyboard
//  device, which the strip re-reads on the next SyncSwitchBarState.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HandleSwitchBarClick (Apple2cSwitchBar::Part part)
{
    // The devices are read here on the UI thread; a machine switch on the
    // CPU thread holds the lifetime lock exclusively while it replaces
    // them, and a click that lands in that window is dropped.
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);
    Apple2eKeyboard *                    iieKbd = nullptr;



    if (!lifetime.owns_lock())
    {
        return;
    }

    iieKbd = m_machine.GetRefs().iieKeyboard;

    switch (part)
    {
        case Apple2cSwitchBar::Part::Reset:
            // Only a modifier-qualified press resets, matching the case key.
            if ((GetKeyState (VK_CONTROL) & 0x8000) != 0)
            {
                if (m_machine.GetRefs().keyboard != nullptr)
                {
                    m_machine.GetRefs().keyboard->SetKeyDown (false);
                }

                RequestReset();
            }

            break;

        case Apple2cSwitchBar::Part::EightyForty:
            if (iieKbd != nullptr)
            {
                bool  newIn = !iieKbd->IsEightyColumnSwitchIn();

                iieKbd->SetEightyColumnSwitchIn (newIn);
                PersistSwitchState ("eightyColumnSwitch", newIn);
            }

            break;

        case Apple2cSwitchBar::Part::Keyboard:
            if (iieKbd != nullptr)
            {
                bool  newDvorak = !iieKbd->IsKeyboardSwitchDvorak();

                iieKbd->SetKeyboardSwitchDvorak (newDvorak);
                PersistSwitchState ("keyboardDvorak", newDvorak);
            }

            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetCaptureBandThicknessPx
//
//  How tall the stand-in bar's band is.
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

int EmulatorShell::GetCaptureBandThicknessPx (int clientWidthPx) const
{
    IDxuiTextRenderer *  text    = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    std::wstring         line    = GetStandInBannerText();
    DxuiInfoBanner       measure (line);



    if (line.empty() || m_d3dRenderer.IsFullscreen() || clientWidthPx <= 0)
    {
        return 0;
    }

    measure.SetCentered (true);
    measure.SetDpi      (m_scaler.GetDpi());

    //  MEASURED WHEN THERE IS A RENDERER TO ASK, because the bar centers its
    //  text and a centered banner picks its line width from that measurement.
    //  The estimate behind GetPreferredHeightPx is an AVERAGE glyph width: a
    //  wide face measures past it, and a height taken from it would reserve a
    //  line fewer than the paint lays down. The estimate stays as the fallback
    //  for the moments before the renderer exists.
    if (text != nullptr)
    {
        return (int) measure.GetMeasuredHeightPx (*text, (float) clientWidthPx, m_scaler);
    }

    return (int) measure.GetPreferredHeightPx ((float) clientWidthPx, m_scaler);
}
