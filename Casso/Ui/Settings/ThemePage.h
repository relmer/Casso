#pragma once

#include "Pch.h"

#include "../Chrome/CassoTheme.h"
#include "../Chrome/DriveWidget.h"
#include "../Chrome/JoystickToggleButton.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiButton.h"
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

class ThemePage : public DxuiPanel
{
public:
    ThemePage();

    using ThemeSelectFn      = std::function<void (const std::string & themeName)>;
    using FramebufferSourceFn = std::function<const uint32_t * (int & outWidthPx, int & outHeightPx)>;
    using MountedPathFn      = std::function<std::wstring (int driveIndex)>;

    void  SetThemes             (std::vector<std::string>  themeIds,
                                 std::vector<std::wstring> displayNames,
                                 int                       activeIndex);
    void  SetOnThemeSelected    (ThemeSelectFn       fn) { m_onThemeSelected   = std::move (fn); }
    void  SetFramebufferSource  (FramebufferSourceFn fn) { m_framebufferSource = std::move (fn); }
    void  SetMountedPathSource  (MountedPathFn       fn) { m_mountedPathSource = std::move (fn); }

    // FR-132: fired when the user clicks the "Apply now" button next to
    // the theme dropdown. The panel host wires this to live-apply the
    // currently-selected theme without closing the dialog.
    using ApplyThemeNowFn = std::function<void ()>;
    void  SetOnApplyThemeNow    (ApplyThemeNowFn     fn) { m_onApplyThemeNow   = std::move (fn); }

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
    std::vector<std::string>      m_themeIds;
    int                           m_activeIndex = -1;
    ThemeSelectFn                 m_onThemeSelected;
    FramebufferSourceFn           m_framebufferSource;
    MountedPathFn                 m_mountedPathSource;
    ApplyThemeNowFn               m_onApplyThemeNow;

    DxuiLabel                         m_themeLabel;
    DxuiDropdown                      m_themeDropdown;
    DxuiButton                        m_applyNowButton;
    RECT                          m_previewRect = {};
    DxuiDpiScaler                     m_scaler;

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
