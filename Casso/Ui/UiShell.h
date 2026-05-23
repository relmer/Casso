#pragma once

#include "Pch.h"

#include "Animation.h"
#include "DwriteTextRenderer.h"
#include "DxUiPainter.h"
#include "FocusManager.h"
#include "HitTester.h"
#include "UiInput.h"


class D3DRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  UiShell
//
//  Top-level owner of the native UI runtime. Constructs and wires the
//  painter, text renderer, input translator, hit-tester, focus manager,
//  and animation broker against the active `D3DRenderer`. Provides a
//  single per-frame `Render()` entry point that the renderer's
//  after-blit hook invokes between the emulator blit and `Present`.
//
//  The shell does not own the swap chain; it borrows the device,
//  context, and back-buffer surface from the renderer for the duration
//  of a frame. On window resize or device-lost the consumer must call
//  the corresponding `OnResize` / `OnDeviceLost` / `OnDeviceRestored`
//  hooks so the text renderer's bitmap target is rebuilt against the
//  new surface.
//
////////////////////////////////////////////////////////////////////////////////

class UiShell
{
public:
    UiShell  () = default;
    ~UiShell ();

    HRESULT  Initialize        (D3DRenderer * pRenderer);
    void     Shutdown          ();

    HRESULT  OnResize          (int viewportWidthPx,
                                int viewportHeightPx,
                                UINT dpi);

    HRESULT  OnDeviceLost      ();
    HRESULT  OnDeviceRestored  ();

    void     Render            ();

    void     SetDebugBannerText (const std::wstring & text) { m_debugBanner = text; }
    void     SetShowDebugBanner (bool showBanner)           { m_showBanner  = showBanner; }

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


    D3DRenderer        * m_renderer = nullptr;     // non-owning

    DxUiPainter          m_painter;
    DwriteTextRenderer   m_text;
    UiInput              m_input;
    HitTester            m_hitTest;
    FocusManager         m_focus;
    Animation            m_anim;

    int                  m_viewportWidthPx  = 0;
    int                  m_viewportHeightPx = 0;
    UINT                 m_dpi              = 96;
    bool                 m_initialized      = false;
    bool                 m_targetDirty      = true;

    std::wstring         m_debugBanner;
    bool                 m_showBanner       = true;
};
