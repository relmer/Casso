#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  D3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

class D3DRenderer
{
public:
    D3DRenderer ();
    ~D3DRenderer ();

    HRESULT Initialize (HWND hwnd, int texWidth, int texHeight);
    HRESULT UploadAndPresent (const uint32_t * framebuffer);
    HRESULT ToggleFullscreen (HWND hwnd);
    HRESULT Resize (int width, int height);

    bool IsFullscreen () const { return m_fullscreen; }

    void Shutdown ();

private:
    HRESULT InitializeShaders ();

    ComPtr<ID3D11Device>             m_device;
    ComPtr<ID3D11DeviceContext>      m_context;
    ComPtr<IDXGISwapChain>           m_swapChain;
    ComPtr<ID3D11RenderTargetView>   m_rtv;
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11SamplerState>       m_sampler;
    ComPtr<ID3D11VertexShader>       m_vertexShader;
    ComPtr<ID3D11PixelShader>        m_pixelShader;
    ComPtr<ID3D11Buffer>             m_vertexBuffer;
    ComPtr<ID3D11Buffer>             m_indexBuffer;
    ComPtr<ID3D11InputLayout>        m_inputLayout;

    int     m_texWidth    = 0;
    int     m_texHeight   = 0;
    bool    m_fullscreen  = false;

    RECT    m_windowedRect  = {};
    LONG    m_windowedStyle = 0;
};





