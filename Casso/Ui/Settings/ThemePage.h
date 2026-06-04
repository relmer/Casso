#pragma once

#include "Pch.h"

#include "../Chrome/ChromeTheme.h"
#include "../Chrome/DriveWidget.h"
#include "../Chrome/JoystickToggleButton.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Dropdown.h"
#include "../Widgets/Label.h"





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

class ThemePage
{
public:
    using ThemeSelectFn      = std::function<void (const std::string & themeName)>;
    using FramebufferSourceFn = std::function<const uint32_t * (int & outWidthPx, int & outHeightPx)>;
    using MountedPathFn      = std::function<std::wstring (int driveIndex)>;

    void  SetThemes             (std::vector<std::string>  themeIds,
                                 std::vector<std::wstring> displayNames,
                                 int                       activeIndex);
    void  SetOnThemeSelected    (ThemeSelectFn       fn) { m_onThemeSelected   = std::move (fn); }
    void  SetFramebufferSource  (FramebufferSourceFn fn) { m_framebufferSource = std::move (fn); }
    void  SetMountedPathSource  (MountedPathFn       fn) { m_mountedPathSource = std::move (fn); }

    void  Layout                (const RECT & rect, const DxuiDpiScaler & scaler);

    void  OnLButtonDown         (int x, int y);
    void  OnLButtonUp           (int x, int y);
    void  OnMouseHover          (int x, int y);
    bool  OnKey                 (WPARAM vk);

    void  Paint                 (DxUiPainter & painter, DwriteTextRenderer & text) const;

    void  CollectFocusables (std::vector<std::function<void (bool)>> & out);
    bool  AnyDropdownOpen   () const { return m_themeDropdown.IsOpen(); }

    const Dropdown                 & ThemeDropdown    () const { return m_themeDropdown; }
    const std::vector<std::string> & Themes           () const { return m_themeIds; }
    int                              ActiveThemeIndex () const { return m_activeIndex; }

private:
    std::vector<std::string>      m_themeIds;
    int                           m_activeIndex = -1;
    ThemeSelectFn                 m_onThemeSelected;
    FramebufferSourceFn           m_framebufferSource;
    MountedPathFn                 m_mountedPathSource;

    Label                         m_themeLabel;
    Dropdown                      m_themeDropdown;
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
