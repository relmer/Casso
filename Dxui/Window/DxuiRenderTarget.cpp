#include "Pch.h"

#include "DxuiRenderTarget.h"

#include "Theme/IDxuiTheme.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTarget::DxuiRenderTarget / ~DxuiRenderTarget
//
////////////////////////////////////////////////////////////////////////////////

DxuiRenderTarget::DxuiRenderTarget () = default;


DxuiRenderTarget::~DxuiRenderTarget ()
{
    // m_painter / m_textRenderer / m_device / m_context are torn down by the
    // subclass (it created them and knows the ordering vs. its swap chain);
    // the ComPtr / unique_ptr members simply release anything still live here.
    ReleaseComposeTarget();
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTarget::SetComposeHook
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRenderTarget::SetComposeHook (ComposeHook hook)
{
    bool  wasComposing = static_cast<bool> (m_composeHook);


    m_composeHook = std::move (hook);

    // Leaving the compose path: restore the text renderer's D2D target to the
    // back buffer (it was pointed at the offscreen texture) BEFORE dropping the
    // offscreen target, so a window that stops composing paints straight to the
    // back buffer again instead of a released surface.
    if (wasComposing && !m_composeHook)
    {
        (void) BindTextTarget (false);
        ReleaseComposeTarget();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTarget::BindTextTarget
//
//  Point the text renderer's Direct2D target at either the offscreen content
//  texture (compose path) or the swap-chain back buffer (direct path). Called
//  by the subclass's CreateBackBufferRtv on resize and by RenderFrame when the
//  compose path first needs the offscreen target. Binding to the offscreen
//  ensures the texture exists (same size as the back buffer).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderTarget::BindTextTarget (bool offscreen)
{
    HRESULT               hr  = S_OK;
    UINT                  dpi = 0;
    ComPtr<IDXGISurface>  surface;


    if (m_textRenderer == nullptr)
    {
        return S_OK;
    }

    dpi = TargetDpi();

    if (offscreen)
    {
        SIZE  sz        = BackBufferSizePx();
        bool  recreated = false;

        hr = EnsureComposeTarget ((int) sz.cx, (int) sz.cy, recreated);
        CHRA (hr);

        hr = m_contentTex.As (&surface);
        CHRA (hr);

        hr = m_textRenderer->BindBackBuffer (surface.Get(), dpi, dpi);
        CHRA (hr);

        m_textBoundToOffscreen = true;
    }
    else
    {
        surface = BackBufferSurface();
        if (surface == nullptr)
        {
            return S_OK;
        }

        hr = m_textRenderer->BindBackBuffer (surface.Get(), dpi, dpi);
        CHRA (hr);

        m_textBoundToOffscreen = false;
    }

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTarget::ReleaseComposeTarget
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRenderTarget::ReleaseComposeTarget ()
{
    m_contentSrv.Reset();
    m_contentRtv.Reset();
    m_contentTex.Reset();
    m_contentWidthPx  = 0;
    m_contentHeightPx = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTarget::EnsureComposeTarget
//
//  Lazily (re)creates the offscreen content texture + its RTV / SRV at the
//  given size. The content tree paints into this texture; the compose hook
//  samples it (SRV) to produce the final back-buffer frame. Same BGRA8 format
//  as the back buffer so a straight copy path stays valid.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderTarget::EnsureComposeTarget (int widthPx, int heightPx, bool & recreated)
{
    HRESULT               hr   = S_OK;
    D3D11_TEXTURE2D_DESC  td   = {};


    recreated = false;

    if (m_device == nullptr || widthPx <= 0 || heightPx <= 0)
    {
        return E_FAIL;
    }

    if (m_contentTex != nullptr &&
        m_contentWidthPx == widthPx && m_contentHeightPx == heightPx)
    {
        return S_OK;
    }

    ReleaseComposeTarget();
    recreated = true;

    td.Width          = (UINT) widthPx;
    td.Height         = (UINT) heightPx;
    td.MipLevels      = 1;
    td.ArraySize      = 1;
    td.Format         = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage          = D3D11_USAGE_DEFAULT;
    td.BindFlags      = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_device->CreateTexture2D (&td, nullptr, m_contentTex.GetAddressOf());
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (m_contentTex.Get(), nullptr, m_contentRtv.GetAddressOf());
    CHRA (hr);

    hr = m_device->CreateShaderResourceView (m_contentTex.Get(), nullptr, m_contentSrv.GetAddressOf());
    CHRA (hr);

    m_contentWidthPx  = widthPx;
    m_contentHeightPx = heightPx;

Error:
    if (FAILED (hr))
    {
        ReleaseComposeTarget();
    }
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTarget::RenderFrame
//
//  The per-frame pump, extracted from DxuiHwndSource::PaintPump. Clears the
//  back buffer, runs the before-present hook (emulator composite), paints the
//  content tree (subclass PaintContent), runs the after-paint hook, and
//  Presents. The offscreen compose path (m_composeHook) is wired in a later
//  slice; today a non-null hook falls through to the direct back-buffer path.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRenderTarget::RenderFrame (const IDxuiTheme * theme)
{
    ID3D11RenderTargetView *  backRtv = BackBufferRtv();
    SIZE                      sz      = {};
    float                     clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    uint32_t                  bgArgb  = 0xFF000000u;


    if (backRtv == nullptr || m_context == nullptr ||
        m_painter == nullptr || m_textRenderer == nullptr)
    {
        return;
    }

    sz = BackBufferSizePx();
    if (sz.cx <= 0 || sz.cy <= 0)
    {
        return;
    }

    // Theme background clear. Without a theme, still clear to opaque black so a
    // partially-themed startup frame doesn't present back-buffer garbage.
    if (theme != nullptr)
    {
        bgArgb = theme->Background();
    }
    clearColor[0] = (float) ((bgArgb >> 16) & 0xFFu) / 255.0f;
    clearColor[1] = (float) ((bgArgb >>  8) & 0xFFu) / 255.0f;
    clearColor[2] = (float) ((bgArgb      ) & 0xFFu) / 255.0f;
    clearColor[3] = (float) ((bgArgb >> 24) & 0xFFu) / 255.0f;

    // Opt-in offscreen compose path: paint the content tree into the offscreen
    // texture, then hand the compose hook that texture's SRV + the back-buffer
    // RTV so it produces the final frame (blur / see-through reveal). The hook
    // owns the back buffer (binds + fills it), so there is no back-buffer clear
    // or before/after-paint hook here -- those belong to the direct path.
    if (m_composeHook)
    {
        bool     recreated = false;
        HRESULT  hrEnsure  = EnsureComposeTarget ((int) sz.cx, (int) sz.cy, recreated);

        if (SUCCEEDED (hrEnsure) && m_contentRtv != nullptr && m_contentSrv != nullptr)
        {
            // Point the text renderer's D2D target at the offscreen texture so
            // glyphs land there too (only when it actually changed).
            if (recreated || !m_textBoundToOffscreen)
            {
                (void) BindTextTarget (true);
            }

            m_context->OMSetRenderTargets    (1, m_contentRtv.GetAddressOf(), nullptr);
            m_context->ClearRenderTargetView (m_contentRtv.Get(), clearColor);

            if (theme != nullptr)
            {
                PaintContent (m_contentRtv.Get(), (int) sz.cx, (int) sz.cy, *theme);
            }

            m_composeHook (m_contentSrv.Get(), backRtv, (int) sz.cx, (int) sz.cy);

            PresentFrame();
            return;
        }
        // EnsureComposeTarget failed -> fall through to the direct path.
    }

    m_context->OMSetRenderTargets    (1, &backRtv, nullptr);
    m_context->ClearRenderTargetView (backRtv, clearColor);

    // Composite the consumer's content (e.g. the Apple ][ framebuffer) into the
    // back buffer FIRST; the panel-tree passes below are additive.
    {
        const std::function<void()> &  beforeHook = BeforePresentHook();

        if (beforeHook)
        {
            beforeHook();
        }
    }

    // Paint the content tree + host caption + modal overlay onto the back
    // buffer (subclass owns what "content" is; the text renderer's D2D target
    // is already bound to this back buffer by the subclass's CreateBackBufferRtv).
    if (theme != nullptr)
    {
        PaintContent (backRtv, (int) sz.cx, (int) sz.cy, *theme);
    }

    // After-paint compositor hook: full-screen shader passes on the back buffer
    // before Present (e.g. a settings live-preview blur/compose pass).
    {
        const std::function<void(ID3D11RenderTargetView *, int, int)> &  afterHook = AfterPaintHook();

        if (afterHook)
        {
            afterHook (backRtv, (int) sz.cx, (int) sz.cy);
        }
    }

    PresentFrame();
}
