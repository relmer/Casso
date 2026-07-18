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

    hr = RenderCrtFrame (dstRtv, contentRect);
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
//  RenderCrtFrame
//
//  Aspect-fits the emulator content into `contentRect`, caches the
//  resulting on-screen rect (for hit-testing / preview overlap), and
//  runs the CRT post-process pass into `dstRtv`. Scoped so the perf
//  timer measures the post-process pass alone.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::RenderCrtFrame (ID3D11RenderTargetView * dstRtv, const RECT & contentRect)
{
    HRESULT          hr         = S_OK;
    RECT             fittedRect = {};
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
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::ToggleFullscreen (HWND hwnd)
{
    HRESULT     hr        = S_OK;
    HMONITOR    hMon      = nullptr;
    MONITORINFO mi        = { sizeof (mi) };
    LONG        style     = 0;
    BOOL        fSuccess  = FALSE;



    // A modal loop opening mid-transition (assert dialog, message box) pumps
    // messages and can dispatch a queued Alt+Enter into a NESTED toggle. A
    // nested enter would capture the borderless fullscreen state as the
    // "windowed" state to restore, stranding the user in an unescapable
    // full-monitor popup that covers the taskbar and every other window.
    // Ignore toggles while one is in flight.
    BAIL_OUT_IF (m_fsTransition, S_OK);
    m_fsTransition = true;

    if (!m_fullscreen)
    {
        // Belt and braces vs the same corruption arriving some other way:
        // never capture a caption-less (borderless popup) state as the
        // windowed state to restore.
        style = GetWindowLong (hwnd, GWL_STYLE);
        CBRAEx ((style & WS_CAPTION) == WS_CAPTION, E_UNEXPECTED);

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

        SetWindowLong (hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
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
        // mid-transition rect; it drops only after the window is back.
        SetWindowLong (hwnd, GWL_STYLE, m_windowedStyle);

        fSuccess = SetWindowPos (hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
        CWRA (fSuccess);

        m_windowedPlacement.length = sizeof (m_windowedPlacement);
        fSuccess = SetWindowPlacement (hwnd, &m_windowedPlacement);
        CWRA (fSuccess);

        m_fullscreen = false;
    }

Error:
    m_fsTransition = false;
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


