#include "Pch.h"

#include "CassoExplorer/CrtPictureRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPictureRenderer::~CrtPictureRenderer
//
////////////////////////////////////////////////////////////////////////////////

CrtPictureRenderer::~CrtPictureRenderer()
{
    Release();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPictureRenderer::Render
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPictureRenderer::Render (
    ID3D11Device                * device,
    ID3D11DeviceContext         * context,
    const std::vector<uint32_t> & source,
    int                           sourceW,
    int                           sourceH,
    int                           width,
    int                           height,
    const CrtParams             & params,
    std::vector<uint32_t>       & outBgra)
{
    HRESULT                     hr         = S_OK;
    RECT                        whole      = { 0, 0, width, height };
    D3D11_MAPPED_SUBRESOURCE    mapped     = {};
    bool                        isMapped   = false;
    ID3D11RenderTargetView    * noTarget   = nullptr;
    ID3D11ShaderResourceView  * noViews[2] = {};
    bool                        sized      = width > 0 && height > 0 && sourceW > 0 && sourceH > 0
                                             && source.size() >= (size_t) sourceW * (size_t) sourceH;



    CBREx (device != nullptr && context != nullptr, E_INVALIDARG);
    CBREx (sized, E_INVALIDARG);

    hr = Bind (device, context);
    CHR (hr);

    hr = EnsureSource (sourceW, sourceH);
    CHR (hr);

    hr = EnsureTarget (width, height);
    CHR (hr);

    m_context->UpdateSubresource (m_source.Get(), 0, nullptr, source.data(), (UINT) sourceW * sizeof (uint32_t), 0);

    hr = m_crt.Process (m_sourceView.Get(), m_targetView.Get(), params, whole, width, height, sourceW, sourceH);
    CHR (hr);

    m_context->CopyResource (m_staging.Get(), m_target.Get());

    hr = m_context->Map (m_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    CHR (hr);

    isMapped = true;
    outBgra.resize ((size_t) width * (size_t) height);

    for (int row = 0; row < height; row++)
    {
        const uint32_t * line = (const uint32_t *) ((const BYTE *) mapped.pData + (size_t) row * mapped.RowPitch);

        for (int column = 0; column < width; column++)
        {
            outBgra[(size_t) row * (size_t) width + (size_t) column] = line[column] | 0xFF000000u;
        }
    }

Error:
    if (isMapped)
    {
        m_context->Unmap (m_staging.Get(), 0);
    }

    if (m_context != nullptr)
    {
        m_context->OMSetRenderTargets (1, &noTarget, nullptr);
        m_context->PSSetShaderResources (0, ARRAYSIZE (noViews), noViews);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPictureRenderer::Bind
//
//  A new device, as after the window moved to another adapter, starts the
//  post-process and every texture afresh.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPictureRenderer::Bind (ID3D11Device * device, ID3D11DeviceContext * context)
{
    HRESULT  hr = S_OK;



    if (m_device.Get() == device && m_context.Get() == context && m_crtReady)
    {
        return S_OK;
    }

    Release();

    m_device  = device;
    m_context = context;

    hr = m_crt.Initialize (device, context);
    CHR (hr);

    m_crtReady = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPictureRenderer::EnsureSource
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPictureRenderer::EnsureSource (int width, int height)
{
    HRESULT               hr   = S_OK;
    D3D11_TEXTURE2D_DESC  desc = {};



    if (m_source != nullptr && m_sourceW == width && m_sourceH == height)
    {
        return S_OK;
    }

    m_source.Reset();
    m_sourceView.Reset();

    desc.Width            = (UINT) width;
    desc.Height           = (UINT) height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    hr = m_device->CreateTexture2D (&desc, nullptr, &m_source);
    CHR (hr);

    hr = m_device->CreateShaderResourceView (m_source.Get(), nullptr, &m_sourceView);
    CHR (hr);

    m_sourceW = width;
    m_sourceH = height;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPictureRenderer::EnsureTarget
//
//  The picture's target, and the staging copy it is read back through.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CrtPictureRenderer::EnsureTarget (int width, int height)
{
    HRESULT               hr   = S_OK;
    D3D11_TEXTURE2D_DESC  desc = {};



    if (m_target != nullptr && m_targetW == width && m_targetH == height)
    {
        return S_OK;
    }

    m_target.Reset();
    m_targetView.Reset();
    m_staging.Reset();

    desc.Width            = (UINT) width;
    desc.Height           = (UINT) height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_RENDER_TARGET;

    hr = m_device->CreateTexture2D (&desc, nullptr, &m_target);
    CHR (hr);

    hr = m_device->CreateRenderTargetView (m_target.Get(), nullptr, &m_targetView);
    CHR (hr);

    desc.Usage          = D3D11_USAGE_STAGING;
    desc.BindFlags      = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = m_device->CreateTexture2D (&desc, nullptr, &m_staging);
    CHR (hr);

    m_targetW = width;
    m_targetH = height;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPictureRenderer::Release
//
////////////////////////////////////////////////////////////////////////////////

void CrtPictureRenderer::Release()
{
    if (m_crtReady)
    {
        m_crt.Shutdown();
        m_crtReady = false;
    }

    m_source.Reset();
    m_sourceView.Reset();
    m_target.Reset();
    m_targetView.Reset();
    m_staging.Reset();
    m_device.Reset();
    m_context.Reset();

    m_sourceW = 0;
    m_sourceH = 0;
    m_targetW = 0;
    m_targetH = 0;
}
