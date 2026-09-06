#include "Pch.h"

#include "D3DRenderer.h"

// The blit pair, compiled to bytecode by fxc at build time (see
// Casso.vcxproj). The CRT chain shares this same vertex shader.
#include "blit.vs.h"
#include "blit.ps.h"

#include "PerfStats.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Vertex structure
//
////////////////////////////////////////////////////////////////////////////////

struct Vertex
{
    float x;
    float y;
    float u;
    float v;
};





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope constants
//
////////////////////////////////////////////////////////////////////////////////

// Frames to keep re-rendering after the emulator framebuffer goes
// idle, so the persistence trail finishes decaying. At 60 fps,
// 90 frames = 1.5s; 0.8^90 is < UNORM precision even before the bias.
static constexpr int  s_kPersistenceSettleFrames = 90;





////////////////////////////////////////////////////////////////////////////////
//
//  D3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

D3DRenderer::D3DRenderer()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~D3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

D3DRenderer::~D3DRenderer()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Adopts the externally-owned device, context, and swap chain
//  (typically DxuiHwndSource's) rather than creating its own. Builds
//  the upload texture, sampler, shaders, vertex / index buffers, and
//  CRT post-process chain, but holds no back-buffer RTV of its own.
//  Callers invoke UploadAndComposite once per frame; the host owns
//  Present.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::Initialize (
    ID3D11Device          * pDevice,
    ID3D11DeviceContext   * pContext,
    IDXGISwapChain1       * pSwapChain,
    int                     texWidth,
    int                     texHeight,
    const RECT            & initialTargetRect)
{
    HRESULT                hr             = S_OK;
    DXGI_SWAP_CHAIN_DESC1  scd            = {};
    int                    initialW       = 0;
    int                    initialH       = 0;



    CBRAEx (pDevice,    E_INVALIDARG);
    CBRAEx (pContext,   E_INVALIDARG);
    CBRAEx (pSwapChain, E_INVALIDARG);

    m_device            = pDevice;
    m_context           = pContext;
    m_texWidth          = texWidth;
    m_texHeight         = texHeight;
    m_targetBoundsPx    = initialTargetRect;

    // SetSourceSize lives on IDXGISwapChain2; QI up so the CRT
    // post-process can drive DWM's presented sub-rect. Every
    // Windows 8.1+ system Casso targets exposes IDXGISwapChain2.
    hr = pSwapChain->QueryInterface (IID_PPV_ARGS (m_swapChain.GetAddressOf()));
    CHRA (hr);

    // Pull initial dimensions from the swap chain itself; the host
    // sized it to match its client area at Create() time, so this
    // matches what the host will eventually composite into.
    hr = m_swapChain->GetDesc1 (&scd);
    CHRA (hr);

    initialW = static_cast<int> (scd.Width);
    initialH = static_cast<int> (scd.Height);

    m_physicalBackBufferW = initialW;
    m_physicalBackBufferH = initialH;
    m_backBufferW         = initialW;
    m_backBufferH         = initialH;

    hr = CreateRenderResources (texWidth, texHeight);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateRenderResources
//
//  Post-device-adoption pipeline setup invoked by Initialize after the
//  externally-owned device + swap chain are adopted. Builds the dynamic
//  upload texture, sampler, shader programs, vertex / index buffers, and
//  CRT post-process chain. The renderer holds no back-buffer RTV of its
//  own -- it composites into the host's RTV (passed to
//  UploadAndComposite) -- and does NOT call SetSourceSize.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::CreateRenderResources (int texWidth, int texHeight)
{
    HRESULT                  hr        = S_OK;
    D3D11_TEXTURE2D_DESC     td        = {};
    D3D11_SAMPLER_DESC       sd        = {};
    D3D11_BUFFER_DESC        bd        = {};
    D3D11_SUBRESOURCE_DATA   initData  = {};



    Vertex vertices[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f },  // Top-left
        {  1.0f,  1.0f, 1.0f, 0.0f },  // Top-right
        { -1.0f, -1.0f, 0.0f, 1.0f },  // Bottom-left
        {  1.0f, -1.0f, 1.0f, 1.0f },  // Bottom-right
    };

    UINT16 indices[] = { 0, 1, 2, 2, 1, 3 };



    td.Width            = static_cast<UINT> (texWidth);
    td.Height           = static_cast<UINT> (texHeight);
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DYNAMIC;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateTexture2D (&td, nullptr, m_texture.GetAddressOf());
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_texture.Get(), nullptr, m_srv.GetAddressOf());
    CHRA (hr);

    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = m_device->CreateSamplerState (&sd, m_sampler.GetAddressOf());
    CHRA (hr);

    hr = InitializeShaders();
    CHRA (hr);

    bd.ByteWidth = sizeof (vertices);
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    initData.pSysMem = vertices;

    hr = m_device->CreateBuffer (&bd, &initData, m_vertexBuffer.GetAddressOf());
    CHRA (hr);

    bd               = {};
    bd.ByteWidth     = sizeof (indices);
    bd.Usage         = D3D11_USAGE_DEFAULT;
    bd.BindFlags     = D3D11_BIND_INDEX_BUFFER;

    initData         = {};
    initData.pSysMem = indices;

    hr = m_device->CreateBuffer (&bd, &initData, m_indexBuffer.GetAddressOf());
    CHRA (hr);

    hr = m_crtPost.Initialize (m_device.Get(), m_context.Get());
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeShaders
//
//  Compiles the pass-through shader pair that blits the emulator framebuffer,
//  plus the quad geometry and sampler it needs.
//
//  These shaders do NOTHING but sample -- no transform, no tint. Positions
//  arrive already in clip space and the pixel shader returns the texel
//  unchanged. Every visual effect lives in the CRT post-process chain instead,
//  which keeps the effects editable in one place and this path trivially
//  correct.
//
//  The source is inline here, not embedded as a resource like the CRT shaders,
//  precisely because it is this short and is never edited for tuning.
//
//  The input layout is validated against the compiled vertex-shader blob, so a
//  mismatch between the vertex struct and the shader signature fails at
//  startup rather than as garbage geometry.
//
//  Everything asserts: a shader that will not compile is a broken build, not
//  something the user did.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::InitializeShaders()
{
    HRESULT            hr     = S_OK;



    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };



    // The shaders arrive as bytecode; nothing compiles HLSL here any more.
    hr = m_device->CreateVertexShader (g_BlitVs,
                                       sizeof (g_BlitVs),
                                       nullptr,
                                       &m_vertexShader);
    CHRA (hr);

    hr = m_device->CreatePixelShader (g_BlitPs,
                                      sizeof (g_BlitPs),
                                      nullptr,
                                      &m_pixelShader);
    CHRA (hr);

    // Create input layout
    hr = m_device->CreateInputLayout (layout,
                                      2,
                                      g_BlitVs, sizeof (g_BlitVs),
                                      &m_inputLayout);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NeedsPresent
