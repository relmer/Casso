#pragma once

#include "Pch.h"

#include "TitleBar.h"


class IChromedPanelContent;
struct ChromeTheme;
class DxuiHostWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ChromedPanelWindow
//
//  Reusable chrome shell for DX-painted modeless popup windows. Owns
//  the HWND + class registration + title bar + NC hit testing + DPI
//  handling + mouse / keyboard dispatch. The actual rendering, layout,
//  and interaction model live in an IChromedPanelContent provided to
//  Create.
//
// Use cases: Settings popup, Disk II Debug panel
//  panel. Each provides its own content + renderer; the chrome is
//  identical and lives here.
//
////////////////////////////////////////////////////////////////////////////////

class ChromedPanelWindow
{
public:
    ChromedPanelWindow  () = default;
    ~ChromedPanelWindow ();

    HRESULT  RegisterClass (HINSTANCE hInstance, LPCWSTR className);
    HRESULT  Create        (HWND                       hwndOwner,
                            IChromedPanelContent     * content,
                            ID3D11Device             * device,
                            ID3D11DeviceContext      * context,
                            const ChromeTheme        * theme);
    void     Destroy       ();
    void     SetTheme      (const ChromeTheme * theme);
    HRESULT  Render        ();

    //
    //  Bring the panel window to the foreground, restoring it first if
    //  it is currently minimized (SW_SHOW alone leaves a minimized
    //  window iconic). Used by the open / toggle hotkey so re-invoking
    //  it un-minimizes and surfaces the window.
    //
    void     Activate      ();

    bool       IsOpen     () const { return m_hwnd != nullptr; }
    HWND       Hwnd       () const { return m_hwnd; }
    TitleBar & GetTitleBar()       { return m_titleBar; }

    //
    //  The adopt-mode DxuiHostWindow whose popup pool hosts this
    //  window's out-of-client popups (e.g. a content right-click menu).
    //  Valid only between OnCreate and OnDestroy; null otherwise.
    //
    DxuiHostWindow *  PopupHost () const { return m_hostWindow.get(); }

private:
    static LRESULT CALLBACK s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT  WndProc            (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    HRESULT  OnCreate           (HWND hwnd);
    void     OnDestroy          ();
    void     OnSize             (int widthPx, int heightPx);
    void     OnDpiChanged       (UINT dpi, const RECT & suggestedRect);
    void     OnGetMinMax        (MINMAXINFO * minMaxInfo);
    LRESULT  ClassifyHitForLegacyChrome (POINT ptScreen);
    bool     OnNcLButtonDown    (HWND hwnd, LRESULT hitTest);
    void     OnNcMouse          (UINT message, WPARAM wParam, LPARAM lParam);
    void     OnMouse            (UINT message, WPARAM wParam, LPARAM lParam);
    void     OnKeyDown          (WPARAM vk);
    void     CloseWithCancel    ();
    void     DestroyIfContentInactive ();

    SIZE     GetPreferredClientSize (UINT dpi) const;
    RECT     GetInitialWindowRect   (HWND hwndOwner, UINT dpi) const;

    HINSTANCE                m_hInstance = nullptr;
    HWND                     m_hwnd      = nullptr;
    HWND                     m_hwndOwner = nullptr;
    IChromedPanelContent   * m_content   = nullptr;
    ID3D11Device           * m_device    = nullptr;
    ID3D11DeviceContext    * m_context   = nullptr;
    TitleBar                 m_titleBar;
    const ChromeTheme      * m_theme     = nullptr;
    LPCWSTR                  m_className = nullptr;
    bool                     m_hasFocus  = false;

    // DxuiHostWindow running in adopt mode -- wraps this HWND and
    // takes over WM_NCCALCSIZE / WM_NCHITTEST classification. The
    // bespoke legacy-chrome hit-test is plugged in via
    // SetHitTestDelegate inside OnCreate; everything else still runs
    // through ChromedPanelWindow's WndProc.
    std::unique_ptr<DxuiHostWindow>  m_hostWindow;
};
