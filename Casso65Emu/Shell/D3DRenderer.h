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
    ID3D11Device *          m_device       = nullptr;
    ID3D11DeviceContext *   m_context      = nullptr;
    IDXGISwapChain *        m_swapChain    = nullptr;
    ID3D11RenderTargetView * m_rtv         = nullptr;
    ID3D11Texture2D *       m_texture      = nullptr;
    ID3D11ShaderResourceView * m_srv       = nullptr;
    ID3D11SamplerState *    m_sampler      = nullptr;
    ID3D11VertexShader *    m_vertexShader = nullptr;
    ID3D11PixelShader *     m_pixelShader  = nullptr;
    ID3D11Buffer *          m_vertexBuffer = nullptr;
    ID3D11Buffer *          m_indexBuffer  = nullptr;
    ID3D11InputLayout *     m_inputLayout  = nullptr;

    int     m_texWidth    = 0;
    int     m_texHeight   = 0;
    bool    m_fullscreen  = false;

    RECT    m_windowedRect  = {};
    LONG    m_windowedStyle = 0;
};
