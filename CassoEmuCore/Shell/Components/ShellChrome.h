#pragma once

#include "Pch.h"

#include "Ui/Chrome/Apple2cSwitchBar.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Chrome/ChromeBand.h"
#include "Ui/Chrome/MainMenu.h"
#include "Ui/Chrome/VolumeFlyout.h"



class DriveWidget;
class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome
//
//  Everything the window draws around the picture: the main menu, the
//  command toolbar and its volume flyout, the chrome bands and the dock that
//  arranges them around the emulator viewport, the tooltips, the pointer-
//  capture bar, the transient notices, the //c case-switch strip, the frame
//  rate readout and the fullscreen top-edge reveal, and the chrome theme
//  they are all painted in.
//
//  The window's message handlers -- OnSize above all, which is the one
//  authoritative layout pass -- still read and write the band state
//  directly, so the shell is a friend.
//
////////////////////////////////////////////////////////////////////////////////

class ShellChrome
{
    friend class EmulatorShell;

public:
    explicit ShellChrome (EmulatorShell & shell);
    ~ShellChrome();

    // The chrome theme everything is painted in.
    CassoTheme &  GetTheme () { return m_chromeTheme; }

    // The caption buttons' tooltip, which the update indicator in the caption
    // shares.
    DxuiTooltip &  GetCaptionTooltip () { return m_captionTooltip; }
    //  How many bands peel an edge off the client area before the emulator
    //  viewport gets the rest. Named so the two directions that walk them
    //  cannot size their arrays differently.
    static constexpr int  kDockedBandCount = 7;

    void  CollectDockedBands (class IDxuiControl * (& outBands)[kDockedBandCount]);

    // Drives the host's root panel layout for the Apple ][ viewport
    // child. Computes the framebuffer rectangle (client minus chrome
    // bands) via the DxuiDockLayout and invokes m_viewport->Layout,
    // which fires OnViewportBoundsChanged when the rectangle differs
    // from the last value reported.
    void    UpdateViewportLayout          (int widthPx, int heightPx);

    // Chrome-band sizing via DxuiDockLayout (replaces LayoutManager).
    // SyncChromeBands stamps each band's GetBounds() with its DPI-scaled
    // pixel thickness. ComputeViewportRect docks the bands + center and
    // returns the middle (emulator viewport) rect. GetClientSizeForCenterPx
    // is the inverse: given a desired center size in px, the client size
    // that hosts it. GetClientSizeForFramebufferPx DPI-scales a DIP
    // framebuffer grid first, then adds the chrome insets.
    void    SyncChromeBands               ();
    RECT    ComputeViewportRect           (int widthPx, int heightPx);

    // Re-run the chrome layout at the current client size after a machine
    // switch: adding/removing the Disk ][ controller changes the drive band +
    // widgets + hit-test map, but no WM_SIZE fires when the window size itself
    // is unchanged, so OnSize would never re-evaluate it. See the
    // WM_APP_DXUI_UPDATE_TITLE handler (the switch-completion signal).
    void    ReflowChromeForMachineChange  ();

    // How tall the bar is at a given width, for the band that reserves the
    // room and the paint that fills it. One answer, so the two cannot
    // disagree about whether the band is big enough.
    int           GetStandInBarHeightPx (float widthPx) const;

    // Lays the flat drive row (and the recorder after it) into the drive band.
    // visibleCount is how many drives will be SHOWN, which is not always the
    // array size: the row is centered on that, so a //c with no external
    // drive centers its one drive rather than leaving a gap where the second
    // would have been.
    void         LayoutDriveWidgetsInCommandBar (
        std::array<DriveWidget, 2>  & driveChrome,
        int                           bottomInsetPx,
        int                           clientW,
        int                           clientH,
        UINT                          dpi,
        float                         sceneScale,
        int                           visibleCount);

    void    LayoutSwitchBar        (UINT dpi);
    void    SyncSwitchBarState     ();
    void    HandleSwitchBarClick   (Apple2cSwitchBar::Part part);

    // Pushes a freshly-activated CassoTheme into the layout-affecting
    // chrome state: drive bar thickness, per-drive compact flag, and
    // (if the bottom inset changed) a window resize that preserves the
    // emulator pixel grid. Called from the ThemeManager listener.
    void    ApplyThemeToChrome    (const CassoTheme & theme);

    // Settings > Theme opt in/out for the CRT monitor. Applies live -- relays
    // out the chrome in place -- and persists to GlobalUserPrefs.
    void    SetCrtMonitorEnabled (bool enabled);

    // Fullscreen presentation (FR-014): every chrome element collapses to
    // nothing -- host caption, menu bar, toolbar, joystick row, drive band,
    // //c switch strip -- so the glass-fill scene owns the whole client.
    void    SetChromeHiddenForFullscreenScene (bool hidden);

    // The pointer-capture banner and the fullscreen top-edge chrome reveal,
    // both driven from the per-frame UI upkeep.
    void    SyncStandInBanner    ();
    void    SyncFrameRateReadout ();

    void    TickFullscreenTopChrome();

