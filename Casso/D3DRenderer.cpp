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
    SIZE        * pMax  = reinterpret_cast<SIZE *> (lParam);
    MONITORINFO   mi    = { sizeof (mi) };
    BOOL          ok    = FALSE;
    LONG          monW  = 0;
    LONG          monH  = 0;

    ok = GetMonitorInfoW (hMonitor, &mi);
    if (ok)
    {
        monW = mi.rcMonitor.right  - mi.rcMonitor.left;
        monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
        pMax->cx = std::max<LONG> (pMax->cx, monW);
        pMax->cy = std::max<LONG> (pMax->cy, monH);
    }
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
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::Initialize (HWND hwnd, int texWidth, int texHeight)
{
    HRESULT                    hr                  = S_OK;
    DXGI_SWAP_CHAIN_DESC1      scd                 = {};
    UINT                       createFlags         = 0;
    D3D_FEATURE_LEVEL          featureLevel;
    ComPtr<ID3D11Texture2D>    backBuffer;
    ComPtr<IDXGIDevice>        dxgiDevice;
    ComPtr<IDXGIAdapter>       dxgiAdapter;
    ComPtr<IDXGIFactory2>      dxgiFactory;
    ComPtr<IDXGISwapChain1>    swapChain1;
    D3D11_VIEWPORT             vp                  = {};
    D3D11_TEXTURE2D_DESC       td                  = {};
    D3D11_SAMPLER_DESC         sd                  = {};
    D3D11_BUFFER_DESC          bd                  = {};
    D3D11_SUBRESOURCE_DATA     initData            = {};
    int                        initialBackBufferW  = texWidth;
    int                        initialBackBufferH  = texHeight;
    SIZE                       physicalSize        = {};

    Vertex vertices[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f },  // Top-left
        {  1.0f,  1.0f, 1.0f, 0.0f },  // Top-right
        { -1.0f, -1.0f, 0.0f, 1.0f },  // Bottom-left
        {  1.0f, -1.0f, 1.0f, 1.0f },  // Bottom-right
    };

    UINT16 indices[] = { 0, 1, 2, 2, 1, 3 };



    m_hwnd      = hwnd;
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

    // Flip-model swap chain back buffer is allocated at the max of all
    // attached monitor sizes (clamped up to s_kMinPhysicalBackBuffer)
    // so SetSourceSize -- not ResizeBuffers -- handles every routine
    // resize. The driver-internal-error crash that motivated the
    // flip-model migration was specifically triggered by
    // ResizeBuffers stress during rapid drag-resize.
    physicalSize          = ChooseInitialPhysicalBackBufferSize (initialBackBufferW, initialBackBufferH);
    m_physicalBackBufferW = static_cast<int> (physicalSize.cx);
    m_physicalBackBufferH = static_cast<int> (physicalSize.cy);

