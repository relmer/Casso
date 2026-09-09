#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WarpRenderHarness
//
//  A Direct3D device a test can render through, with no window and no display.
//
//  The renderer was the last part of the emulator anyone could claim had to
//  live in the executable because it "touches the hardware". It does not: WARP
//  is a software rasterizer, it needs no adapter, no swap chain and no HWND,
//  and what comes out of it can be read back into ordinary memory and
//  asserted. That is the whole argument of Principle VI's corrected reading,
//  and this class is what makes it checkable rather than asserted.
//
//  Test Isolation holds: nothing here touches a file, the registry or a
//  display. The device is created per harness instance so tests stay
//  independent of one another.
//
//  IsAvailable() reports false when the platform has no WARP device at all.
//  A test that cannot run says so rather than failing, because a missing
//  software rasterizer is a fact about the machine and not about Casso.
//
////////////////////////////////////////////////////////////////////////////////

class WarpRenderHarness
{
public:

    WarpRenderHarness ()
    {
        HRESULT  hr = D3D11CreateDevice (nullptr,
                                         D3D_DRIVER_TYPE_WARP,
                                         nullptr,
                                         0,
                                         nullptr,
                                         0,
                                         D3D11_SDK_VERSION,
                                         &m_device,
                                         nullptr,
                                         &m_context);

        if (FAILED (hr))
        {
            m_device.Reset();
            m_context.Reset();
        }
    }


    bool                  IsAvailable () const { return (m_device != nullptr); }
    ID3D11Device *        GetDevice   () const { return (m_device.Get()); }
    ID3D11DeviceContext * GetContext  () const { return (m_context.Get()); }


    //
    //  A source texture holding the given BGRA pixels, with a shader resource
    //  view over it -- the shape the post-process chain consumes.
    //
    HRESULT MakeSource (int                          width,
                        int                          height,
                        const std::vector<uint32_t> & pixels,
                        ComPtr<ID3D11ShaderResourceView> & outSrv) const
    {
        HRESULT                   hr      = S_OK;
        D3D11_TEXTURE2D_DESC      desc    = {};
        D3D11_SUBRESOURCE_DATA    initial = {};
        ComPtr<ID3D11Texture2D>   texture;

        desc.Width            = (UINT) width;
        desc.Height           = (UINT) height;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        initial.pSysMem     = pixels.data();
        initial.SysMemPitch = (UINT) (width * sizeof (uint32_t));

        hr = m_device->CreateTexture2D (&desc, &initial, &texture);
        CHR (hr);

        hr = m_device->CreateShaderResourceView (texture.Get(), nullptr, &outSrv);
        CHR (hr);

    Error:
        return (hr);
    }


    //
    //  A render target of the given size, plus its view.
    //
    HRESULT MakeTarget (int                        width,
                        int                        height,
                        ComPtr<ID3D11Texture2D>  & outTexture,
                        ComPtr<ID3D11RenderTargetView> & outRtv) const
    {
        HRESULT               hr   = S_OK;
        D3D11_TEXTURE2D_DESC  desc = {};

        desc.Width            = (UINT) width;
        desc.Height           = (UINT) height;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_DEFAULT;
        desc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = m_device->CreateTexture2D (&desc, nullptr, &outTexture);
        CHR (hr);

        hr = m_device->CreateRenderTargetView (outTexture.Get(), nullptr, &outRtv);
        CHR (hr);

    Error:
        return (hr);
    }


    //
    //  Copies a rendered target into ordinary memory through a staging
    //  texture, which is the only way the CPU may read a DEFAULT resource.
    //
    HRESULT ReadBack (ID3D11Texture2D       * source,
                      std::vector<uint32_t> & outPixels) const
    {
        HRESULT                   hr      = S_OK;
        D3D11_TEXTURE2D_DESC      desc    = {};
        D3D11_MAPPED_SUBRESOURCE  mapped  = {};
        ComPtr<ID3D11Texture2D>   staging;
        UINT                      row     = 0;
        bool                      fMapped = false;

        source->GetDesc (&desc);

        desc.Usage          = D3D11_USAGE_STAGING;
        desc.BindFlags      = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags      = 0;

        hr = m_device->CreateTexture2D (&desc, nullptr, &staging);
        CHR (hr);

        m_context->CopyResource (staging.Get(), source);

        hr = m_context->Map (staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        CHR (hr);
        fMapped = true;

        outPixels.resize ((size_t) desc.Width * desc.Height);

        for (row = 0; row < desc.Height; row++)
        {
            const uint8_t *  line = (const uint8_t *) mapped.pData + (size_t) row * mapped.RowPitch;

            memcpy (&outPixels[(size_t) row * desc.Width], line, (size_t) desc.Width * sizeof (uint32_t));
        }

    Error:
        if (fMapped)
        {
            m_context->Unmap (staging.Get(), 0);
        }

        return (hr);
    }


private:

    ComPtr<ID3D11Device>         m_device;
    ComPtr<ID3D11DeviceContext>  m_context;
};
