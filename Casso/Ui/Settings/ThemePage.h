#pragma once

#include "Pch.h"

#include "../Chrome/CassoTheme.h"
#include "../Chrome/ChromeMetrics.h"
#include "../Chrome/DriveWidget.h"
#include "../Chrome/JoystickToggleButton.h"
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

    // The 3D desk-scene opt in/out. The checkbox applies live through the
    // callback and is enabled only while a skeuomorphic (non-compact)
    // theme is selected in the dropdown.
    using DeskSceneFn = std::function<void (bool enabled)>;
    void  SetOnDeskSceneToggled (DeskSceneFn fn) { m_onDeskSceneToggled = std::move (fn); }
    void  SetDeskSceneChecked   (bool checked)   { m_deskSceneCheckbox.SetChecked (checked); }

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
    static constexpr int  kPrevSysButtonWDp    = 46;
    static constexpr int  kPrevSysButtonGapDp  = 1;
    static constexpr int  kPrevCaptionFontDp   = 14;
    static constexpr int  kPrevNavFontDp       = 13;

    static RECT      MakeRect (int l, int t, int w, int h);
    static uint32_t  LerpArgb (uint32_t a, uint32_t b, float t);


    // Computes the actual preview rect inside availRect that matches
    // the 100%-zoom window's aspect ratio (which depends on the bottom
    // inset -- full/compact drive bar when a controller is present, or the
    // thin joystick band alone when it isn't). Returns the scale factor used
    // so the caller can size sub-regions consistently.
    static void  ComputePreviewGeometry (const RECT  & availRect,
                                         int           driveBandDp,
                                         RECT        & outPrevRect,
                                         float       & outScale);

    static void  PaintPreviewWindow (DxuiPainter                          & painter,
                                     DxuiTextRenderer                     & text,
                                     const RECT                           & availRect,
                                     const CassoTheme                     & theme,
                                     bool                                   hasDisk,
                                     const std::function<const uint32_t * (int &, int &)> & framebufferSource,
                                     const std::function<std::wstring (int)>              & mountedPathSource,
                                     const std::function<WriteProtectInfo (int)>          & writeProtectSource,
                                     std::array<DriveWidget, 2>           & previewDrives,
                                     JoystickToggleButton                 & previewButton);

    std::vector<std::string>      m_themeIds;
    int                           m_activeIndex = -1;
    // Enables the desk-scene checkbox only while the selected theme is
    // skeuomorphic (compact themes never draw the monitor).
    void  UpdateDeskSceneCheckboxEnabled ();

    ThemeSelectFn                 m_onThemeSelected;
    FramebufferSourceFn           m_framebufferSource;
    MountedPathFn                 m_mountedPathSource;
    WriteProtectFn                m_writeProtectSource;
    HasDiskSourceFn               m_hasDiskSource;
    ApplyThemeNowFn               m_onApplyThemeNow;
    DeskSceneFn                   m_onDeskSceneToggled;

    DxuiLabel      m_themeLabel;
    DxuiDropdown   m_themeDropdown;
    DxuiButton     m_applyNowButton;
    DxuiCheckbox   m_deskSceneCheckbox;
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
};
