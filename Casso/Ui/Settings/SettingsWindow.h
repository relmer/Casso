#pragma once

#include "Pch.h"

#include "SettingsWindowRenderer.h"
#include "../Chrome/TitleBar.h"


class SettingsPanel;
class DxuiHostWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsWindow
//
////////////////////////////////////////////////////////////////////////////////

class SettingsWindow
{
public:
    SettingsWindow  () = default;
    ~SettingsWindow();

    HRESULT RegisterClass (HINSTANCE hInstance);
    HRESULT Create        (HWND                   hwndOwner,
                           SettingsPanel        * panel,
                           ID3D11Device         * device,
                           ID3D11DeviceContext  * context,
                           const CassoTheme    * theme);
    void    Destroy       ();
    void    SetTheme      (const CassoTheme * theme);
    HRESULT Render        ();

    bool    IsOpen() const { return m_hwnd != nullptr; }
    HWND    Hwnd   () const { return m_hwnd; }

    SettingsWindowRenderer & GetRenderer() { return m_renderer; }

    // Returns the adopted DxuiHostWindow (non-owning). Valid only
    // while the popup HWND is open (between Create() and Destroy());
    // returns nullptr otherwise. Page-owned dropdowns plug this into
    // SetPopupHost so their menus render through the popup-host pool
    // and escape the SettingsWindow client clip (FR-054 / FR-061;
    // SC-008).
    DxuiHostWindow *  Host() const { return m_hostWindow.get(); }

private:
    static LRESULT CALLBACK s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT WndProc       (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    HRESULT OnCreate      (HWND hwnd);
    void    OnDestroy     ();
    void    OnSize        (int widthPx, int heightPx);
    void    OnDpiChanged  (UINT dpi, const RECT & suggestedRect);
    void    OnGetMinMax   (MINMAXINFO * minMaxInfo);
    LRESULT ClassifyHitForLegacyChrome (POINT ptScreen);
    bool    OnNcLButtonDown (HWND hwnd, LRESULT hitTest);
    void    OnNcMouse     (UINT message, WPARAM wParam, LPARAM lParam);
    void    OnMouse       (UINT message, WPARAM wParam, LPARAM lParam);
    void    OnKeyDown     (WPARAM vk);
    void    OnChar        (WPARAM ch);
    void    OnTimer       (WPARAM timerId);
    void    CloseWithCancel();
    void    DestroyIfPanelClosed();

    SIZE    GetPreferredClientSize (UINT dpi) const;
    RECT    GetInitialWindowRect   (HWND hwndOwner, UINT dpi) const;

    HINSTANCE             m_hInstance = nullptr;
    HWND                  m_hwnd      = nullptr;
    HWND                  m_hwndOwner = nullptr;
    SettingsPanel       * m_panel     = nullptr;
    ID3D11Device        * m_device    = nullptr;
    ID3D11DeviceContext * m_context   = nullptr;
    SettingsWindowRenderer  m_renderer;
    TitleBar              m_titleBar;
    CassoTheme const   * m_theme     = nullptr;
    bool                  m_hasFocus  = false;

    // DxuiHostWindow running in adopt mode -- wraps this HWND and
    // takes over WM_NCCALCSIZE / WM_NCHITTEST classification. The
    // bespoke legacy-chrome hit-test is plugged in via
    // SetHitTestDelegate inside OnCreate; everything else still runs
    // through SettingsWindow's WndProc.
    std::unique_ptr<DxuiHostWindow>  m_hostWindow;
};




