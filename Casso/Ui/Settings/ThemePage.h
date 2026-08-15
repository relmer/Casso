#pragma once

#include "Pch.h"

#include "../Chrome/CassoTheme.h"
#include "../Chrome/ChromeMetrics.h"
#include "../Chrome/DriveWidget.h"
#include "../Chrome/JoystickToggleButton.h"
#include "../Chrome/MainMenu.h"
#include "Window/DxuiCaptionBar.h"
#include "../IDriveCommandSink.h"
#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"


class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage
//
//  Lets the user pick the active chrome theme. The page itself is
//  view-only: it surfaces the list of discovered themes and reports
//  selection changes through a callback the panel host wires into
//  EmulatorShell so the active theme can be activated and persisted.
//
////////////////////////////////////////////////////////////////////////////////

class ThemePage : public DxuiPropertyPage
{
public:
    explicit ThemePage (std::wstring title = L"Theme");

    using  ThemeSelectFn       = std::function<void (const std::string & themeName)>;
    using  FramebufferSourceFn = std::function<const uint32_t * (int & outWidthPx, int & outHeightPx)>;
    using  MountedPathFn       = std::function<std::wstring (int driveIndex)>;
    using  WriteProtectFn      = std::function<WriteProtectInfo (int driveIndex)>;
    using  HasDiskSourceFn     = std::function<bool ()>;

    void  SetThemes             (std::vector<std::string>  themeIds,
                                 std::vector<std::wstring> displayNames,
                                 int                       activeIndex);
    void  SetOnThemeSelected    (ThemeSelectFn       fn) { m_onThemeSelected   = std::move (fn); }
    void  SetFramebufferSource  (FramebufferSourceFn fn) { m_framebufferSource = std::move (fn); }
    void  SetMountedPathSource  (MountedPathFn       fn) { m_mountedPathSource = std::move (fn); }
    void  SetWriteProtectSource (WriteProtectFn      fn) { m_writeProtectSource = std::move (fn); }

    // Reports whether the staged machine config has an ENABLED Disk ][
    // controller. When it returns false the preview drops the drive widgets
    // and collapses the drive bar to the joystick band, mirroring the live
    // chrome (Phase D). Live-tracks the staged toggle (#84 Phase C). Absent =>
    // assume present (back-compat).
    void  SetHasDiskSource      (HasDiskSourceFn     fn) { m_hasDiskSource     = std::move (fn); }

    // FR-132: fired when the user clicks the "Apply now" button next to
    // the theme dropdown. The panel host wires this to live-apply the
    // currently-selected theme without closing the dialog.
    using ApplyThemeNowFn = std::function<void ()>;
    void  SetOnApplyThemeNow    (ApplyThemeNowFn     fn) { m_onApplyThemeNow   = std::move (fn); }

    // The CRT monitor opt in/out (the monitor alone -- the 3D drives always
    // render). The checkbox applies live through the callback and is enabled
    // only while a skeuomorphic (non-compact) theme is selected.
    using CrtMonitorFn = std::function<void (bool enabled)>;
    void  SetOnCrtMonitorToggled (CrtMonitorFn fn) { m_onCrtMonitorToggled = std::move (fn); }
    void  SetCrtMonitorChecked   (bool checked)   { m_crtMonitorCheckbox.SetChecked (checked); }

    // The theme id the dropdown currently shows (may differ from the id
    // active at open once the user has changed the selection). Empty if
    // no themes are loaded.
    std::string  SelectedThemeId () const;

    // Routes the owned theme dropdown's popup menu through the host's
    // popup-host pool so the menu HWND escapes the page's clipping
    // bounds. Pass nullptr to revert to the in-panel PaintMenu path.
    void  SetPopupHost          (DxuiHwndSource * host) { m_themeDropdown.SetPopupHost (host); }