//
////////////////////////////////////////////////////////////////////////////////

bool D3DRenderer::NeedsPresent (bool framebufferDirty) const
{
    // Three independent reasons to present, in cheapest-test-first order:
    //
    //   1. New emulator content, or a forced redraw.
    //   2. The persistence shader animates a fading trail every frame even
    //      when the framebuffer has not changed. Keep re-rendering until the
    //      trail is fully decayed -- ~1.5s at the highest decay (amber's 0.8)
    //      with the UNORM bias is more than enough.
    //   3. Any other slider / toggle change, which touches CrtParams.
    return framebufferDirty
        || m_redrawForced
        || (m_crtParams.persistence > 0.0f && m_idleFramesSinceFbChange < s_kPersistenceSettleFrames)
        || memcmp (&m_crtParams, &m_lastPresentedParams, sizeof (CrtParams)) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UploadAndComposite
//
//  Uploads the framebuffer and runs the CRT post-process pass, then
//  skips the swap-chain Present -- the host owns the Present call --
//  and the after-blit chrome hook, since chrome paints via the host's
//  panel-tree Paint pump rather than this renderer's hook.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::UploadAndComposite (ID3D11RenderTargetView * dstRtv, const uint32_t * framebuffer)
{
    HRESULT                    hr            = S_OK;
    D3D11_MAPPED_SUBRESOURCE   mapped        = {};
    const uint32_t           * src           = nullptr;
    Byte                     * dst           = nullptr;
    RECT                       contentRect   = {};



    BAIL_OUT_IF (m_context == nullptr || dstRtv == nullptr, S_OK);
    BAIL_OUT_IF (m_deviceRemoved,                           S_OK);

    // Minimized (or mid-resize to zero): nothing to composite, and the CRT
    // post-process rejects the empty target. Skip; resumes on restore.
    BAIL_OUT_IF (m_backBufferW <= 0 || m_backBufferH <= 0,   S_OK);

    if (m_texture != nullptr && framebuffer != nullptr)
    {
        hr = m_context->Map (m_texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        CHRA (hr);

        src = framebuffer;
        dst = static_cast<Byte *> (mapped.pData);

        for (int y = 0; y < m_texHeight; y++)
        {
            memcpy (dst, src, static_cast<size_t> (m_texWidth) * 4);
            src += m_texWidth;
            dst += mapped.RowPitch;
        }

        m_context->Unmap (m_texture.Get(), 0);
    }

    // No full-buffer clear here: the host's PaintPump already cleared
    // the back buffer to the theme background before invoking this
    // hook, and the CRT final pass writes the full back buffer
    // (emulator frame plus black letterbox bars) into dstRtv.

    // The target rectangle comes from the DxuiViewport bounds (pushed
    // in by EmulatorShell), not from the chrome inset side-channel.
    // Fall back to the full back buffer if no viewport bounds have
    // been reported yet.
    if (m_targetBoundsPx.right > m_targetBoundsPx.left &&
        m_targetBoundsPx.bottom > m_targetBoundsPx.top)
    {
        contentRect = m_targetBoundsPx;
    }
    else
    {
        contentRect.left   = 0;
        contentRect.top    = 0;
        contentRect.right  = m_backBufferW;
        contentRect.bottom = m_backBufferH;
    }

    hr = RenderCrtFrame (dstRtv, contentRect, m_backBufferW, m_backBufferH);
    CHRA (hr);

    m_redrawForced        = false;
    m_lastPresentedParams = m_crtParams;

    if (framebuffer != nullptr)
    {
        m_idleFramesSinceFbChange = 0;
    }
    else
    {
        m_idleFramesSinceFbChange++;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UploadAndCompositeOffscreen
//
//  The desk-scene variant of UploadAndComposite: the CRT chain's output lands
//  in this renderer's own offscreen target for the scene to sample on the
//  monitor glass, instead of the host's back buffer. The target is cleared to
//  opaque black first -- unlike the back buffer, no host clear ever covers it,
//  and the letterbox margins around the fitted rect must read as black glass
//  rather than stale frames.
//
//  The target is exactly the PICTURE's size, not the window's. It used to be
//  window-sized with the picture in one corner, which meant all nine of the
//  chain's passes swept the whole window to produce an image occupying about
//  a twentieth of it -- measured at 93% of the process's entire GPU cost, and
//  essentially all of it spent filtering black pixels the glass never samples.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::UploadAndCompositeOffscreen (const uint32_t * framebuffer, const RECT & pictureRect)
{
    HRESULT                    hr       = S_OK;
    float                      black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    D3D11_MAPPED_SUBRESOURCE   mapped   = {};
    const uint32_t           * src      = nullptr;
    Byte                     * dst      = nullptr;
    int                        pictureW = (int) (pictureRect.right  - pictureRect.left);
    int                        pictureH = (int) (pictureRect.bottom - pictureRect.top);



    BAIL_OUT_IF (m_context == nullptr,                     S_OK);
    BAIL_OUT_IF (m_deviceRemoved,                          S_OK);
    BAIL_OUT_IF (m_backBufferW <= 0 || m_backBufferH <= 0, S_OK);
    BAIL_OUT_IF (pictureW <= 0 || pictureH <= 0,           S_OK);

    hr = EnsureSceneContentTarget (pictureW, pictureH);
    CHRA (hr);

    m_context->ClearRenderTargetView (m_sceneRtv.Get(), black);

    if (m_texture != nullptr && framebuffer != nullptr)
    {
        hr = m_context->Map (m_texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        CHRA (hr);

        src = framebuffer;
        dst = static_cast<Byte *> (mapped.pData);

        for (int y = 0; y < m_texHeight; y++)
        {
            memcpy (dst, src, static_cast<size_t> (m_texWidth) * 4);
            src += m_texWidth;
            dst += mapped.RowPitch;
        }

        m_context->Unmap (m_texture.Get(), 0);
    }

    hr = RenderCrtFrame (m_sceneRtv.Get(), pictureRect, pictureW, pictureH);
    CHRA (hr);

    // Remembered for the screenshot readback: a Crt capture wants exactly the
    // region the picture was just rendered into, and recomputing it there
    // would be a second copy of this path's geometry to keep in step.
    m_scenePictureRectPx  = pictureRect;

    m_redrawForced        = false;
    m_lastPresentedParams = m_crtParams;

    if (framebuffer != nullptr)
    {
        m_idleFramesSinceFbChange = 0;
    }
    else
    {
        m_idleFramesSinceFbChange++;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureSceneContentTarget
//
//  Sized to the PICTURE, so the chain's passes cover exactly the pixels the
//  glass will sample and the scene's UVs span the whole texture. See
//  UploadAndCompositeOffscreen for what a window-sized target cost.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::EnsureSceneContentTarget (int width, int height)
{
    HRESULT               hr   = S_OK;
    D3D11_TEXTURE2D_DESC  desc = {};



    CBRAEx (width > 0 && height > 0, E_INVALIDARG);

    BAIL_OUT_IF (m_sceneTex != nullptr && m_sceneTexW == width && m_sceneTexH == height, S_OK);

    m_sceneTex.Reset();
    m_sceneRtv.Reset();
    m_sceneSrv.Reset();

    desc.Width            = (UINT) width;
    desc.Height           = (UINT) height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    // EIGHT BITS, deliberately, even though the chain that fills it now works
    // in ten.
    //
    // This target looks like scratch and is not: it is where the picture
    // becomes eight-bit for the desk scene, and the glass is sized to sample
    // it at roughly one texel per pixel, so the dither the final CRT pass
    // lays down here lands almost exactly on the output grid. Ten bits here
    // moves that boundary downstream to the glass draw instead, which is a
    // magnifying sample through a curved mesh -- the dither would arrive
    // smeared and off-grid, and the rounding it was meant to scatter would
    // happen somewhere it no longer covers.
    //
    // Widening this was tried and reverted: it put the banding back on the
    // desk scene, which is the presentation the whole fix exists for. The flat
    // path is unaffected either way -- it never touches this target, and its
    // final blit goes straight to the back buffer.
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_device->CreateTexture2D (&desc, nullptr, m_sceneTex.GetAddressOf());
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (m_sceneTex.Get(), nullptr, m_sceneRtv.GetAddressOf());
    CHRA (hr);

    hr = m_device->CreateShaderResourceView (m_sceneTex.Get(), nullptr, m_sceneSrv.GetAddressOf());
    CHRA (hr);

    m_sceneTexW = width;
    m_sceneTexH = height;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBackRegion
//
//  A rectangle of GPU pixels brought home.
//
//  THE FIRST READBACK IN THIS TREE. Everything else here uploads: every other
//  Map in Casso and Dxui is WRITE_DISCARD. So the shape is worth stating --
//  a GPU texture cannot be mapped for reading, and a staging texture cannot be
//  a render target, which is why this is a copy followed by a map rather than
//  either one alone.
//
//  ROW PITCH IS NOT WIDTH * 4. The driver pads rows to its own alignment, so
//  the rows are copied one at a time out of the mapped span rather than in a
//  single memcpy of the whole thing. Getting this wrong produces an image
//  sheared diagonally, which looks like a rendering bug rather than a copy bug.
//
//  The region is clamped to what the source actually holds. Rects here come
//  from panel layout that a resize may have moved on since, and a
//  CopySubresourceRegion running off the end of a texture is a device-removal
//  offense -- expensive for something a clamp handles.
//
//  Single exit is doing real work: the map MUST be released on every path, and
//  an early return anywhere below leaves the texture mapped for the life of
//  the process.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::ReadBackRegion (ID3D11Texture2D * source,
                                     const RECT      & regionPx,
                                     CapturedImage   & outImage)
{
    HRESULT                    hr        = S_OK;
    D3D11_TEXTURE2D_DESC       srcDesc   = {};
    D3D11_TEXTURE2D_DESC       dstDesc   = {};
    D3D11_MAPPED_SUBRESOURCE   mapped    = {};
    D3D11_BOX                  box       = {};
    ComPtr<ID3D11Texture2D>    staging;
    const Byte *               src       = nullptr;
    Byte *                     dst       = nullptr;
    LONG                       left      = 0;
    LONG                       top       = 0;
    LONG                       right     = 0;
    LONG                       bottom    = 0;
    int                        width     = 0;
    int                        height    = 0;
    int                        y         = 0;
    size_t                     rowBytes  = 0;
    bool                       mappedOk  = false;
    bool                       haveSize  = false;



    CBRAEx (source != nullptr, E_INVALIDARG);
    CBRAEx (m_device != nullptr && m_context != nullptr, E_UNEXPECTED);

    source->GetDesc (&srcDesc);

    left   = max (0L, regionPx.left);
    top    = max (0L, regionPx.top);
    right  = min ((LONG) srcDesc.Width,  regionPx.right);
    bottom = min ((LONG) srcDesc.Height, regionPx.bottom);

    width  = (int) (right - left);
    height = (int) (bottom - top);

    haveSize = (width > 0) && (height > 0);
    CBREx (haveSize, E_INVALIDARG);

    dstDesc.Width          = (UINT) width;
    dstDesc.Height         = (UINT) height;
    dstDesc.MipLevels      = 1;
    dstDesc.ArraySize      = 1;
    dstDesc.Format         = srcDesc.Format;
    dstDesc.SampleDesc.Count = 1;
    dstDesc.Usage          = D3D11_USAGE_STAGING;
    dstDesc.BindFlags      = 0;
    dstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = m_device->CreateTexture2D (&dstDesc, nullptr, staging.GetAddressOf());
    CHRA (hr);

    box.left   = (UINT) left;
    box.top    = (UINT) top;
    box.front  = 0;
    box.right  = (UINT) right;
    box.bottom = (UINT) bottom;
    box.back   = 1;

    m_context->CopySubresourceRegion (staging.Get(), 0, 0, 0, 0, source, 0, &box);

    hr = m_context->Map (staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    CHRA (hr);
    mappedOk = true;

    outImage.widthPx  = width;
    outImage.heightPx = height;
    rowBytes          = (size_t) width * CapturedImage::kBytesPerPixel;
    outImage.bgra.resize (rowBytes * height);

    src = (const Byte *) mapped.pData;
    dst = outImage.bgra.data();

    for (y = 0; y < height; y++)
    {
        memcpy (dst + ((size_t) y * rowBytes), src + ((size_t) y * mapped.RowPitch), rowBytes);
    }

Error:
    if (mappedOk)
    {
        m_context->Unmap (staging.Get(), 0);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureBackBufferRegion
//
//  The composed window, which is where the desk scene exists -- the CRT
//  chain's own target holds only the picture.
//
//  Must be called BEFORE Present. The swap chain is FLIP_DISCARD, so a
//  presented back buffer's contents are undefined by definition and this would
//  read whatever the queue happened to hand back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::CaptureBackBufferRegion (const RECT & regionPx, CapturedImage & outImage)
{
    HRESULT                   hr = S_OK;
    ComPtr<ID3D11Texture2D>   backBuffer;



    CBRAEx (m_swapChain != nullptr, E_UNEXPECTED);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (backBuffer.GetAddressOf()));
    CHRA (hr);

    hr = ReadBackRegion (backBuffer.Get(), regionPx, outImage);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureSceneTargetRegion
//
//  The CRT chain's offscreen target: the picture with its effects and nothing
//  around it. Only populated on the desk-scene path, so a caller reaching here
//  under a flat theme has resolved the wrong source.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::CaptureSceneTargetRegion (const RECT & regionPx, CapturedImage & outImage)
{
    HRESULT   hr = S_OK;



    CBRAEx (m_sceneTex != nullptr, E_UNEXPECTED);

    hr = ReadBackRegion (m_sceneTex.Get(), regionPx, outImage);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderCrtFrame
//
//  Aspect-fits the emulator content into `contentRect`, caches the
//  resulting on-screen rect (for hit-testing / preview overlap), and
//  runs the CRT post-process pass into `dstRtv`. Scoped so the perf
//  timer measures the post-process pass alone.
//
//  The framebuffer's own size goes along with the fitted rect because the
//  chain measures its blur radii in emulated pixels, and those two are what
//  say how large one of those is on this target.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::RenderCrtFrame (ID3D11RenderTargetView * dstRtv,
                                     const RECT             & contentRect,
                                     int                      targetW,
                                     int                      targetH)
{
    HRESULT          hr         = S_OK;
    RECT             fittedRect = {};
    ScopedPerfTimer  timer ("D3DRenderer.CrtPostProcess");



    if (m_texWidth > 0 && m_texHeight > 0 && targetW > 0 && targetH > 0)
    {
        fittedRect = ComputeAspectFitRectInRect (contentRect, m_texWidth, m_texHeight);
    }

    CacheEmulatorContentScreenRect (fittedRect);

    hr = m_crtPost.Process (m_srv.Get(),
                            dstRtv,
                            m_crtParams,
                            fittedRect,
                            targetW,
                            targetH,
                            m_texWidth,
                            m_texHeight);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CacheEmulatorContentScreenRect
//
//  Records where the emulator image lands in SCREEN coordinates, for consumers
//  that work outside the window's client space.
//
//  The HWND is queried from the swap chain's OutputWindow on demand rather
//  than cached alongside it. The host-owned swap chain is HWND-based, so the
//  authoritative answer already lives there, and a cached copy could go stale
//  across a device or window rebuild.
//
//  The rect is CLEARED before anything else, so every early exit leaves an
//  empty rect that callers read as "not available" rather than a stale one
//  describing a previous size or position.
//
////////////////////////////////////////////////////////////////////////////////

void D3DRenderer::CacheEmulatorContentScreenRect (const RECT & fittedRect)
{
    HRESULT                hr     = S_OK;
    POINT                  origin = {};
    BOOL                   ok     = FALSE;
    DXGI_SWAP_CHAIN_DESC   scd    = {};
    HWND                   hwnd   = nullptr;



    m_emulatorContentScreenRect = {};
    BAIL_OUT_IF (!m_swapChain || IsRectEmpty (&fittedRect), S_OK);

    // The HWND is the swap chain's OutputWindow -- the host-owned swap
    // chain is HWND-based (CreateSwapChainForHwnd), so we query it on
    // demand rather than caching a copy that could go stale.
    hr = m_swapChain->GetDesc (&scd);
    CHRA (hr);

    hwnd = scd.OutputWindow;
    BAIL_OUT_IF (hwnd == nullptr, S_OK);

    ok = ClientToScreen (hwnd, &origin);
    CWRA (ok);

    m_emulatorContentScreenRect.left   = fittedRect.left   + origin.x;
    m_emulatorContentScreenRect.top    = fittedRect.top    + origin.y;
    m_emulatorContentScreenRect.right  = fittedRect.right  + origin.x;
    m_emulatorContentScreenRect.bottom = fittedRect.bottom + origin.y;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleFullscreen
//
//  Borderless-fullscreen toggle. Every guard below exists because a specific
//  way of getting it wrong strands the user behind an unescapable full-monitor
//  popup covering the taskbar and every other window.
//
//  RE-ENTRANCY. A modal loop opening mid-transition -- an assert dialog, a
//  message box -- pumps messages and can dispatch a queued Alt+Enter into a
//  NESTED toggle. That nested enter would capture the already-borderless state
//  as the "windowed" state to restore, so there is nothing left to go back to.
//  Toggles are ignored while one is in flight, and `armed` gates the cleanup so
//  a bailed nested call cannot disarm the outer toggle's guard.
//
//  DIRECTION is decided from the flag AND the actual window style, not from
//  the flag alone. A caption-less window IS fullscreen whatever the flag says,
//  and acting on a stale flag is the other route into the trap. A desync means
//  a previous transition failed half-way or something else restyled the
//  window, so it asserts for a debug build and then recovers toward a windowed
//  state -- recovery beats consistency here.
//
//  FLAG ORDERING. m_fullscreen is set true BEFORE the window is touched, and
//  stays true through the restore. SetWindowPos delivers WM_SIZE
//  synchronously, and the resize path consults IsFullscreen to decide whether
//  to persist placement. With the flag still false on entry, the full-monitor
//  rect was saved as the user's windowed placement -- permanently stomping
//  their real window size in prefs. On exit it drops only once the window is
//  back, so no mid-transition rect is persisted either.
//
//  The full window PLACEMENT is saved rather than just the current rect, so a
//  maximized window round-trips back to maximized with its underlying normal
//  size intact.
//
//  If the desync recovery runs before any successful entry ever captured a
//  placement, the restore falls back to a stock overlapped style and normal
//  show -- the user always gets a movable, closable window back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::ToggleFullscreen (HWND hwnd)
{
    HRESULT     hr         = S_OK;
    HMONITOR    hMon       = nullptr;
    MONITORINFO mi         = { sizeof (mi) };
    LONG        style      = 0;
    LONG        priorStyle = 0;
    bool        styleIsFs  = false;
    bool        armed      = false;
    BOOL        fSuccess   = FALSE;



    // A modal loop opening mid-transition (assert dialog, message box) pumps
    // messages and can dispatch a queued Alt+Enter into a NESTED toggle. A
    // nested enter would capture the borderless fullscreen state as the
    // "windowed" state to restore, stranding the user in an unescapable
    // full-monitor popup that covers the taskbar and every other window.
    // Ignore toggles while one is in flight. `armed` gates the Error-path
    // clear so a bailed nested call does not disarm the OUTER toggle's guard.
    BAIL_OUT_IF (m_fsTransition, S_OK);
    m_fsTransition = true;
    armed          = true;

    // Decide direction from the flag AND the actual window state. A caption-
    // less style IS fullscreen whatever the flag says; acting on a stale flag
    // here is how a user ends up trapped behind a borderless full-monitor
    // popup. Desync means a transition previously failed half-way (or someone
    // else restyled the window) -- assert so a debug build surfaces it, then
    // recover by restoring a windowed state.
    style     = GetWindowLong (hwnd, GWL_STYLE);
    styleIsFs = ((style & WS_CAPTION) != WS_CAPTION);
    ASSERT (styleIsFs == m_fullscreen);

    if (!m_fullscreen && !styleIsFs)
    {
        // Save the full windowed placement (not just the current rect) so a
        // maximized window round-trips back to maximized with its underlying
        // normal size intact -- and a normal window gets its exact size back.
        m_windowedStyle             = style;
        m_windowedPlacement.length  = sizeof (m_windowedPlacement);
        fSuccess = GetWindowPlacement (hwnd, &m_windowedPlacement);
        CWRA (fSuccess);

        hMon     = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
        fSuccess = GetMonitorInfo (hMon, &mi);
        CWRA (fSuccess);

        // Flip the flag BEFORE touching the window: SetWindowPos delivers
        // WM_SIZE synchronously, and the resize path consults IsFullscreen()
        // to decide whether to persist window placement / auto-resize chrome.
        // With the flag still false, entering fullscreen used to save the
        // full-monitor rect as the user's windowed placement -- permanently
        // stomping their real window size in prefs.
        m_fullscreen = true;

        SetLastError (0);
        priorStyle = SetWindowLong (hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        CWRA (priorStyle != 0);

        fSuccess = SetWindowPos (hwnd,
                                  HWND_TOP,
                                  mi.rcMonitor.left, mi.rcMonitor.top,
                                  mi.rcMonitor.right - mi.rcMonitor.left,
                                  mi.rcMonitor.bottom - mi.rcMonitor.top,
                                  SWP_FRAMECHANGED);
        CWRA (fSuccess);
    }
    else
    {
        // Restore the windowed style + placement. m_fullscreen stays true
        // through the restore so the synchronous WM_SIZE does not persist a
        // mid-transition rect; it drops only after the window is back. If the
        // desync recovery is running before any successful entry captured a
        // placement, fall back to a stock overlapped style + normal show so
        // the user always gets a movable, closable window back.
        LONG  restoreStyle = m_windowedStyle;

        m_fullscreen = true;

        if ((restoreStyle & WS_CAPTION) != WS_CAPTION)
        {
            restoreStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE;
        }

        SetLastError (0);
        priorStyle = SetWindowLong (hwnd, GWL_STYLE, restoreStyle);
        CWRA (priorStyle != 0);

        fSuccess = SetWindowPos (hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
        CWRA (fSuccess);

        if (m_windowedPlacement.length == sizeof (m_windowedPlacement))
        {
            fSuccess = SetWindowPlacement (hwnd, &m_windowedPlacement);
            CWRA (fSuccess);
        }
        else
        {
            // No captured placement (desync recovery): un-fullscreen to a
            // normal window without moving it; the user can take it from
            // there now that the caption is back.
            ShowWindow (hwnd, SW_SHOWNORMAL);
        }

        m_fullscreen = false;
    }

Error:
    if (armed)
    {
        m_fsTransition = false;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void D3DRenderer::Shutdown()
{
    m_crtPost.Shutdown();

    m_inputLayout.Reset();
    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_sampler.Reset();
    m_srv.Reset();
    m_texture.Reset();
    m_sceneTex.Reset();
    m_sceneRtv.Reset();
    m_sceneSrv.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();

    m_sceneTexW                  = 0;
    m_sceneTexH                  = 0;
    m_emulatorContentScreenRect  = {};
}


