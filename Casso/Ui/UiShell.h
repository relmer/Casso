#pragma once

#include "Pch.h"

#include "Chrome/CassoTheme.h"


class D3DRenderer;
class MainMenu;




////////////////////////////////////////////////////////////////////////////////
//
//  UiShell
//
//  Input router and hit-tester for the native window chrome. Owns the
//  DWrite text renderer used for chrome text measurement, plus the theme
//  and viewport metrics the settings panel consults during layout /
//  paint. The chrome itself paints through the host panel tree, so
//  UiShell renders nothing.
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

    void     SetMainMenu        (MainMenu * mainMenu)           { m_mainMenu      = mainMenu; }
    void     SetTheme           (const CassoTheme * theme)      { m_theme         = theme; }

    bool     OnMouseMove        (int x, int y, bool leftDown);
    void     OnMouseLeave       ();
    bool     OnLButtonDown      (int x, int y);
    bool     OnLButtonUp        (int x, int y);
    bool     HandleKey          (WPARAM vk);
    bool     IsCapturingInput   () const;

    DxuiTextRenderer  & Text    ()       { return m_text; }
    DxuiHitTester     & HitTest ()       { return m_hitTest; }

    int    ViewportWidth  () const { return m_viewportWidthPx; }
    int    ViewportHeight () const { return m_viewportHeightPx; }

    const DxuiDpiScaler  & Scaler () const { return m_scaler; }

    const CassoTheme & Theme () const
    {
        static const CassoTheme s_kFallback = CassoTheme::Skeuomorphic();
        return (m_theme != nullptr) ? *m_theme : s_kFallback;
    }

private:
    D3DRenderer                 * m_renderer      = nullptr;

    DxuiTextRenderer              m_text;
    DxuiHitTester                 m_hitTest;

    MainMenu                    * m_mainMenu      = nullptr;
    const CassoTheme            * m_theme         = nullptr;

    int                           m_viewportWidthPx  = 0;
    int                           m_viewportHeightPx = 0;
    UINT                          m_dpi              = 96;
    DxuiDpiScaler                 m_scaler;
    bool                          m_leftDown         = false;
};
