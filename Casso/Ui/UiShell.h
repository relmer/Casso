#pragma once

#include "Pch.h"

#include "Animation.h"
#include "DwriteTextRenderer.h"
#include "DxUiPainter.h"
#include "FocusManager.h"
#include "HitTester.h"
#include "UiInput.h"
#include "Chrome/ChromeTheme.h"
#include "Chrome/DriveWidget.h"
#include "Chrome/NavLayer.h"
#include "Chrome/TitleBar.h"


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
    void     SetChrome          (TitleBar                        * titleBar,
                                  NavLayer                        * navLayer,
                                  std::array<DriveWidget, 2>      * driveWidgets,
                                  const ChromeTheme               * theme);
    void     SetSettingsPanel   (SettingsPanel                   * settingsPanel);
    bool     OnMouseMove        (int x, int y, bool leftDown);
    bool     OnLButtonDown      (int x, int y);
    bool     OnLButtonUp        (int x, int y);
    bool     HandleKey          (WPARAM vk);

    DxUiPainter         & Painter   ()       { return m_painter; }
    DwriteTextRenderer  & Text      ()       { return m_text; }
    UiInput             & Input     ()       { return m_input; }
    HitTester           & HitTest   ()       { return m_hitTest; }
    FocusManager        & Focus     ()       { return m_focus; }
    Animation           & Tweens    ()       { return m_anim; }

    int    ViewportWidth  () const { return m_viewportWidthPx; }
    int    ViewportHeight () const { return m_viewportHeightPx; }

private:
    HRESULT  RefreshTextTarget ();

    D3DRenderer                 * m_renderer     = nullptr;

    DxUiPainter                   m_painter;
    DwriteTextRenderer            m_text;
    UiInput                       m_input;
    HitTester                     m_hitTest;
    FocusManager                  m_focus;
    Animation                     m_anim;

    TitleBar                    * m_titleBar      = nullptr;
    NavLayer                    * m_navLayer      = nullptr;
    std::array<DriveWidget, 2>  * m_driveWidgets  = nullptr;
    const ChromeTheme           * m_theme         = nullptr;
    SettingsPanel               * m_settingsPanel = nullptr;

    int                           m_viewportWidthPx  = 0;
    int                           m_viewportHeightPx = 0;
    UINT                          m_dpi              = 96;
    bool                          m_initialized      = false;
    bool                          m_targetDirty      = true;
    bool                          m_leftDown         = false;
    int                           m_frameIndex       = 0;

    std::wstring                  m_debugBanner;
    bool                          m_showBanner       = false;
};
