#include "Pch.h"

#include "UiShell.h"

#include "D3DRenderer.h"





namespace
{
    constexpr uint32_t  s_kBannerArgb     = 0xFFE0E0E0;
    constexpr uint32_t  s_kAccentBarArgb  = 0x80003366;
    constexpr float     s_kBannerFontDip  = 14.0f;
    constexpr float     s_kAccentBarHigh  = 4.0f;
    constexpr float     s_kBannerPadDip   = 8.0f;
    constexpr wchar_t   s_kBannerFamily[] = L"Segoe UI";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~UiShell
//
////////////////////////////////////////////////////////////////////////////////

UiShell::~UiShell ()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Wires the painter + text renderer onto the live `D3DRenderer`
//  device. Must run after the renderer has come up so the device,
//  context, and back-buffer surface are available.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::Initialize (D3DRenderer * pRenderer)
{
    HRESULT                hr     = S_OK;
    ID3D11Device         * device = nullptr;
    ID3D11DeviceContext  * ctx    = nullptr;



    CBRAEx (pRenderer, E_INVALIDARG);

    m_renderer = pRenderer;
    device     = m_renderer->GetDevice();
    ctx        = m_renderer->GetContext();

    CBRA (device);
    CBRA (ctx);

    hr = m_painter.Initialize (device, ctx);
    CHRA (hr);

    hr = m_text.Initialize (device);
    CHRA (hr);

    m_viewportWidthPx  = m_renderer->GetBackBufferWidth();
    m_viewportHeightPx = m_renderer->GetBackBufferHeight();
    m_targetDirty      = true;
    m_initialized      = true;

    if (m_debugBanner.empty())
    {
        m_debugBanner = L"Casso UI";
    }

Error:
    if (FAILED (hr))
    {
        Shutdown();
    }
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::Shutdown ()
{
    m_anim.ClearTweens();
    m_focus.Clear();
    m_hitTest.Clear();
    m_input.Clear();
    m_text.Shutdown();
    m_painter.Shutdown();
    m_renderer    = nullptr;
    m_initialized = false;
    m_targetDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnResize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::OnResize (int viewportWidthPx, int viewportHeightPx, UINT dpi)
{
    m_viewportWidthPx  = viewportWidthPx;
    m_viewportHeightPx = viewportHeightPx;
    m_dpi              = (dpi == 0) ? 96 : dpi;
    m_targetDirty      = true;

    m_text.UnbindBackBuffer();

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceLost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::OnDeviceLost ()
{
    HRESULT  hrPainter = m_painter.OnDeviceLost();
    HRESULT  hrText    = m_text.OnDeviceLost();

    m_initialized = false;
    m_targetDirty = true;

    if (FAILED (hrPainter)) { return hrPainter; }
    return hrText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceRestored
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::OnDeviceRestored ()
{
    HRESULT                hr      = S_OK;
    ID3D11Device         * device  = nullptr;
    ID3D11DeviceContext  * ctx     = nullptr;



    CBRA (m_renderer);

    device = m_renderer->GetDevice();
    ctx    = m_renderer->GetContext();
    CBRA (device);
    CBRA (ctx);

    hr = m_painter.OnDeviceRestored (device, ctx);
    CHRA (hr);

    hr = m_text.OnDeviceRestored (device);
    CHRA (hr);

    m_initialized = true;
    m_targetDirty = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshTextTarget
//
//  Re-acquires the swap-chain back-buffer surface and binds it to the
//  text renderer. Invoked once after init and again any time the
//  swap-chain back buffer changes (resize, device restore).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::RefreshTextTarget ()
{
    HRESULT                hr        = S_OK;
    ComPtr<IDXGISurface>   surface;



    CBRA (m_renderer);

    hr = m_renderer->GetBackBufferDxgiSurface (&surface);
    CHRA (hr);

    hr = m_text.BindBackBuffer (surface.Get(), m_dpi, m_dpi);
    CHRA (hr);

    m_targetDirty = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Composites the native chrome on top of the emulator framebuffer.
//  This baseline paints a thin top accent bar (proof-of-life for the
//  painter) and draws a "Casso UI" banner in the upper-left via the
//  D2D / DirectWrite text renderer (proof-of-life for the text pass).
//  Subsequent phases stack additional chrome surfaces on top of this
//  hook.
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::Render ()
{
    HRESULT                  hr   = S_OK;
    ID3D11RenderTargetView * rtv  = nullptr;



    if (!m_initialized || (m_renderer == nullptr))
    {
        return;
    }

    if (m_targetDirty)
    {
        HRESULT  hrRefresh = RefreshTextTarget();

        if (FAILED (hrRefresh))
        {
            // Defer rebind to next frame; a transient bind failure
            // shouldn't drop the entire UI composite.
            return;
        }
    }

    m_viewportWidthPx  = m_renderer->GetBackBufferWidth();
    m_viewportHeightPx = m_renderer->GetBackBufferHeight();

    rtv = m_renderer->GetBackBufferRtv();

    if (rtv != nullptr)
    {
        hr = m_painter.Begin (m_viewportWidthPx, m_viewportHeightPx);

        if (SUCCEEDED (hr))
        {
            m_painter.FillRect (0.0f,
                                0.0f,
                                (float) m_viewportWidthPx,
                                s_kAccentBarHigh,
                                s_kAccentBarArgb);

            hr = m_painter.End (rtv);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    if (m_showBanner && !m_debugBanner.empty())
    {
        hr = m_text.BeginDraw();

        if (SUCCEEDED (hr))
        {
            HRESULT  hrDraw = m_text.DrawString (m_debugBanner.c_str(),
                                                 s_kBannerPadDip,
                                                 s_kBannerPadDip,
                                                 (float) m_viewportWidthPx  - s_kBannerPadDip,
                                                 (float) m_viewportHeightPx - s_kBannerPadDip,
                                                 s_kBannerArgb,
                                                 s_kBannerFontDip,
                                                 s_kBannerFamily);
            IGNORE_RETURN_VALUE (hrDraw, S_OK);

            hr = m_text.EndDraw();
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}
