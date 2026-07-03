#include "Pch.h"

#include "D3DRenderer.h"

#include "PerfStats.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
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
//  Smoke-test back-buffer dump helpers
//
////////////////////////////////////////////////////////////////////////////////

static constexpr wchar_t  s_kpszSmokeDumpEnv[]   = L"CASSO_SMOKE_DUMP_DIR";
static constexpr UINT64   s_kSmokeFrameStartup   = 60;
static constexpr UINT64   s_kSmokeFrameSettings  = 240;

// Frames to keep re-rendering after the emulator framebuffer goes
// idle, so the persistence trail finishes decaying. At 60 fps,
// 90 frames = 1.5s; 0.8^90 is < UNORM precision even before the bias.
static constexpr int      s_kPersistenceSettleFrames = 90;
static constexpr DWORD    s_kBmpHeaderSize       = sizeof (BITMAPFILEHEADER) + sizeof (BITMAPINFOHEADER);
static constexpr WORD     s_kBmpMagic            = 0x4D42;
static constexpr WORD     s_kBmpPlanes           = 1;
static constexpr WORD     s_kBmpBitsPerPixel     = 32;
static constexpr UINT     s_kMaxBoundPsSrvSlots  = 2;

// Floor for the flip-model back buffer's allocated size. Picked so the
// back buffer is large enough for typical monitor configurations on a
// fresh launch -- this lets SetSourceSize handle every reasonable
// resize without ever invoking ResizeBuffers on the hot drag path.
// Memory cost: 4096 * 4096 * 4 bytes * 2 buffers ~= 134 MB. Acceptable
// for the modern GPUs Casso targets, and the cost of NOT paying it is
// the driver-side crash that prompted the flip-model migration.
static constexpr int      s_kMinPhysicalBackBuffer = 4096;

// BufferCount for the flip-model swap chain. Flip-discard requires at
// least 2; we don't need triple-buffering.
static constexpr UINT     s_kFlipModelBufferCount  = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseInitialPhysicalBackBufferSize
//
//  Walks every attached display and returns the max pixel dimensions
//  observed (clamped to at least s_kMinPhysicalBackBuffer). The flip-
//  model swap chain is allocated at this size and SetSourceSize then
//  hands DWM the (logical) sub-rect we actually want presented, so a
//  drag-stress resize loop touches no GPU allocations at all.
//
////////////////////////////////////////////////////////////////////////////////

static BOOL CALLBACK MaxMonitorEnumProc (HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam)
{
    HRESULT       hr       = S_OK;
    SIZE        * pMax     = reinterpret_cast<SIZE *> (lParam);
    MONITORINFO   mi       = { sizeof (mi) };
    BOOL          fSuccess = FALSE;
    LONG          monW     = 0;
    LONG          monH     = 0;



    fSuccess = GetMonitorInfoW (hMonitor, &mi);
    CBRA (fSuccess);

    monW = mi.rcMonitor.right  - mi.rcMonitor.left;
    monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
    pMax->cx = std::max<LONG> (pMax->cx, monW);
    pMax->cy = std::max<LONG> (pMax->cy, monH);

Error:
    return TRUE;
}





static SIZE ChooseInitialPhysicalBackBufferSize (int initialClientW, int initialClientH)
{
    SIZE  monMax = { 0, 0 };
    SIZE  result = { s_kMinPhysicalBackBuffer, s_kMinPhysicalBackBuffer };

    EnumDisplayMonitors (nullptr, nullptr, MaxMonitorEnumProc, reinterpret_cast<LPARAM> (&monMax));

    result.cx = std::max<LONG> (result.cx, monMax.cx);
    result.cy = std::max<LONG> (result.cy, monMax.cy);
    result.cx = std::max<LONG> (result.cx, initialClientW);
    result.cy = std::max<LONG> (result.cy, initialClientH);
    return result;
}





