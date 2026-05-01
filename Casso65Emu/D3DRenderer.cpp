#include "Pch.h"

#include "D3DRenderer.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
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
    HRESULT                      hr          = S_OK;
    DXGI_SWAP_CHAIN_DESC         scd         = {};
    UINT                         createFlags = 0;
    D3D_FEATURE_LEVEL            featureLevel;
    ID3D11Texture2D            * backBuffer  = nullptr;
    D3D11_VIEWPORT               vp          = {};
    D3D11_TEXTURE2D_DESC         td          = {};
    D3D11_SAMPLER_DESC           sd          = {};
    D3D11_BUFFER_DESC            bd          = {};
    D3D11_SUBRESOURCE_DATA       initData    = {};

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

    // Create device and swap chain
    scd.BufferCount                        = 1;
    scd.BufferDesc.Width                   = static_cast<UINT> (texWidth);
    scd.BufferDesc.Height                  = static_cast<UINT> (texHeight);
    scd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                       = hwnd;
    scd.SampleDesc.Count                   = 1;
    scd.Windowed                           = TRUE;

#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

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

    // Create render target view
    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);
    hr = m_device->CreateRenderTargetView (backBuffer, nullptr, &m_rtv);
    CHRA (hr);
    backBuffer->Release ();

    m_context->OMSetRenderTargets (1, &m_rtv, nullptr);

    // Viewport
    vp.Width    = static_cast<float> (texWidth);
    vp.Height   = static_cast<float> (texHeight);
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports (1, &vp);

    // Create dynamic texture for framebuffer upload
    td.Width            = static_cast<UINT> (texWidth);
    td.Height           = static_cast<UINT> (texHeight);
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DYNAMIC;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateTexture2D (&td, nullptr, &m_texture);
    CHRA (hr);
    hr = m_device->CreateShaderResourceView (m_texture, nullptr, &m_srv);
    CHRA (hr);

    // Bilinear sampler for smooth scaling when window is resized
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = m_device->CreateSamplerState (&sd, &m_sampler);
    CHRA (hr);

    // Compile and create shaders
    hr = InitializeShaders ();
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

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeShaders
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::InitializeShaders ()
{
    HRESULT      hr     = S_OK;
    ID3DBlob   * vsBlob = nullptr;
    ID3DBlob   * psBlob = nullptr;
    ID3DBlob   * errors = nullptr;

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
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
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
    if (errors) { errors->Release (); errors = nullptr; }
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
    if (errors) { errors->Release (); errors = nullptr; }
    CHRA (hr);

    // Create vertex shader
    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer (),
                                       vsBlob->GetBufferSize (),
                                       nullptr,
                                       &m_vertexShader);
    CHRA (hr);

    // Create pixel shader
    hr = m_device->CreatePixelShader (psBlob->GetBufferPointer (),
                                      psBlob->GetBufferSize (),
                                      nullptr,
                                      &m_pixelShader);
    CHRA (hr);

    // Create input layout
    hr = m_device->CreateInputLayout (layout,
                                      2,
                                      vsBlob->GetBufferPointer (),
                                      vsBlob->GetBufferSize (),
                                      &m_inputLayout);
    CHRA (hr);

Error:
    if (vsBlob) { vsBlob->Release (); }
    if (psBlob) { psBlob->Release (); }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UploadAndPresent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::UploadAndPresent (const uint32_t * framebuffer)
{
    HRESULT                    hr     = S_OK;
    D3D11_MAPPED_SUBRESOURCE   mapped = {};
    const uint32_t           * src    = nullptr;
    Byte                     * dst    = nullptr;
    UINT                       stride = sizeof (Vertex);
    UINT                       offset = 0;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };



    if (m_context == nullptr || m_swapChain == nullptr)
    {
        return S_OK;
    }

    // Upload framebuffer to texture
    if (m_texture != nullptr && framebuffer != nullptr)
    {
        hr = m_context->Map (m_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        CHRA (hr);

        src = framebuffer;
        dst = static_cast<Byte *> (mapped.pData);

        for (int y = 0; y < m_texHeight; y++)
        {
            memcpy (dst, src, static_cast<size_t> (m_texWidth) * 4);
            src += m_texWidth;
            dst += mapped.RowPitch;
        }

        m_context->Unmap (m_texture, 0);
    }

    // Clear render target
    m_context->ClearRenderTargetView (m_rtv, clearColor);

    // Draw the textured quad
    if (m_vertexShader != nullptr && m_pixelShader != nullptr)
    {
        m_context->VSSetShader (m_vertexShader, nullptr, 0);
        m_context->PSSetShader (m_pixelShader, nullptr, 0);
        m_context->PSSetShaderResources (0, 1, &m_srv);
        m_context->PSSetSamplers (0, 1, &m_sampler);

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
    HRESULT     hr        = S_OK;
    HMONITOR    hMon;
    MONITORINFO mi        = { sizeof (mi) };
    BOOL        monResult = FALSE;



    if (!m_fullscreen)
    {
        // Save windowed state
        m_windowedStyle = GetWindowLong (hwnd, GWL_STYLE);
        GetWindowRect (hwnd, &m_windowedRect);

        // Go borderless fullscreen
        hMon      = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
        monResult = GetMonitorInfo (hMon, &mi);
        CWRA (monResult);

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
//  Resize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT D3DRenderer::Resize (int width, int height)
{
    HRESULT             hr         = S_OK;
    ID3D11Texture2D   * backBuffer = nullptr;
    D3D11_VIEWPORT      vp         = {};



    if (m_swapChain == nullptr || m_device == nullptr || m_context == nullptr)
    {
        return S_OK;
    }

    if (width <= 0 || height <= 0)
    {
        return S_OK;
    }

    // Release the old render target view before resizing
    m_context->OMSetRenderTargets (0, nullptr, nullptr);

    if (m_rtv != nullptr)
    {
        m_rtv->Release ();
        m_rtv = nullptr;
    }

    // Resize the swap chain buffers
    hr = m_swapChain->ResizeBuffers (0,
                                     static_cast<UINT> (width),
                                     static_cast<UINT> (height),
                                     DXGI_FORMAT_UNKNOWN,
                                     0);
    CHRA (hr);

    // Re-create render target view
    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (backBuffer, nullptr, &m_rtv);
    CHRA (hr);
    
    backBuffer->Release ();

    m_context->OMSetRenderTargets (1, &m_rtv, nullptr);

    // Update viewport to match new window size
    vp.Width    = static_cast<float> (width);
    vp.Height   = static_cast<float> (height);
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports (1, &vp);

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
    if (m_indexBuffer)   { m_indexBuffer->Release ();   m_indexBuffer   = nullptr; }
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