    void  Layout                (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Virtual paint override. Beyond the fanned-out label + dropdown, this
    // page draws a bespoke theme-preview window (mock chrome + framebuffer)
    // between the dropdown box and its popup menu, which the inherited
    // DxuiPanel auto-fan-out walk cannot supply -- so ThemePage overrides
    // Paint (like DisplayPage) rather than relying on the base fan-out.
    // The host paints via the concrete DxuiPainter / DxuiTextRenderer,
    // recovered by a downcast inside the definition.
    void  Paint                 (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    DxuiDropdown                       & ThemeDropdown    ()       { return m_themeDropdown; }
    const DxuiDropdown                 & ThemeDropdown    () const { return m_themeDropdown; }
    const std::vector<std::string> & Themes           () const { return m_themeIds; }
    int                              ActiveThemeIndex () const { return m_activeIndex; }

private:
    // Row/preview geometry and the painting helpers that consume it. Every
    // reader is a ThemePage method, so the block belongs to the class.
    static constexpr int  kRowHeightDp     = 28;
    static constexpr int  kLabelWidthDp    = 140;
    static constexpr int  kDropdownWidthDp = 220;
    static constexpr int  kPagePadDp       = 16;

    static constexpr int  kPrevFbWidthDp       = ChromeMetrics::kFramebufferWidthPx;   // 560
    static constexpr int  kPrevFbHeightDp      = ChromeMetrics::kFramebufferHeightPx;  // 384
    static constexpr int  kPrevTitleBarDp      = 32;
    static constexpr int  kPrevNavStripDp      = 32;
    static constexpr int  kPrevDriveBarFullDp  = 225;
    static constexpr int  kPrevDriveBarCmptDp  = 105;
    static constexpr int  kPrevJoystickBandDp  = 43;
    // No system-button or caption/menu font metrics here any more: the real
    // DxuiCaptionBar and MainMenu carry their own, and a second set of
    // numbers describing the same chrome is precisely how the mock drifted
    // out of step with it.

    static RECT      MakeRect (int l, int t, int w, int h);


    // Computes the actual preview rect inside availRect that matches
    // the 100%-zoom window's aspect ratio (which depends on the bottom
    // inset -- full/compact drive bar when a controller is present, or the
    // thin joystick band alone when it isn't). Returns the scale factor used
    // so the caller can size sub-regions consistently.
    static void  ComputePreviewGeometry (const RECT  & availRect,
                                         int           driveBandDp,
                                         RECT        & outPrevRect,
                                         float       & outScale);

public:
    // What the 3D desk scene should draw over the mock window, decided during
    // the 2D paint and consumed by the sheet's after-paint hook. The preview
    // cannot draw the scene itself: it paints through the 2D painter, and the
    // scene needs the render target after the panel tree is done with it.
    //
    // Skeuo themes hand the picture and drives to the scene exactly as the
    // live chrome does, so the 2D paint LEAVES THOSE OUT rather than drawing
    // underneath -- the mock's flat screen rect and flat drive widgets are
    // what made the preview keep showing a presentation the app retired.
    enum class PreviewSceneMode
    {
        None,        // compact theme: the flat mock is the whole preview
        Full,        // monitor and drives, the CRT checkbox on
        DrivesOnly,  // CRT opted out: flat picture, 3D drives beneath it
    };

    struct PreviewSceneRequest
    {
        PreviewSceneMode  mode   = PreviewSceneMode::None;
        RECT              rectPx = {};   // where the scene composes
        RECT              clipPx = {};   // and where it must stop

        // The mock's own reduced DPI, which the 2D widgets already scale by.
        // It does NOT set the scene's size -- the layout contain-fits to the
        // viewport and never reads it -- it only scales the dp reservations
        // the layout makes inside that fit.
        UINT              dpi    = 96;
    };

    // Consume-once: the sheet takes the request and leaves it empty. A page
    // that did not paint this frame therefore asks for nothing, which is what
    // stops the scene from staying on screen after the user switches tabs.
    PreviewSceneRequest  TakeSceneRequest () const
    {
        PreviewSceneRequest  taken = m_sceneRequest;

        m_sceneRequest = {};

        return taken;
    }

    // The live framebuffer, for the scene's glass -- the same source the flat
    // preview blit uses. Null when the shell has not supplied one.
    const uint32_t *  FramebufferPixels (int & outW, int & outH) const
    { return m_framebufferSource ? m_framebufferSource (outW, outH) : nullptr; }

private:
    static void  PaintPreviewWindow (DxuiPainter                          & painter,
                                     DxuiTextRenderer                     & text,
                                     const RECT                           & availRect,
                                     const CassoTheme                     & theme,
                                     bool                                   hasDisk,
                                     const std::function<const uint32_t * (int &, int &)> & framebufferSource,
                                     const std::function<std::wstring (int)>              & mountedPathSource,
                                     const std::function<WriteProtectInfo (int)>          & writeProtectSource,
                                     std::array<DriveWidget, 2>           & previewDrives,
                                     JoystickToggleButton                 & previewButton,
                                     DxuiCaptionBar                       & previewCaption,
                                     MainMenu                             & previewMenu,
                                     bool                                 & chromeConfigured,
                                     bool                                   crtMonitor,
                                     PreviewSceneRequest                  & outScene);

    std::vector<std::string>      m_themeIds;
    int                           m_activeIndex = -1;
    // Enables the CRT-monitor checkbox only while the selected theme is
    // skeuomorphic (compact themes never draw the monitor).
    void  UpdateCrtMonitorCheckboxEnabled ();

    ThemeSelectFn        m_onThemeSelected;
    FramebufferSourceFn  m_framebufferSource;
    MountedPathFn        m_mountedPathSource;
    WriteProtectFn       m_writeProtectSource;
    HasDiskSourceFn      m_hasDiskSource;
    ApplyThemeNowFn      m_onApplyThemeNow;
    CrtMonitorFn         m_onCrtMonitorToggled;

    DxuiLabel      m_themeLabel;
    DxuiDropdown   m_themeDropdown;
    DxuiButton     m_applyNowButton;
    DxuiCheckbox   m_crtMonitorCheckbox;
    RECT           m_previewRect          = {};
    DxuiDpiScaler  m_scaler;

    // Preview-only DriveWidget instances rendered with the staged
    // theme inside the mock window. Mutable because Paint is const
    // but the widgets' Layout / SetCompact / SyncFromState are not.
    mutable std::array<DriveWidget, 2>  m_previewDrives;
    mutable bool                        m_previewDrivesInitialized = false;

    // Preview-only joystick-mode toggle button. Rendered "on" so the
    // theme preview also shows the lit blue LED in the band above the
    // drive widgets, mirroring the live chrome.
    mutable JoystickToggleButton        m_previewJoystickButton;

    // The app's OWN caption bar and menu, instanced for the preview rather
    // than redrawn by hand. Neither is adopted or wired to anything -- they
    // are laid out and painted, nothing more -- so the mock cannot drift out
    // of step with the chrome it is advertising.
    mutable DxuiCaptionBar              m_previewCaption;
    mutable MainMenu                    m_previewMenu;
    mutable bool                        m_previewChromeConfigured = false;

    // Refreshed every paint, read by the sheet right afterward.
    mutable PreviewSceneRequest         m_sceneRequest;
};