static HRESULT DumpBackBufferBmp (
    ID3D11Device        * device,
    ID3D11DeviceContext * context,
    IDXGISwapChain      * swapChain,
    UINT64                frameIndex,
    UINT                  logicalW,
    UINT                  logicalH)
{
    HRESULT                   hr                = S_OK;
    wchar_t                   dumpDir[MAX_PATH] = {};
    DWORD                     chars             = 0;
    ComPtr<ID3D11Texture2D>   backBuffer;
    ComPtr<ID3D11Texture2D>   staging;
    D3D11_TEXTURE2D_DESC      desc              = {};
    D3D11_MAPPED_SUBRESOURCE  mapped            = {};
    std::filesystem::path     outPath;
    std::ofstream             out;
    BITMAPFILEHEADER          fileHeader        = {};
    BITMAPINFOHEADER          infoHeader        = {};
    LONG                      y                 = 0;
    UINT                      dumpW             = 0;
    UINT                      dumpH             = 0;



    chars = GetEnvironmentVariableW (s_kpszSmokeDumpEnv, dumpDir, ARRAYSIZE (dumpDir));
    BAIL_OUT_IF (chars == 0, S_OK);
    CBRA (device);
    CBRA (context);
    CBRA (swapChain);

    hr = swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    backBuffer->GetDesc (&desc);
    desc.Usage          = D3D11_USAGE_STAGING;
    desc.BindFlags      = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags      = 0;

    hr = device->CreateTexture2D (&desc, nullptr, &staging);
    CHRA (hr);

    context->CopyResource (staging.Get(), backBuffer.Get());

    hr = context->Map (staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    CHRA (hr);

    outPath = std::filesystem::path (dumpDir) / (frameIndex == s_kSmokeFrameSettings ? L"p3_1_frame240_backbuffer.bmp" : L"p3_1_frame60_backbuffer.bmp");
    out.open (outPath, std::ios::binary | std::ios::trunc);
    CBR (out.good());

    // Under the flip-model swap chain the back buffer is sized to the
    // physical max (often much larger than the visible area). Only the
    // top-left logicalW x logicalH region holds meaningful pixels; the
    // rest is undefined. Dump only the logical sub-rect so the smoke
    // image matches what the user actually sees on screen.
    dumpW = (logicalW > 0 && logicalW <= desc.Width)  ? logicalW : desc.Width;
    dumpH = (logicalH > 0 && logicalH <= desc.Height) ? logicalH : desc.Height;

    fileHeader.bfType    = s_kBmpMagic;
    fileHeader.bfOffBits = s_kBmpHeaderSize;
    fileHeader.bfSize    = s_kBmpHeaderSize + dumpW * dumpH * sizeof (uint32_t);

    infoHeader.biSize        = sizeof (BITMAPINFOHEADER);
    infoHeader.biWidth       = static_cast<LONG> (dumpW);
    infoHeader.biHeight      = -static_cast<LONG> (dumpH);
    infoHeader.biPlanes      = s_kBmpPlanes;
    infoHeader.biBitCount    = s_kBmpBitsPerPixel;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage   = dumpW * dumpH * sizeof (uint32_t);

    out.write (reinterpret_cast<const char *> (&fileHeader), sizeof (fileHeader));
    out.write (reinterpret_cast<const char *> (&infoHeader), sizeof (infoHeader));
    for (y = 0; y < static_cast<LONG> (dumpH); y++)
    {
        const char * row = reinterpret_cast<const char *> (static_cast<const Byte *> (mapped.pData) + mapped.RowPitch * y);
        out.write (row, dumpW * sizeof (uint32_t));
    }

Error:
    if (mapped.pData != nullptr)
    {
        context->Unmap (staging.Get(), 0);
    }

    return hr;
}





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
//  Initialize2
//
//  Adopt-mode: skip device + swap-chain creation and reuse the
//  externally-owned device, context, and swap chain (typically
//  DxuiHwndSource's). Still creates this renderer's own back-buffer
//  RTV, dynamic upload texture, sampler, shaders, vertex / index
//  buffers, and CRT post-process chain. Callers in this mode invoke
//  UploadAndComposite once per frame; the host owns Present.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::Initialize2 (
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

    // SetSourceSize lives on IDXGISwapChain2; QI up so we can share
    // the same hot-path resize trick the standalone Initialize uses.
    // Every Windows 8.1+ system Casso targets exposes IDXGISwapChain2.
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
//  Shared post-device-creation pipeline setup invoked by Initialize
//  (after the device + swap chain are stood up) and by Initialize2
//  (after the externally-owned device + swap chain are adopted).
//  Builds the back-buffer RTV, dynamic upload texture, sampler,
//  shader programs, vertex / index buffers, and CRT post-process
//  chain. Does NOT call SetSourceSize -- callers handle DWM
//  sub-rect negotiation themselves.
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



    // Host-owned (Initialize2 / adopt) mode is the only mode: the host
    // owns the swap chain and the only back-buffer RTV plus its resize
    // lifecycle. The renderer composites into the host's RTV (passed to
    // UploadAndComposite) and never holds a back-buffer reference of its
    // own, so the host's ResizeBuffers never contends with a stale
    // renderer reference.
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
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::InitializeShaders()
{
    HRESULT            hr     = S_OK;
    ComPtr<ID3DBlob>   vsBlob;
    ComPtr<ID3DBlob>   psBlob;
    ComPtr<ID3DBlob>   errors;

    static const char kVertexShaderSrc[] =
        "struct VSInput  { float2 pos : POSITION; float2 uv : TEXCOORD; };\n"
        "struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "VSOutput main (VSInput i)\n"
        "{\n"
        "    VSOutput o;\n"
        "    o.pos = float4 (i.pos, 0.0f, 1.0f);\n"
        "    o.uv  = i.uv;\n"
        "    return o;\n"
        "}\n";

    static const char kPixelShaderSrc[] =
        "Texture2D    tex : register(t0);\n"
        "SamplerState sam : register(s0);\n"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
        "float4 main (PSInput i) : SV_TARGET\n"
        "{\n"
        "    return tex.Sample (sam, i.uv);\n"
        "}\n";

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };



    // Compile vertex shader
    hr = D3DCompile (kVertexShaderSrc,
                     sizeof (kVertexShaderSrc) - 1,
                     "VS",
                     nullptr,
                     nullptr,
                     "main",
                     "vs_4_0",
                     0,
                     0,
                     &vsBlob,
                     &errors);
    CHRA (hr);

    // Compile pixel shader
    hr = D3DCompile (kPixelShaderSrc,
                     sizeof (kPixelShaderSrc) - 1,
                     "PS",
                     nullptr,
                     nullptr,
                     "main",
                     "ps_4_0",
                     0,
                     0,
                     &psBlob,
                     &errors);
    CHRA (hr);

    // Create vertex shader
    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(),
                                       vsBlob->GetBufferSize(),
                                       nullptr,
                                       &m_vertexShader);
    CHRA (hr);

    // Create pixel shader
    hr = m_device->CreatePixelShader (psBlob->GetBufferPointer(),
                                      psBlob->GetBufferSize(),
                                      nullptr,
                                      &m_pixelShader);
    CHRA (hr);

    // Create input layout
    hr = m_device->CreateInputLayout (layout,
                                      2,
                                      vsBlob->GetBufferPointer(),
                                      vsBlob->GetBufferSize(),
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
    if (framebufferDirty || m_redrawForced)
    {
        return true;
    }

    // Persistence shader animates a fading trail every frame even
    // when the emulator framebuffer hasn't changed. Keep re-rendering
    // until the trail is fully decayed -- ~1.5s at the highest decay
    // (amber's 0.8) with the UNORM bias is more than enough.
    if (m_crtParams.persistence > 0.0f && m_idleFramesSinceFbChange < s_kPersistenceSettleFrames)
    {
        return true;
    }

    // Any other slider / toggle change touches CrtParams.
    if (memcmp (&m_crtParams, &m_lastPresentedParams, sizeof (CrtParams)) != 0)
    {
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UploadAndComposite
//
//  Adopt-mode entry point (Initialize2). Uploads the framebuffer and
//  runs the CRT post-process pass, then skips the swap-chain Present
//  -- the host owns the Present call -- and the
//  after-blit chrome hook, since chrome paints via the host's panel-
//  tree Paint pump rather than this renderer's hook.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::UploadAndComposite (ID3D11RenderTargetView * dstRtv, const uint32_t * framebuffer)
{
    HRESULT                    hr            = S_OK;
    D3D11_MAPPED_SUBRESOURCE   mapped        = {};
    const uint32_t           * src           = nullptr;
    Byte                     * dst           = nullptr;
    RECT                       contentRect   = {};
    RECT                       fittedRect    = {};



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

    // In Initialize2 mode the target rectangle comes from the
    // DxuiViewport bounds (pushed in by EmulatorShell), not from
    // the chrome inset side-channel. Fall back to the full back
    // buffer if no viewport bounds have been reported yet.
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

    {
        ScopedPerfTimer  timer ("D3DRenderer.CrtPostProcess");


        if (m_texWidth > 0 && m_texHeight > 0 && m_backBufferW > 0 && m_backBufferH > 0)
        {
            fittedRect = ComputeAspectFitRectInRect (contentRect, m_texWidth, m_texHeight);
        }

        CacheEmulatorContentScreenRect (fittedRect);

        hr = m_crtPost.Process (m_srv.Get(),
                                dstRtv,
                                m_crtParams,
                                fittedRect,
                                m_backBufferW,
                                m_backBufferH);
        CHRA (hr);
    }

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
//  CacheEmulatorContentScreenRect
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

    // The HWND is the swap chain's OutputWindow -- both the standalone and
    // host-owned swap chains are HWND-based (CreateSwapChainForHwnd), so we
    // query it on demand rather than caching a copy that could go stale.
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
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::ToggleFullscreen (HWND hwnd)
{
    HRESULT     hr        = S_OK;
    HMONITOR    hMon;
    MONITORINFO mi        = { sizeof (mi) };
    BOOL        fSuccess  = FALSE;



    if (!m_fullscreen)
    {
        // Save windowed state
        m_windowedStyle = GetWindowLong (hwnd, GWL_STYLE);
        fSuccess = GetWindowRect (hwnd, &m_windowedRect);
        CWRA (fSuccess);

        // Go borderless fullscreen
        hMon      = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
        fSuccess = GetMonitorInfo (hMon, &mi);
        CWRA (fSuccess);

        SetWindowLong (hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        fSuccess = SetWindowPos (hwnd,
                                  HWND_TOP,
                                  mi.rcMonitor.left, mi.rcMonitor.top,
                                  mi.rcMonitor.right - mi.rcMonitor.left,
                                  mi.rcMonitor.bottom - mi.rcMonitor.top,
                                  SWP_FRAMECHANGED);
        CWRA (fSuccess);

        m_fullscreen = true;
    }
    else
    {
        // Restore windowed
        SetWindowLong (hwnd, GWL_STYLE, m_windowedStyle);
        fSuccess = SetWindowPos (hwnd,
                                  HWND_NOTOPMOST,
                                  m_windowedRect.left, m_windowedRect.top,
                                  m_windowedRect.right - m_windowedRect.left,
                                  m_windowedRect.bottom - m_windowedRect.top,
                                  SWP_FRAMECHANGED);
        CWRA (fSuccess);

        m_fullscreen = false;
    }

Error:
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
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();

    m_emulatorContentScreenRect  = {};
}


