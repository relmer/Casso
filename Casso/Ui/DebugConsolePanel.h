#pragma once

#include "Chrome/ChromedPanelWindow.h"
#include "Chrome/IChromedPanelContent.h"
#include "DxUiPainter.h"
#include "DwriteTextRenderer.h"


struct ChromeTheme;
class TitleBar;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugConsolePanel
//
//  Spec-011 / US6. Themed DX replacement for the legacy Win32
//  DebugConsole EDIT-control window. Hosts itself inside a
//  ChromedPanelWindow and exposes the same Log / LogConfig contract
//  the shell already uses to push diagnostic strings. Lines are kept
//  in a thread-safe buffer; the body paints them in a monospace font
//  with mouse-wheel + keyboard scrolling, click-drag / Shift+arrow
//  text selection, Ctrl+A select-all, and Ctrl+C copy-selection to
//  the clipboard.
//
////////////////////////////////////////////////////////////////////////////////

class DebugConsolePanel : public IChromedPanelContent
{
public:
    DebugConsolePanel  ();
    ~DebugConsolePanel () override;

    HRESULT Create  (HINSTANCE              hInstance,
                     HWND                   hwndOwner,
                     ID3D11Device         * device,
                     ID3D11DeviceContext  * context,
                     const ChromeTheme    * theme);
    void    Show    ();
    void    Hide    ();
    void    Destroy ();

    bool    IsOpen    () const { return m_window.IsOpen(); }
    bool    IsVisible () const { return m_window.IsOpen() && IsWindowVisible (m_window.Hwnd()) != FALSE; }
    HWND    Hwnd      () const { return m_window.Hwnd(); }

    HRESULT RenderFrame ();
    void    SetTheme    (const ChromeTheme * theme);

    // Same contract as the legacy DebugConsole. Safe to call from any
    // thread; the buffer is mutex-guarded and Render reads under the
    // same lock.
    void    Log       (const std::wstring & message);
    void    LogConfig (const std::string  & summary);

    // IChromedPanelContent.
    LPCWSTR  GetWindowClassName () const override;
    LPCWSTR  GetWindowTitle     () const override;
    HRESULT  OnHostCreated      (HWND                   hwnd,
                                 ID3D11Device         * device,
                                 ID3D11DeviceContext  * context,
                                 int                    widthPx,
                                 int                    heightPx,
                                 UINT                   dpi,
                                 TitleBar             * titleBar,
                                 const ChromeTheme    * theme) override;
    void     OnHostDestroyed    () override;
    HRESULT  OnHostResize       (int widthPx, int heightPx, UINT dpi) override;
    HRESULT  Render             () override;
    void     SetChromeTheme     (TitleBar * titleBar, const ChromeTheme * theme) override;
    SIZE     PreferredClientSize (UINT dpi) const override;

    void     OnLButtonDown (int x, int y) override;
    void     OnLButtonUp   (int x, int y) override;
    void     OnMouseMove   (int x, int y) override;
    void     OnMouseWheel  (int x, int y, int delta) override;
    bool     OnKey         (WPARAM vk) override;

    void     Accept () override;
    void     Cancel () override;
    bool     IsContentActive () const override { return true; }

private:
    struct Pos
    {
        int  line   = 0;
        int  column = 0;
    };

    HRESULT  EnsureSwapChain         ();
    HRESULT  CreateBackBufferRtv     ();
    void     ReleaseRenderTargets    ();
    void     ClampScroll             ();
    int      LinesVisible            () const;
    int      LineHeightPx            () const;
    void     CopySelectionToClipboard ();
    void     EnsureCharMetrics       ();
    Pos      HitTestChar              (int xPx, int yPx) const;
    int      ClampColumn             (int line, int col) const;
    void     CollapseSelectionToCaret ();
    void     SelectAll               ();
    bool     HasSelection            () const;
    void     OrderedSelection        (Pos & lo, Pos & hi) const;
    void     MoveCaretLineEnd        (Pos & p, bool toEnd) const;
    int      BodyTopPx               () const;

    static constexpr UINT  s_kSwapBufferCount = 2;
    static constexpr int   s_kFontDip         = 13;
    static constexpr int   s_kPadDp           = 12;

    ChromedPanelWindow                   m_window;
    HWND                                 m_hwnd     = nullptr;
    ID3D11Device                       * m_device   = nullptr;
    ID3D11DeviceContext                * m_context  = nullptr;
    TitleBar                           * m_titleBar = nullptr;
    const ChromeTheme                  * m_theme    = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>      m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    DxUiPainter                          m_painter;
    DwriteTextRenderer                   m_text;
    int                                  m_widthPx  = 0;
    int                                  m_heightPx = 0;
    UINT                                 m_dpi      = 96;

    mutable std::mutex                   m_bufferMutex;
    std::vector<std::wstring>            m_lines;
    int                                  m_scrollLine = 0;
    static constexpr size_t              s_kMaxLines  = 4096;

    Pos                                  m_selAnchor;
    Pos                                  m_selCaret;
    bool                                 m_selecting        = false;
    float                                m_charWidthPx      = 0.0f;
    bool                                 m_charMetricsReady = false;
};
