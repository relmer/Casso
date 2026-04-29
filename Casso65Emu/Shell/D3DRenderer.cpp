#include "Pch.h"

#include "D3DRenderer.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")





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
//  D3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

D3DRenderer::D3DRenderer ()
    : m_device        (nullptr),
      m_context       (nullptr),
      m_swapChain     (nullptr),
      m_rtv           (nullptr),
      m_texture       (nullptr),
      m_srv           (nullptr),
      m_sampler       (nullptr),
      m_vertexShader  (nullptr),
      m_pixelShader   (nullptr),
      m_vertexBuffer  (nullptr),
      m_indexBuffer   (nullptr),
      m_inputLayout   (nullptr),
      m_texWidth      (0),
      m_texHeight     (0),
      m_fullscreen    (false),
      m_windowedRect  {},
      m_windowedStyle (0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~D3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

D3DRenderer::~D3DRenderer ()
{
    Shutdown ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::Initialize (HWND hwnd, int texWidth, int texHeight)
{
    HRESULT hr = S_OK;

    m_texWidth  = texWidth;
    m_texHeight = texHeight;

    // Create device and swap chain
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount       = 1;
    scd.BufferDesc.Width  = static_cast<UINT> (texWidth);
    scd.BufferDesc.Height = static_cast<UINT> (texHeight);
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = hwnd;
    scd.SampleDesc.Count  = 1;
    scd.Windowed          = TRUE;

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;

    CHR (D3D11CreateDeviceAndSwapChain (
        nullptr,
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
        &m_context));

    // Create render target view
    {
        ID3D11Texture2D * backBuffer = nullptr;
        CHR (m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer)));
        CHR (m_device->CreateRenderTargetView (backBuffer, nullptr, &m_rtv));
        backBuffer->Release ();
    }

    m_context->OMSetRenderTargets (1, &m_rtv, nullptr);

    // Viewport
    {
        D3D11_VIEWPORT vp = {};
        vp.Width    = static_cast<float> (texWidth);
        vp.Height   = static_cast<float> (texHeight);
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports (1, &vp);
    }

    // Create dynamic texture for framebuffer upload
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = static_cast<UINT> (texWidth);
        td.Height           = static_cast<UINT> (texHeight);
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DYNAMIC;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

        CHR (m_device->CreateTexture2D (&td, nullptr, &m_texture));
        CHR (m_device->CreateShaderResourceView (m_texture, nullptr, &m_srv));
    }

    // Point sampler
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

        CHR (m_device->CreateSamplerState (&sd, &m_sampler));
    }

    // Compile shaders at runtime (simpler than FXC build step for now)
    // Use d3dcompiler for runtime compilation
    {
        // Simple passthrough vertex shader bytecode (pre-assembled)
        // We'll use D3DCompile from d3dcompiler.lib, but to avoid that
        // dependency, we use a minimal pre-compiled shader inline.
        //
        // For the initial build, we'll skip shader compilation and just
        // set up the pipeline. Shaders will be loaded from .cso files
        // once the FXC build step is configured.
        //
        // TEMPORARY: Use minimal embedded shaders
    }

    // Vertex buffer (full-screen quad)
    {
        Vertex vertices[] =
        {
            { -1.0f,  1.0f, 0.0f, 0.0f },  // Top-left
            {  1.0f,  1.0f, 1.0f, 0.0f },  // Top-right
            { -1.0f, -1.0f, 0.0f, 1.0f },  // Bottom-left
            {  1.0f, -1.0f, 1.0f, 1.0f },  // Bottom-right
        };

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = sizeof (vertices);
        bd.Usage     = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = vertices;

        CHR (m_device->CreateBuffer (&bd, &initData, &m_vertexBuffer));
    }

    // Index buffer
    {
        UINT16 indices[] = { 0, 1, 2, 2, 1, 3 };

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = sizeof (indices);
        bd.Usage     = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = indices;

        CHR (m_device->CreateBuffer (&bd, &initData, &m_indexBuffer));
    }

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
    HRESULT hr = S_OK;

    if (m_context == nullptr || m_swapChain == nullptr)
    {
        return S_OK;
    }

    // Upload framebuffer to texture
    if (m_texture != nullptr && framebuffer != nullptr)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        CHR (m_context->Map (m_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));

        const uint32_t * src = framebuffer;
        Byte * dst = static_cast<Byte *> (mapped.pData);

        for (int y = 0; y < m_texHeight; y++)
        {
            memcpy (dst, src, static_cast<size_t> (m_texWidth) * 4);
            src += m_texWidth;
            dst += mapped.RowPitch;
        }

        m_context->Unmap (m_texture, 0);
    }

    // Draw quad (without shaders for now — just clear + present)
    {
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_context->ClearRenderTargetView (m_rtv, clearColor);
    }

    // If shaders are loaded, draw the textured quad
    if (m_vertexShader != nullptr && m_pixelShader != nullptr)
    {
        m_context->VSSetShader (m_vertexShader, nullptr, 0);
        m_context->PSSetShader (m_pixelShader, nullptr, 0);
        m_context->PSSetShaderResources (0, 1, &m_srv);
        m_context->PSSetSamplers (0, 1, &m_sampler);

        UINT stride = sizeof (Vertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers (0, 1, &m_vertexBuffer, &stride, &offset);
        m_context->IASetIndexBuffer (m_indexBuffer, DXGI_FORMAT_R16_UINT, 0);
        m_context->IASetInputLayout (m_inputLayout);
        m_context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_context->DrawIndexed (6, 0, 0);
    }

    hr = m_swapChain->Present (1, 0);

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
    HRESULT hr = S_OK;

    if (!m_fullscreen)
    {
        // Save windowed state
        m_windowedStyle = GetWindowLong (hwnd, GWL_STYLE);
        GetWindowRect (hwnd, &m_windowedRect);

        // Go borderless fullscreen
        HMONITOR hMon = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof (mi) };
        CWRA (GetMonitorInfo (hMon, &mi));

        SetWindowLong (hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos (hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_FRAMECHANGED);

        m_fullscreen = true;
    }
    else
    {
        // Restore windowed
        SetWindowLong (hwnd, GWL_STYLE, m_windowedStyle);
        SetWindowPos (hwnd, HWND_NOTOPMOST,
            m_windowedRect.left, m_windowedRect.top,
            m_windowedRect.right - m_windowedRect.left,
            m_windowedRect.bottom - m_windowedRect.top,
            SWP_FRAMECHANGED);

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

void D3DRenderer::Shutdown ()
{
    if (m_inputLayout)   { m_inputLayout->Release ();   m_inputLayout   = nullptr; }
    if (m_indexBuffer)   { m_indexBuffer->Release ();    m_indexBuffer   = nullptr; }
    if (m_vertexBuffer)  { m_vertexBuffer->Release ();  m_vertexBuffer  = nullptr; }
    if (m_pixelShader)   { m_pixelShader->Release ();   m_pixelShader   = nullptr; }
    if (m_vertexShader)  { m_vertexShader->Release ();  m_vertexShader  = nullptr; }
    if (m_sampler)       { m_sampler->Release ();       m_sampler       = nullptr; }
    if (m_srv)           { m_srv->Release ();           m_srv           = nullptr; }
    if (m_texture)       { m_texture->Release ();       m_texture       = nullptr; }
    if (m_rtv)           { m_rtv->Release ();           m_rtv           = nullptr; }
    if (m_swapChain)     { m_swapChain->Release ();     m_swapChain     = nullptr; }
    if (m_context)       { m_context->Release ();       m_context       = nullptr; }
    if (m_device)        { m_device->Release ();        m_device        = nullptr; }
}