#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // BGRA support lets Direct2D bind a target bitmap directly over
    // our DXGI back buffer for the DirectWrite text pass. The flag is
    // free when present and required for the D2D-on-D3D11 path used
    // by the native UI overlay.
    createFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    hr = D3D11CreateDevice (nullptr,
                            D3D_DRIVER_TYPE_HARDWARE,
                            nullptr,
                            createFlags,
                            nullptr,
                            0,
                            D3D11_SDK_VERSION,
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

    // Walk device -> IDXGIDevice -> adapter -> parent factory so we
    // create the swap chain on the same DXGI factory the device is
    // already associated with (rather than minting an independent
    // factory via CreateDXGIFactory). This is the pattern Microsoft
    // documents for D3D11 + flip-model.
    hr = m_device.As (&dxgiDevice);
    CHRA (hr);
    hr = dxgiDevice->GetAdapter (&dxgiAdapter);
    CHRA (hr);
    hr = dxgiAdapter->GetParent (IID_PPV_ARGS (&dxgiFactory));
    CHRA (hr);

    // Flip-discard swap chain. Back buffer stays at physical (max)
    // size for the lifetime of the renderer; SetSourceSize on Resize
    // tells DWM what subrect to present. BGRA matches Video/PixelFormat.h
    // so the framebuffer upload stays a straight memcpy.
    scd.Width            = static_cast<UINT> (m_physicalBackBufferW);
    scd.Height           = static_cast<UINT> (m_physicalBackBufferH);
    scd.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.Stereo           = FALSE;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount      = s_kFlipModelBufferCount;
    scd.Scaling          = DXGI_SCALING_STRETCH;
    scd.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;
    scd.Flags            = 0;

    hr = dxgiFactory->CreateSwapChainForHwnd (m_device.Get(),
                                              hwnd,
                                              &scd,
                                              nullptr,
                                              nullptr,
                                              &swapChain1);
    CHRA (hr);

    // SetSourceSize lives on IDXGISwapChain2 (dxgi1_3). The factory
    // hands back IDXGISwapChain1; QI up. IDXGISwapChain2 is present
    // on every Windows 8.1+ system Casso targets.
    hr = swapChain1.As (&m_swapChain);
    CHRA (hr);

    // Casso owns Alt+Enter via the menu system's fullscreen toggle;
    // disable DXGI's default hijacking so it does not punt us into
    // an exclusive-fullscreen mode behind our back.
    hr = dxgiFactory->MakeWindowAssociation (hwnd, DXGI_MWA_NO_ALT_ENTER);
    CHRA (hr);

    // Create render target view
    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, &m_rtv);
    CHRA (hr);

    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);

    // Tell DWM the initial source sub-rect; without this, the
    // first Present would scale the whole (oversized) back buffer
    // into the client area.
    hr = m_swapChain->SetSourceSize (static_cast<UINT> (initialBackBufferW),
                                     static_cast<UINT> (initialBackBufferH));
    CHRA (hr);

    // Viewport spans the logical (presented) sub-rect only. Anything
    // we render outside this rect would never reach the screen anyway
    // -- DWM clips to the source size at present time.
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
//  Initialize2
//
//  Adopt-mode: skip device + swap-chain creation and reuse the
//  externally-owned device, context, and swap chain (typically
//  DxuiHostWindow's). Still creates this renderer's own back-buffer
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


    CBRA (pDevice);
    CBRA (pContext);
    CBRA (pSwapChain);

    m_device            = pDevice;
    m_context           = pContext;
    m_hwnd              = nullptr;
    m_externalSwapChain = true;
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
    ComPtr<ID3D11Texture2D>  backBuffer;
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


    // Full-ownership (hwnd / Initialize) mode owns the swap chain, so it
    // creates and binds the back-buffer RTV here. In external
    // (Initialize2 / host-owned) mode the host owns the only back-buffer
    // RTV and its resize lifecycle; the renderer composites into the
    // host's RTV (passed to UploadAndComposite) and never holds a
    // back-buffer reference of its own, so the host's ResizeBuffers
    // never contends with a stale renderer reference.
    if (!m_externalSwapChain)
    {
        hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (backBuffer.GetAddressOf()));
        CHRA (hr);
        hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
        CHRA (hr);

        m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);
    }

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

    // Minimized (or mid-resize to zero): the back buffer is 0x0, so there
    // is nothing to draw and the CRT post-process would reject the empty
    // target. Skip the frame entirely; rendering resumes on restore.
    BAIL_OUT_IF (m_backBufferW <= 0 || m_backBufferH <= 0, S_OK);

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

        CacheEmulatorContentScreenRect (fittedRect);

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

    // Reset the redraw-needed flag BEFORE the after-blit hook so the
    // hook can mark redraw for the NEXT frame -- e.g. chrome with an
    // in-progress door animation needs another paint to advance.
    // Without this, the hook's MarkRedrawNeeded would be overwritten
    // by the post-Present reset below and the animation freezes after
    // a single frame.
    m_redrawForced = false;

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
            HRESULT  hrDump = DumpBackBufferBmp (m_device.Get(),
                                                 m_context.Get(),
                                                 m_swapChain.Get(),
                                                 s_frameIndex,
                                                 static_cast<UINT> (m_backBufferW),
                                                 static_cast<UINT> (m_backBufferH));
            IGNORE_RETURN_VALUE (hrDump, S_OK);
        }
    }

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

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
//  UploadAndComposite
//
//  Adopt-mode entry point (Initialize2). Performs the same framebuffer
//  upload + CRT post-process pass as UploadAndPresent but skips the
//  swap-chain Present -- the host owns the Present call -- and the
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
    HRESULT  hr     = S_OK;
    POINT    origin = {};
    BOOL     ok     = FALSE;



    m_emulatorContentScreenRect = {};
    BAIL_OUT_IF (m_hwnd == nullptr || IsRectEmpty (&fittedRect), S_OK);

    ok = ClientToScreen (m_hwnd, &origin);
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
    int                         newPhysicalW                    = 0;
    int                         newPhysicalH                    = 0;
    bool                        needsRealloc                    = false;



    BAIL_OUT_IF (m_swapChain == nullptr || m_device == nullptr || m_context == nullptr, S_OK);
    BAIL_OUT_IF (width <= 0 || height <= 0, S_OK);
    BAIL_OUT_IF (m_deviceRemoved, S_OK);

    m_redrawForced = true;

    // External (Initialize2 / host-owned swap chain) mode: the host owns
    // ResizeBuffers and the only back-buffer RTV; the renderer just
    // refreshes the size it feeds the CRT post-process and must never
    // touch the swap chain. EmulatorShell::OnSize also pushes the size
    // via SetBackBufferSize, so this guard simply makes a stray Resize
    // harmless.
    if (m_externalSwapChain)
    {
        SetBackBufferSize (width, height);
    }
    BAIL_OUT_IF (m_externalSwapChain, S_OK);

    // Hot path: the logical area still fits in the allocated back
    // buffer. Tell DWM the new source sub-rect, update the viewport,
    // and we're done -- no GPU allocation, no driver stress, no
    // resource invalidation. This is the entire point of the
    // flip-model migration.
    needsRealloc = (width > m_physicalBackBufferW) || (height > m_physicalBackBufferH);

    if (needsRealloc)
    {
        // Cold path: window grew past the initial physical allocation
        // (e.g., user dragged it onto a larger monitor than we sized
        // for at startup). Grow the back buffer once, then return to
        // the SetSourceSize hot path. Headroom plus the monitor max
        // keeps subsequent edge-of-monitor drags off this branch.
        newPhysicalW = std::max (width,  m_physicalBackBufferW);
        newPhysicalH = std::max (height, m_physicalBackBufferH);

        // Unbind SRVs from the pixel shader -- m_srv (framebuffer
        // upload) and the CRT's intermediates may still be bound from
        // the previous UploadAndPresent and the driver retains them
        // until rebind. Letting them dangle through ResizeBuffers has
        // tripped DXGI_ERROR_DRIVER_INTERNAL_ERROR.
        m_context->OMSetRenderTargets   (0, nullptr, nullptr);
        m_context->PSSetShaderResources (0, s_kMaxBoundPsSrvSlots, nullSrvs);

        if (m_rtv)
        {
            m_rtv.Reset();
        }

        // DXGI_ERROR_DEVICE_REMOVED on the rare ResizeBuffers path is
        // still treated as a non-bug latch (same rationale as the
        // legacy bitblt code), letting the window stay interactive on
        // the last good frame instead of asserting.
        hr = m_swapChain->ResizeBuffers (s_kFlipModelBufferCount,
                                         static_cast<UINT> (newPhysicalW),
                                         static_cast<UINT> (newPhysicalH),
                                         DXGI_FORMAT_UNKNOWN,
                                         0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            HRESULT  removedReason = m_device->GetDeviceRemovedReason();
            wchar_t  trace[256]    = {};


            swprintf_s (trace,
                        L"D3DRenderer::Resize: ResizeBuffers returned DXGI_ERROR_DEVICE_REMOVED; GetDeviceRemovedReason=0x%08lX; rendering disabled until restart.\n",
                        (unsigned long) removedReason);
            OutputDebugStringW (trace);
            m_deviceRemoved = true;
        }
        BAIL_OUT_IF (hr == DXGI_ERROR_DEVICE_REMOVED, hr);
        CHRA (hr);

        hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
        if (hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            HRESULT  removedReason = m_device->GetDeviceRemovedReason();
            wchar_t  trace[256]    = {};


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

        m_physicalBackBufferW = newPhysicalW;
        m_physicalBackBufferH = newPhysicalH;
    }

    // SetSourceSize is the flip-model way to update what DWM
    // presents -- metadata-only, no GPU allocation, sub-pixel exact.
    hr = m_swapChain->SetSourceSize (static_cast<UINT> (width), static_cast<UINT> (height));
    CHRA (hr);

    // Viewport tracks the new logical sub-rect so the CRT pass and
    // chrome composite never write outside what DWM will present.
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

    m_hwnd                       = nullptr;
    m_emulatorContentScreenRect  = {};
}