    // How tall the capture bar's band is right now: zero unless the pointer is
    // held, and zero in fullscreen, where there are no bands at all and the
    // bar rides under the toolbar reveal instead.
    int     GetCaptureBandThicknessPx (int clientWidthPx) const;

    // Re-docks the chrome after the notice's band appears or goes.
    //
    // IT DOES NOT RESIZE THE WINDOW, unlike the machine-change reflow beside
    // it. A notice is transient and the user did not ask for a bigger window
    // to hold it: the picture gives up the height and takes it back.
    void    ReflowChromeForChangeBand ();

    void  RefreshToolbarThemeList          ();
    void  SyncToolbarState                 ();

    void  ShowNotice   (const std::wstring & text);
    void  PostNotice   (const std::wstring & text);
    void  SyncNotice   ();

    // The lowest edge of whatever chrome is docked (or, in fullscreen,
    // revealed) at the top of the client, which is where an overlay that
    // wants to sit over the picture without covering the chrome begins.
    LONG  ComputeTopOverlayEdgePx (const RECT & client) const;

    // Hides (or restores) the overlays that describe the application rather
    // than the machine, for the duration of a capture paint.
    void  SetStandInOverlaysHidden (bool hidden);

    // Chrome layout via DxuiDockLayout. The three bands carry the title
    // bar, nav strip, and drive bar pixel thicknesses in their GetBounds();
    // m_centerBand (Fill) captures the emulator viewport rect the dock
    // leaves in the middle. m_driveBarThicknessDp is the live drive-bar
    // thickness the theme mutates (compact vs full).
    // The fixed band metrics -- title bar, nav strip, //c switch strip -- are
    // ChromeBandLayout's, with the rule for when each band collapses to zero.
    // (The command toolbar band's thickness comes from m_toolbar.GetBandDp() --
    // it varies with the responsive mode planned for the window width.)
    static constexpr int  s_kInitialDriveBandDp = 256;

    // How long after that dock a cancel is still credibly its echo. Long
    // enough to cover a settle pass on a slow frame, short enough that a real
    // takeover arriving later is never mistaken for it.
    static constexpr int64_t s_kCaptureReflowEchoMs = 750;

private:
    EmulatorShell  & m_shell;

    // Chrome surfaces. MainMenu owns the parity table for legacy IDM_*
    // commands and runs alongside the existing Win32 menu bar until the
    // painter retires the latter. The caption (title + icon + min/max/
    // close) is owned and rendered by the DxuiHwndSource, not here.
    MainMenu                    m_mainMenu;
    CassoTheme                  m_chromeTheme   = CassoTheme::MakeSkeuomorphic();

    // The command toolbar: the strip below the menu bar with Settings /
    // theme + monitor-color pickers / Printer (+status LED) / master Volume
    // + Mute / Input / Fullscreen / Screenshot / Reset / Power, filled from
    // the same command table the menu bar reads. The three emulator parts
    // it hosts -- the printer light, the input cluster and the volume
    // flyout -- are held by pointer from its entries. The light is the
    // printer component's.
    DxuiToolbar         m_toolbar;
    VolumeFlyout        m_volumeFlyout;

    // Theme ids in the toolbar picker's row order, so a picked row resolves
    // to the id ThemeManager wants. Rebuilt whenever the catalog is.
    std::vector<std::string>  m_toolbarThemeIds;

    // Desk-scene zoom: the monitor's SceneScale from the last layout. The
    // drive widgets and the (scaled part of the) drive band follow it so the
    // whole scene zooms together when the window resizes. 1.0 for compact
    // themes and at the 100%-zoom default window size.
    float                      m_chromeSceneScale = 1.0f;

    DxuiTooltip          m_toolbarTooltip;   // labels for the toolbar's icon-only mode

    // Apple //c case-switch strip (reset button + 80/40 and keyboard latching
    // switches + disk-use / power LEDs), painted in its own chrome band between
    // the emulator viewport and the drive bar. Present only on the //c; its
    // band collapses to zero height on every other machine. Manually
    // hit-tested / actioned by the mouse handlers, like the other chrome.
    Apple2cSwitchBar  m_switchBar;
    DxuiTooltip       m_switchBarTooltip;

    // Hover tooltip for the drive widgets, surfaced when the pointer
    // rests over a write-protected drive. Explains that the disk is
    // write-protected and names the source(s) -- image flag, user
    // setting, or an unwritable backing file. Shares the host popup pool
    // with the other chrome tooltips (the hover regions are mutually exclusive).
    DxuiTooltip               m_driveTooltip;

    // Hover tooltip for the caption's minimize, maximize and close buttons.
    // The stock system tooltip for those is suppressed at the host, so this
    // is the only one, and it matches every other tooltip in the window.
    DxuiTooltip               m_captionTooltip;

    // Solid background for the bottom drive-bar band. The CRT composite
    // writes the whole back buffer (emulator frame + black), so the chrome
    // bands need an opaque surface painted on top; the title and menu bars
    // cover their own bands, this covers the drive bar.
    DxuiSurface           m_driveBandSurface;

