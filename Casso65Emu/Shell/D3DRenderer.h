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
    ID3D11Device *          m_device;
    ID3D11DeviceContext *   m_context;
    IDXGISwapChain *        m_swapChain;
    ID3D11RenderTargetView * m_rtv;
    ID3D11Texture2D *       m_texture;
    ID3D11ShaderResourceView * m_srv;
    ID3D11SamplerState *    m_sampler;
    ID3D11VertexShader *    m_vertexShader;
    ID3D11PixelShader *     m_pixelShader;
    ID3D11Buffer *          m_vertexBuffer;
    ID3D11Buffer *          m_indexBuffer;
    ID3D11InputLayout *     m_inputLayout;

    int     m_texWidth;
    int     m_texHeight;
    bool    m_fullscreen;

    RECT    m_windowedRect;
    LONG    m_windowedStyle;
};
