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
static constexpr DWORD    s_kBmpHeaderSize       = sizeof (BITMAPFILEHEADER) + sizeof (BITMAPINFOHEADER);
static constexpr WORD     s_kBmpMagic            = 0x4D42;
static constexpr WORD     s_kBmpPlanes           = 1;
static constexpr WORD     s_kBmpBitsPerPixel     = 32;
static constexpr UINT     s_kMaxBoundPsSrvSlots  = 2;





static HRESULT DumpBackBufferBmp (
    ID3D11Device        * device,
    ID3D11DeviceContext * context,
    IDXGISwapChain      * swapChain,
    UINT64                frameIndex)
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

    fileHeader.bfType    = s_kBmpMagic;
    fileHeader.bfOffBits = s_kBmpHeaderSize;
    fileHeader.bfSize    = s_kBmpHeaderSize + desc.Width * desc.Height * sizeof (uint32_t);

    infoHeader.biSize        = sizeof (BITMAPINFOHEADER);
    infoHeader.biWidth       = static_cast<LONG> (desc.Width);
    infoHeader.biHeight      = -static_cast<LONG> (desc.Height);
    infoHeader.biPlanes      = s_kBmpPlanes;
    infoHeader.biBitCount    = s_kBmpBitsPerPixel;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage   = desc.Width * desc.Height * sizeof (uint32_t);

    out.write (reinterpret_cast<const char *> (&fileHeader), sizeof (fileHeader));
    out.write (reinterpret_cast<const char *> (&infoHeader), sizeof (infoHeader));
    for (y = 0; y < static_cast<LONG> (desc.Height); y++)
    {
        const char * row = reinterpret_cast<const char *> (static_cast<const Byte *> (mapped.pData) + mapped.RowPitch * y);
        out.write (row, desc.Width * sizeof (uint32_t));
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
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::Initialize (HWND hwnd, int texWidth, int texHeight)
{
    HRESULT                    hr                  = S_OK;
    DXGI_SWAP_CHAIN_DESC       scd                 = {};
    UINT                       createFlags         = 0;
    D3D_FEATURE_LEVEL          featureLevel;
    ComPtr<ID3D11Texture2D>    backBuffer;
    D3D11_VIEWPORT             vp                  = {};
    D3D11_TEXTURE2D_DESC       td                  = {};
    D3D11_SAMPLER_DESC         sd                  = {};
    D3D11_BUFFER_DESC          bd                  = {};
    D3D11_SUBRESOURCE_DATA     initData            = {};
    int                        initialBackBufferW  = texWidth;
    int                        initialBackBufferH  = texHeight;

    Vertex vertices[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f },  // Top-left
        {  1.0f,  1.0f, 1.0f, 0.0f },  // Top-right
        { -1.0f, -1.0f, 0.0f, 1.0f },  // Bottom-left
        {  1.0f, -1.0f, 1.0f, 1.0f },  // Bottom-right
    };

    UINT16 indices[] = { 0, 1, 2, 2, 1, 3 };



    m_texWidth  = texWidth;
    m_texHeight = texHeight;

    // The swap chain back buffer is sized to the host window's client area
    // rather than the emulator framebuffer dimensions. This gives the native
    // UI overlay a back buffer large enough to host chrome around the
    // letterboxed emulator output without DXGI rescaling at present time.
    {
        RECT  rcClient = {};


        if (GetClientRect (hwnd, &rcClient))
        {
            initialBackBufferW = std::max<int> (texWidth,  rcClient.right  - rcClient.left);
            initialBackBufferH = std::max<int> (texHeight, rcClient.bottom - rcClient.top);
        }
    }

    // Create device and swap chain
    scd.BufferCount                        = 1;
    scd.BufferDesc.Width                   = static_cast<UINT> (initialBackBufferW);
    scd.BufferDesc.Height                  = static_cast<UINT> (initialBackBufferH);
    // BGRA matches Video/PixelFormat.h byte order (B in byte 0); using
    // R8G8B8A8 instead would force every Windows pixel-export path
    // (CF_DIB clipboard, BMP, WIC) to swizzle R/B on the way out.
    scd.BufferDesc.Format                  = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                       = hwnd;
    scd.SampleDesc.Count                   = 1;
    scd.Windowed                           = TRUE;

#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // BGRA support lets Direct2D bind a target bitmap directly over
    // our DXGI back buffer for the DirectWrite text pass. The flag is
    // free when present and required for the D2D-on-D3D11 path used
    // by the native UI overlay.
    createFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    hr = D3D11CreateDeviceAndSwapChain (nullptr,
                                        D3D_DRIVER_TYPE_HARDWARE,
                                        nullptr,
                                        createFlags,
                                        nullptr,
                                        0,
                                        D3D11_SDK_VERSION,
                                        &scd,
                                        &m_swapChain,
                                        &m_device,
                                        &featureLevel,
                                        &m_context);
    CHRA (hr);

#ifdef _DEBUG
    // Wire the D3D11 InfoQueue so the debug layer DebugBreak()s on the
    // exact call that violates a rule rather than letting the violation
    // propagate into a later AV / DEVICE_REMOVED. Pinpointing the
    // illegal operation is much easier with the stack still at the
    // offending call. We break on WARNING too because the
    // DEVICE_REMOVAL_PROCESS_POSSIBLY_AT_FAULT diagnostic itself is
    // WARNING severity, and the upstream violation that triggered it
    // is often emitted as WARNING as well.
    {
        ComPtr<ID3D11InfoQueue>  d3dInfoQueue;
        HRESULT                  hrD3dInfo = m_device.As (&d3dInfoQueue);


        if (SUCCEEDED (hrD3dInfo) && d3dInfoQueue)
        {
            d3dInfoQueue->SetBreakOnSeverity (D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            d3dInfoQueue->SetBreakOnSeverity (D3D11_MESSAGE_SEVERITY_ERROR,      TRUE);
            d3dInfoQueue->SetBreakOnSeverity (D3D11_MESSAGE_SEVERITY_WARNING,    TRUE);
        }
    }

    // DXGI keeps its own InfoQueue covering swap-chain / Present /
    // ResizeBuffers diagnostics that don't surface through D3D11's queue.
    // Resolve via the optional dxgidebug.dll entry point so we degrade
    // gracefully on machines/SKUs without the DirectX Graphics Tools
    // optional feature installed.
    {
        HMODULE  dxgiDebug = LoadLibraryW (L"dxgidebug.dll");

        if (dxgiDebug != nullptr)
        {
            using PFN_DXGIGetDebugInterface = HRESULT (WINAPI *) (REFIID, void **);
            auto  pfnGet = reinterpret_cast<PFN_DXGIGetDebugInterface> (GetProcAddress (dxgiDebug, "DXGIGetDebugInterface"));

            if (pfnGet != nullptr)
            {
                ComPtr<IDXGIInfoQueue>  dxgiInfoQueue;
                HRESULT                 hrDxgiInfo = pfnGet (IID_PPV_ARGS (&dxgiInfoQueue));

                if (SUCCEEDED (hrDxgiInfo) && dxgiInfoQueue)
                {
                    dxgiInfoQueue->SetBreakOnSeverity (DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                    dxgiInfoQueue->SetBreakOnSeverity (DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR,      TRUE);
                    dxgiInfoQueue->SetBreakOnSeverity (DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING,    TRUE);
                }
            }
        }
    }
#endif

    // Create render target view
    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, &m_rtv);
    CHRA (hr);

    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);

    // Viewport
    vp.Width    = static_cast<float> (initialBackBufferW);
    vp.Height   = static_cast<float> (initialBackBufferH);
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports (1, &vp);

    // Create dynamic texture for framebuffer upload
    td.Width            = static_cast<UINT> (texWidth);
    td.Height           = static_cast<UINT> (texHeight);
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;  // see Video/PixelFormat.h
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DYNAMIC;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateTexture2D (&td, nullptr, &m_texture);
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_texture.Get(), nullptr, &m_srv);
    CHRA (hr);

    // Bilinear sampler for smooth scaling when window is resized
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = m_device->CreateSamplerState (&sd, &m_sampler);
    CHRA (hr);

    // Compile and create shaders
    hr = InitializeShaders();
    CHRA (hr);

    // Vertex buffer (full-screen quad)
    bd.ByteWidth = sizeof (vertices);
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    initData.pSysMem = vertices;

    hr = m_device->CreateBuffer (&bd, &initData, &m_vertexBuffer);
    CHRA (hr);

    // Index buffer
    bd               = {};
    bd.ByteWidth     = sizeof (indices);
    bd.Usage         = D3D11_USAGE_DEFAULT;
    bd.BindFlags     = D3D11_BIND_INDEX_BUFFER;

    initData         = {};
    initData.pSysMem = indices;

    hr = m_device->CreateBuffer (&bd, &initData, &m_indexBuffer);
    CHRA (hr);

    // CRT post-process chain (shaders, ping-pong RTs, sampler). The
    // intermediate RTs are sized lazily on the first Process() call from
    // the back buffer dimensions tracked below.
    hr = m_crtPost.Initialize (m_device.Get(), m_context.Get());
    CHRA (hr);

    m_backBufferW = initialBackBufferW;
    m_backBufferH = initialBackBufferH;

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
//  UploadAndPresent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::UploadAndPresent (const uint32_t * framebuffer)
{
    HRESULT                    hr            = S_OK;
    D3D11_MAPPED_SUBRESOURCE   mapped        = {};
    const uint32_t           * src           = nullptr;
    Byte                     * dst           = nullptr;
    UINT                       stride        = sizeof (Vertex);
    UINT                       offset        = 0;
    float                      clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };



    BAIL_OUT_IF (m_context == nullptr || m_swapChain == nullptr, S_OK);
    // Device permanently removed (or transiently between Resize
    // attempts) -- skip silently. Recovery is a follow-up that
    // recreates the full pipeline; for now we just stop rendering
    // and avoid the AV on a null m_rtv. No log to avoid flooding the
    // output window once the device is dead and the message loop
    // keeps calling UploadAndPresent every iteration.
    BAIL_OUT_IF (m_deviceRemoved || m_rtv == nullptr, S_OK);

    // Upload framebuffer to texture
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

    // Clear render target
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    // Aspect-fit the emulator framebuffer (35:24 for the //e DHGR
    // 560x384) into the chrome-bounded content area. The window's
    // initial client dimensions are sized so the content area exactly
    // matches the framebuffer aspect; pillarbox / letterbox bars
    // only appear when the user resizes the window to a different
    // aspect than the framebuffer's.
    {
        ScopedPerfTimer  timer ("D3DRenderer.CrtPostProcess");
        RECT  contentRect = { 0, 0, m_backBufferW, m_backBufferH };
        RECT  fittedRect  = {};



        if (m_texWidth > 0 && m_texHeight > 0 && m_backBufferW > 0 && m_backBufferH > 0)
        {
            contentRect.top    = std::min (std::max (0, m_topInsetPx), m_backBufferH);
            contentRect.bottom = std::max<LONG> (contentRect.top,
                                                 m_backBufferH - std::max (0, m_bottomInsetPx));
            fittedRect         = ComputeAspectFitRectInRect (contentRect, m_texWidth, m_texHeight);
        }

        hr = m_crtPost.Process (m_srv.Get(),
                                m_rtv.Get(),
                                m_crtParams,
                                fittedRect,
                                m_backBufferW,
                                m_backBufferH);
        CHRA (hr);
    }

    (void) m_vertexShader;
    (void) m_pixelShader;
    (void) m_vertexBuffer;
    (void) m_indexBuffer;
    (void) m_inputLayout;
    (void) stride;
    (void) offset;

    // Hook: native chrome composite pass runs here, between the
    // emulator blit and Present. Skipped silently if no shell is
    // installed (e.g. early-init failure path or unit-test harness).
    if (m_afterBlitHook)
    {
        ScopedPerfTimer  timer ("D3DRenderer.UiComposite");
        m_afterBlitHook();
    }

    {
        static UINT64  s_frameIndex = 0;



        s_frameIndex++;
        if (s_frameIndex == s_kSmokeFrameStartup || s_frameIndex == s_kSmokeFrameSettings)
        {
            HRESULT  hrDump = DumpBackBufferBmp (m_device.Get(), m_context.Get(), m_swapChain.Get(), s_frameIndex);
            IGNORE_RETURN_VALUE (hrDump, S_OK);
        }
    }

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBackBufferDxgiSurface
//
//  Re-acquires the swap chain back buffer and surfaces it as an
//  IDXGISurface for D2D-bitmap binding. Resolved on every call so a
//  stale handle never survives a `ResizeBuffers`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::GetBackBufferDxgiSurface (IDXGISurface ** ppOutSurface) const
{
    HRESULT                  hr      = S_OK;
    ComPtr<ID3D11Texture2D>  backBuffer;



    CBRAEx (ppOutSurface, E_INVALIDARG);
    *ppOutSurface = nullptr;
    CBRA (m_swapChain);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    hr = backBuffer->QueryInterface (__uuidof (IDXGISurface),
                                     reinterpret_cast<void **> (ppOutSurface));
    CHRA (hr);

Error:
    return hr;
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
//  Resize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::Resize (int width, int height)
{
    HRESULT                     hr                              = S_OK;
    ComPtr<ID3D11Texture2D>     backBuffer;
    D3D11_VIEWPORT              vp                              = {};
    ID3D11ShaderResourceView *  nullSrvs[s_kMaxBoundPsSrvSlots] = {};



    BAIL_OUT_IF (m_swapChain == nullptr || m_device == nullptr || m_context == nullptr, S_OK);
    BAIL_OUT_IF (width <= 0 || height <= 0, S_OK);
    BAIL_OUT_IF (m_deviceRemoved, S_OK);

    // Release the old render target view before resizing. Also unbind
    // SRVs from the pixel shader -- m_srv (framebuffer upload) and
    // the CRT's intermediates may still be bound from the previous
    // UploadAndPresent and the driver retains them until rebind.
    // Letting them dangle through ResizeBuffers has tripped
    // DXGI_ERROR_DRIVER_INTERNAL_ERROR on rapid drags.
    m_context->OMSetRenderTargets   (0, nullptr, nullptr);
    m_context->PSSetShaderResources (0, s_kMaxBoundPsSrvSlots, nullSrvs);

    if (m_rtv)
    {
        m_rtv.Reset();
    }

    // Resize the swap chain buffers. DXGI_ERROR_DEVICE_REMOVED here
    // is GPU-side (TDR, driver crash, sleep/wake, RDP transitions,
    // hybrid-GPU switch, VM suspend) -- not a Casso bug. Bail before
    // the assert; the next paint will retry. Any other failure still
    // asserts so real bugs surface. The OutputDebugString traces let
    // us confirm in the debugger whether a null-RTV crash downstream
    // was actually preceded by this whitelisted bail (vs. some other
    // path Reset'ing m_rtv).
    hr = m_swapChain->ResizeBuffers (0,
                                     static_cast<UINT> (width),
                                     static_cast<UINT> (height),
                                     DXGI_FORMAT_UNKNOWN,
                                     0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED)
    {
        HRESULT  removedReason = m_device->GetDeviceRemovedReason();
        wchar_t  trace[256]   = {};


        swprintf_s (trace,
                    L"D3DRenderer::Resize: ResizeBuffers returned DXGI_ERROR_DEVICE_REMOVED; GetDeviceRemovedReason=0x%08lX; rendering disabled until restart.\n",
                    (unsigned long) removedReason);
        OutputDebugStringW (trace);
        m_deviceRemoved = true;
    }
    BAIL_OUT_IF (hr == DXGI_ERROR_DEVICE_REMOVED, hr);
    CHRA (hr);

    // Re-create render target view
    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    if (hr == DXGI_ERROR_DEVICE_REMOVED)
    {
        HRESULT  removedReason = m_device->GetDeviceRemovedReason();
        wchar_t  trace[256]   = {};


        swprintf_s (trace,
                    L"D3DRenderer::Resize: GetBuffer returned DXGI_ERROR_DEVICE_REMOVED; GetDeviceRemovedReason=0x%08lX; rendering disabled until restart.\n",
                    (unsigned long) removedReason);
        OutputDebugStringW (trace);
        m_deviceRemoved = true;
    }
    BAIL_OUT_IF (hr == DXGI_ERROR_DEVICE_REMOVED, hr);
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, &m_rtv);
    CHRA (hr);
    
    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);

    // Update viewport to match new window size
    vp.Width    = static_cast<float> (width);
    vp.Height   = static_cast<float> (height);
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports (1, &vp);

    m_backBufferW = width;
    m_backBufferH = height;

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
    m_rtv.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}


