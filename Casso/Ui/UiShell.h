#pragma once

#include "Pch.h"

#include "FocusManager.h"
#include "Chrome/CassoTheme.h"
#include "Chrome/DriveWidget.h"
#include "Chrome/JoystickToggleButton.h"
#include "Chrome/MainMenu.h"
#include "Widgets/DxuiTooltip.h"


class D3DRenderer;
class SettingsPanel;




////////////////////////////////////////////////////////////////////////////////
//
//  UiShell
//
////////////////////////////////////////////////////////////////////////////////

class UiShell
{
public:
    UiShell  () = default;
    ~UiShell ();

    HRESULT  Initialize         (D3DRenderer * pRenderer);
    void     Shutdown           ();

    HRESULT  OnResize           (int viewportWidthPx,
                                  int viewportHeightPx,
                                  UINT dpi);

    HRESULT  OnDeviceLost       ();
    HRESULT  OnDeviceRestored   ();

    void     Render             ();

    void     SetDebugBannerText (const std::wstring & text) { m_debugBanner = text; }
    void     SetShowDebugBanner (bool showBanner)           { m_showBanner  = showBanner; }
    void     SetChrome          (MainMenu                        * mainMenu,
                                  std::array<DriveWidget, 2>      * driveWidgets,
                                  const CassoTheme               * theme);
    void     SetSettingsPanel   (SettingsPanel                   * settingsPanel);
    void     SetJoystickButton  (JoystickToggleButton            * button,
                                  DxuiTooltip                         * tooltip)
    {
        m_joystickButton  = button;
        m_joystickTooltip = tooltip;
    }
    void     SetDragSource      (const DxuiDragDropTarget            * dragSource) { m_dragSource = dragSource; }
    bool     OnMouseMove        (int x, int y, bool leftDown);
    void     OnMouseLeave       ();
    bool     OnLButtonDown      (int x, int y);
    bool     OnLButtonUp        (int x, int y);
    bool     HandleKey          (WPARAM vk);
    bool     IsCapturingInput   () const;
    bool     IsSettingsCapturing () const;

    DxuiPainter         & Painter   ()       { return m_painter; }
    DxuiTextRenderer  & Text      ()       { return m_text; }
    DxuiInput             & Input     ()       { return m_input; }
    DxuiHitTester           & HitTest   ()       { return m_hitTest; }
    FocusManager        & Focus     ()       { return m_focus; }
    DxuiAnimation           & Tweens    ()       { return m_anim; }

    int    ViewportWidth  () const { return m_viewportWidthPx; }
    int    ViewportHeight () const { return m_viewportHeightPx; }

    const CassoTheme & Theme () const
    {
        static const CassoTheme s_kFallback = CassoTheme::Skeuomorphic();
        return (m_theme != nullptr) ? *m_theme : s_kFallback;
    }

    const DxuiDpiScaler  & Scaler () const { return m_scaler; }

private:
    HRESULT  RefreshTextTarget ();
    void     PaintDragDropOverlay (const ChromeVisualState & visual,
                                    const CassoTheme       & theme,
                                    int                       bottomInset,
                                    int                       barTop);

    D3DRenderer                 * m_renderer     = nullptr;

    DxuiPainter                   m_painter;
    DxuiTextRenderer            m_text;
    DxuiInput                       m_input;
    DxuiHitTester                     m_hitTest;
    FocusManager                  m_focus;
    DxuiAnimation                     m_anim;

    MainMenu                    * m_mainMenu      = nullptr;
    std::array<DriveWidget, 2>  * m_driveWidgets  = nullptr;
    JoystickToggleButton        * m_joystickButton  = nullptr;
    DxuiTooltip                     * m_joystickTooltip = nullptr;
    const CassoTheme           * m_theme         = nullptr;
    SettingsPanel               * m_settingsPanel = nullptr;
    const DxuiDragDropTarget        * m_dragSource    = nullptr;

    int                           m_viewportWidthPx  = 0;
    int                           m_viewportHeightPx = 0;
    UINT                          m_dpi              = 96;
    DxuiDpiScaler                     m_scaler;
    bool                          m_initialized      = false;
    bool                          m_targetDirty      = true;
    bool                          m_leftDown         = false;
    int                           m_frameIndex       = 0;

    std::wstring                  m_debugBanner;
    bool                          m_showBanner       = false;
};