    // "Press Esc to release the mouse and exit paddle mode", on screen for as
    // long as the capture holds. The joystick button carries the same words, but
    // it is chrome: fullscreen hides it, and a captured pointer with the
    // cursor gone and no way out shown is how a user ends up killing the
    // process. A message bar rather than a caption over the picture: it says
    // something and asks nothing, which is what an info banner is, and the
    // chrome under the command strip is the one place it covers nothing.
    DxuiInfoBanner             m_standInBar;

    // An opaque panel behind it, the way the drive bar has one. The banner's
    // own fill is a tint meant to sit on chrome, and the bar does not: it
    // hangs over the picture, and over the desk scene the monitor read
    // straight through the words.
    DxuiSurface                m_standInBarSurface;

    // The transient notices: a screenshot's filename or the reason it failed,
    // which write-protect mechanism a Disk menu command changed, a controller
    // that left. Their own bars rather than the mouse-capture one's, because
    // the two can be wanted at once and these expire on a timer while that one
    // tracks a state. Several can be up at once, stacked in arrival order,
    // each for its own full time.
    //
    // MESSAGE BARS ACROSS THE TOP, NOT A CAPTION ON THE PICTURE. The notice
    // was shadowed text over the bottom of the viewport, which put a filename
    // -- the one thing here that is never about the machine -- in the middle
    // of the photograph. It now reads as the same kind of thing the
    // pointer-capture bar is, and says so by looking like it.
    //
    // AN OVERLAY, THOUGH, WHERE THAT ONE DOCKS. The stack hangs under whatever
    // docked chrome is at the top and covers a little of the picture instead.
    DxuiNoticeStack                m_notices;

    // The frames-per-second readout. Shadowed rather than a notice: it
    // wants a corner, not the centered band a notification takes.
    DxuiShadowedText           m_fpsReadout;

    // The fullscreen menu-bar-and-toolbar reveal, the drive strip's bargain
    // mirrored along the top edge: shown while the pointer is up there,
    // hidden once it leaves and the grace expires.
    bool                       m_fsTopChromeShown  = false;
    int64_t                    m_fsTopChromeLeftMs = 0;
    int64_t                    m_fsTopChromeAnimMs = 0;   // slide start

    DxuiDockLayout           m_chromeDock;
    ChromeBand               m_titleBand;
    ChromeBand               m_navBand;
    ChromeBand               m_toolbarBand;

    // The client width the bands were last laid out for, so the notice's
    // height can be measured against the width it is about to be given.
    int                      m_lastClientWidthPx = 0;

    // The external-change notice's own band, docked under the toolbar.
    //
    // A BAND RATHER THAN AN OVERLAY, and the difference is not cosmetic. Drawn
    // over the viewport it covered the top of the picture, took its width from
    // a rect that follows the emulator's aspect rather than the window, and ran
    // its text and its action off the client edge. As a band the dock gives it
    // the client width, the Fill center shrinks by exactly its height, and the
    // scene rescales into what is left -- the same way the //c switch strip and
    // the drive bar already work.
    //
    // Zero height when nothing is being reported, so every other machine and
    // every quiet session is laid out exactly as before.
    ChromeBand               m_changeBand;

    // The capture bar's own band, docked directly under the change notice so
    // both sit below the command strip. Zero height whenever the pointer is
    // not held, which is every ordinary session.
    ChromeBand               m_standInBand;

    // Whether the one authoritative layout pass (OnSize) is running, so a
    // notice band cannot ask for another from inside it. See
    // ReflowChromeForChangeBand.
    bool                     m_inChromeLayout = false;

    // Set when a capture band was found standing with no capture behind it,
    // and cleared by the re-dock at the top of the next frame. A flag rather
    // than the re-dock itself, because the sync that spots it runs inside the
    // frame the re-dock would repaint.
    bool                     m_standInBandStale = false;

    // When the capture's own band was last docked, on the monotonic clock.
    // The resize that follows bounces WM_CANCELMODE back at whoever holds the
    // pointer, and that one cancel is ours to ignore -- see OnCancelMode.
    // Zeroed once it has been used, so exactly one is ever swallowed.
    int64_t                  m_captureReflowMs = 0;

    ChromeBand               m_driveBand;
    ChromeBand               m_switchBand;
    ChromeBand               m_centerBand;
    int                      m_driveBarThicknessDp = s_kInitialDriveBandDp;

    // Whether the current WINDOW height was sized for a Disk ][ controller
    // being present. Written by OnSize (the authoritative layout, WM_SIZE-only)
    // to the disk-presence it just laid out; ReflowChromeForMachineChange reads
    // this pre-switch value to grow/shrink the window by the drive-band delta
    // (so the viewport keeps its size + the top-left stays put) rather than
    // re-centering inside a fixed window.
    bool                     m_chromeSizedForHasDisk = true;

    // Companion to m_chromeSizedForHasDisk for the //c switch band: whether the
    // current WINDOW height was sized with the switch strip present. Recorded by
    // OnSize; ReflowChromeForMachineChange folds the switch-band delta into the
    // window resize so switching to / from the //c keeps the viewport its size.
    bool                     m_chromeSizedForApple2c = false;
};
