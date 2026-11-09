#include "Pch.h"

#include "Render/DxuiRenderTargets.h"
#include "Render/DxuiSvgPath.h"
#include "DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTargets::DxuiRenderTargets
//
////////////////////////////////////////////////////////////////////////////////

DxuiRenderTargets::DxuiRenderTargets (DxuiTextRenderer & renderer)
    : m_renderer (renderer)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginDrawToTexture
//
//  Points the D2D context at an off-screen D3D texture so the usual DrawString
//  lands in something a shader can sample.
//
//  THE TEXTURE IS KEPT AND GROWN, never shrunk, because a label is rendered
//  only when its text changes and the sizes involved are a few hundred pixels.
//  Reallocating to fit exactly would churn a D3D resource to save nothing.
//
//  96 DPI DELIBERATELY. The caller has already resolved its font to pixels;
//  binding the bitmap at the display DPI would scale it a second time.
//
//  The previously bound target is saved and restored by EndDrawToTexture, so
//  this can run between frames without disturbing the back buffer binding.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderTargets::BeginDrawToTexture (UINT widthPx, UINT heightPx)
{
    HRESULT                  hr    = S_OK;
    D2D1_BITMAP_PROPERTIES1  props = {};
    D3D11_TEXTURE2D_DESC     desc  = {};
    ComPtr<IDXGISurface>     surface;
    bool                     sized = false;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    CBRA (m_renderer.m_d3dDevice);
    CBRAEx (widthPx > 0 && heightPx > 0, E_INVALIDARG);
    CBRA (!m_renderer.m_drawing);

    sized = m_textureTarget != nullptr && m_textureW >= widthPx && m_textureH >= heightPx;

    if (!sized)
    {
        m_textureBitmap.Reset();
        m_textureSrv.Reset();
        m_textureTarget.Reset();

        desc.Width            = widthPx;
        desc.Height           = heightPx;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_DEFAULT;
        desc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = m_renderer.m_d3dDevice->CreateTexture2D (&desc, nullptr, m_textureTarget.GetAddressOf());
        CHRA (hr);

        hr = m_renderer.m_d3dDevice->CreateShaderResourceView (m_textureTarget.Get(), nullptr,
                                                 m_textureSrv.GetAddressOf());
        CHRA (hr);

        hr = m_textureTarget.As (&surface);
        CHRA (hr);

        props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        props.dpiX                  = 96.0f;
        props.dpiY                  = 96.0f;
        props.bitmapOptions         = D2D1_BITMAP_OPTIONS_TARGET;

        hr = m_renderer.m_d2dContext->CreateBitmapFromDxgiSurface (surface.Get(), &props,
                                                        m_textureBitmap.GetAddressOf());
        CHRA (hr);

        m_textureW = widthPx;
        m_textureH = heightPx;
    }

    m_renderer.m_d2dContext->GetTarget (m_savedTarget.ReleaseAndGetAddressOf());
    m_renderer.m_d2dContext->SetTarget (m_textureBitmap.Get());
    m_renderer.m_d2dContext->BeginDraw();
    m_renderer.m_d2dContext->Clear (D2D1::ColorF (0.0f, 0.0f, 0.0f, 0.0f));
    m_renderer.m_drawing = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndDrawToTexture
//
//  Closes the off-screen draw, restores the previous target, and hands back the
//  view. The view belongs to the renderer and is replaced by the next
//  BeginDrawToTexture, so a caller that keeps one keeps a reference.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderTargets::EndDrawToTexture (ID3D11ShaderResourceView ** outSrv)
{
    HRESULT   hr = S_OK;



    DXUI_ASSERT_UI_THREAD();

    CBRAEx (outSrv, E_INVALIDARG);

    *outSrv = nullptr;

    CBRA (m_renderer.m_d2dContext);
    CBRA (m_renderer.m_drawing);

    hr = m_renderer.m_d2dContext->EndDraw();
    m_renderer.m_drawing = false;

    m_renderer.m_d2dContext->SetTarget (m_savedTarget.Get());
    m_savedTarget.Reset();

    CHRA (hr);

    *outSrv = m_textureSrv.Get();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDrawToTextureSize
//
//  Reports the off-screen target's real dimensions, which is what a caller
//  needs to map texture coordinates over the part of it that was drawn. The
//  texture is grown and never shrunk, so this is >= whatever the last
//  BeginDrawToTexture asked for, and the difference is uninitialized surface
//  a 0..1 mapping would sample.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRenderTargets::GetDrawToTextureSize (UINT & outWidthPx, UINT & outHeightPx) const
{
    outWidthPx  = (m_textureTarget != nullptr) ? m_textureW : 0;
    outHeightPx = (m_textureTarget != nullptr) ? m_textureH : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginDrawDeferred
//
//  Points the context at a fresh command list, so everything this pass draws
//  is RECORDED rather than rasterized. EndDrawDeferred replays it.
//
//  This is what the interleaved page pass needs. Direct2D flushes a batch
//  whenever it likes, so text aimed straight at the back buffer can land
//  before the painter's fills and be covered by them. A command list holds no
//  pixels, so an early flush has nowhere to put anything and the recording
//  simply continues.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderTargets::BeginDrawDeferred()
{
    HRESULT  hr = S_OK;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    CBRA (m_renderer.m_targetBound);

    // A closed list cannot be reopened, so the pass gets its own.
    m_cmdList.Reset();

    hr = m_renderer.m_d2dContext->CreateCommandList (m_cmdList.GetAddressOf());
    CHRA (hr);

    m_renderer.m_d2dContext->SetTarget (m_cmdList.Get());
    m_renderer.m_d2dContext->BeginDraw();
    m_renderer.m_drawing = true;

Error:
    if (FAILED (hr))
    {
        m_cmdList.Reset();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndDrawDeferred
//
//  Closes the recording, puts the back buffer back, and draws the recording
//  onto it. Replay happens AFTER the caller has flushed its own geometry,
//  which is the whole point: the text lands on top no matter how much of it
//  there was.
//
//  Calling this without a matching BeginDrawDeferred is a no-op, matching
//  EndDraw, so an early-out paint path need not track whether it began.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderTargets::EndDrawDeferred()
{
    HRESULT  hr           = S_OK;
    HRESULT  hrEnd        = S_OK;
    bool     isTargetLost = false;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    BAIL_OUT_IF (!m_renderer.m_drawing, S_OK);

    hrEnd        = m_renderer.m_d2dContext->EndDraw();
    m_renderer.m_drawing    = false;
    isTargetLost = (hrEnd == D2DERR_RECREATE_TARGET);

    // The target goes back FIRST and unconditionally. Leaving the context
    // aimed at a command list would strand every later pass on a recording
    // nothing replays.
    m_renderer.m_d2dContext->SetTarget (m_renderer.m_target.Get());

    if (isTargetLost)
    {
        m_renderer.UnbindBackBuffer();
    }

    // The recording is dropped rather than replayed when the pass that made
    // it failed: half a frame's text over a frame the caller may not have
    // finished is worse than none of it.
    BAIL_OUT_IF (isTargetLost, S_OK);

    hr = hrEnd;
    CHRA (hr);

    CBRA (m_cmdList);

    hr = m_cmdList->Close();
    CHRA (hr);

    m_renderer.m_d2dContext->BeginDraw();
    m_renderer.m_d2dContext->DrawImage (m_cmdList.Get());
    hrEnd        = m_renderer.m_d2dContext->EndDraw();
    isTargetLost = (hrEnd == D2DERR_RECREATE_TARGET);

    if (isTargetLost)
    {
        m_renderer.UnbindBackBuffer();
    }

    BAIL_OUT_IF (isTargetLost, S_OK);

    hr = hrEnd;
    CHRA (hr);

Error:
    m_cmdList.Reset();
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginDrawOffscreen
//
//  Drop-in alternative to BeginDraw that targets a private offscreen
//  bitmap instead of the swap-chain back-buffer surface. Paired with
//  EndDrawComposite, which flushes the offscreen bitmap and then blits
//  it onto the bound back buffer in a single DrawBitmap.
//
//  Why this exists, and why the page pass uses it rather than BeginDraw:
//
//  Direct2D does not hold a batch until EndDraw. Past a few hundred draws it
//  flushes on its own, and when the target IS the back buffer everything
//  recorded before that flush is already on it. That breaks a caller who
//  interleaves -- one panel-tree walk emits D2D text and D3D fills together,
//  and the fills only end up underneath because the painter flushes first.
//  Text caught by an early flush lands before them and is covered: the menu
//  bar, first to paint and sitting on a filled strip, vanished outright once
//  four glowing labels pushed the frame past the threshold.
//
//  The same mechanism accounts for the middle band Direct2D was seen dropping
//  on ARM64 with a large flip-model surface.
//
//  Against a private bitmap an early flush is harmless -- it lands in the
//  layer -- and the layer composites once, after the fills.
//
//  The offscreen bitmap is cached and only recreated when the target
//  pixel size changes. It is cleared transparent so the composite
//  alpha-blends cleanly over whatever D3D geometry was drawn earlier
//  in the frame.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderTargets::BeginDrawOffscreen()
{
    HRESULT                  hr    = S_OK;
    D2D1_SIZE_U              size  = {};
    D2D1_BITMAP_PROPERTIES1  props = {};
    float                    dpiX  = 96.0f;
    float                    dpiY  = 96.0f;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    CBRA (m_renderer.m_targetBound);

    size = m_renderer.m_target->GetPixelSize();

    // THE LAYER TAKES THE TARGET'S DPI, not 96. Callers pass DIPs, and the
    // back buffer has the display's DPI baked in by BindBackBuffer, so a layer
    // fixed at 96 would put every glyph at 96/dpi of its position and size --
    // invisible on a 96-dpi display, half-scale chrome on a 200% one.
    m_renderer.m_d2dContext->GetDpi (&dpiX, &dpiY);

    if (m_offscreen == nullptr || m_offscreenW != size.width || m_offscreenH != size.height)
    {
        m_offscreen.Reset();

        props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        props.dpiX                  = (dpiX > 0.0f) ? dpiX : 96.0f;
        props.dpiY                  = (dpiY > 0.0f) ? dpiY : 96.0f;
        props.bitmapOptions         = D2D1_BITMAP_OPTIONS_TARGET;

        hr = m_renderer.m_d2dContext->CreateBitmap (size, nullptr, 0, &props, &m_offscreen);
        CHRA (hr);

        m_offscreenW = size.width;
        m_offscreenH = size.height;
    }

    m_renderer.m_d2dContext->SetTarget (m_offscreen.Get());
    m_renderer.m_d2dContext->BeginDraw();
    m_renderer.m_d2dContext->Clear (D2D1::ColorF (0.0f, 0.0f, 0.0f, 0.0f));
    m_renderer.m_drawing = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndDrawComposite
//
//  Flushes the offscreen bitmap started by BeginDrawOffscreen, then
//  rebinds the back-buffer target and draws the offscreen bitmap onto
//  it 1:1 (nearest-neighbor, no scaling). On D2DERR_RECREATE_TARGET
//  the target is dropped so the next BindBackBuffer rebuilds it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderTargets::EndDrawComposite()
{
    HRESULT      hr           = S_OK;
    HRESULT      hrEnd        = S_OK;
    D2D1_SIZE_U  size         = {};
    D2D1_RECT_F  dest         = {};
    float        dpiX         = 96.0f;
    float        dpiY         = 96.0f;
    bool         isTargetLost = false;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    BAIL_OUT_IF (!m_renderer.m_drawing, S_OK);

    hrEnd        = m_renderer.m_d2dContext->EndDraw();
    m_renderer.m_drawing    = false;
    isTargetLost = (hrEnd == D2DERR_RECREATE_TARGET);

    if (isTargetLost)
    {
        m_renderer.UnbindBackBuffer();
        m_offscreen.Reset();
        m_offscreenW = 0;
        m_offscreenH = 0;
    }

    BAIL_OUT_IF (isTargetLost, S_OK);

    hr = hrEnd;
    CHRA (hr);

    size = m_renderer.m_target->GetPixelSize();

    // DIPS, NOT PIXELS. Both rects are read in DIPs -- the source in the
    // layer's, the destination in the back buffer's -- and the two agree
    // because BeginDrawOffscreen gives the layer the target's DPI. Passing the
    // pixel size drew a region dpi/96 times too large from a source that small,
    // which is a 1:1 blit only on a 96-dpi display.
    m_renderer.m_d2dContext->GetDpi (&dpiX, &dpiY);

    dest = D2D1::RectF (0.0f, 0.0f,
                        (float) size.width  * 96.0f / ((dpiX > 0.0f) ? dpiX : 96.0f),
                        (float) size.height * 96.0f / ((dpiY > 0.0f) ? dpiY : 96.0f));

    m_renderer.m_d2dContext->SetTarget (m_renderer.m_target.Get());
    m_renderer.m_d2dContext->BeginDraw();
    m_renderer.m_d2dContext->DrawBitmap (m_offscreen.Get(),
                              &dest,
                              1.0f,
                              D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                              &dest);
    hrEnd        = m_renderer.m_d2dContext->EndDraw();
    isTargetLost = (hrEnd == D2DERR_RECREATE_TARGET);

    if (isTargetLost)
    {
        m_renderer.UnbindBackBuffer();
    }

    BAIL_OUT_IF (isTargetLost, S_OK);

    hr = hrEnd;
    CHRA (hr);

Error:
    return hr;
}
