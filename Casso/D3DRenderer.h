#pragma once

#include "Pch.h"

#include "CrtPostProcess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  D3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

class D3DRenderer
{
public:
    D3DRenderer();
    ~D3DRenderer();

    HRESULT Initialize (HWND hwnd, int texWidth, int texHeight);
    HRESULT UploadAndPresent (const uint32_t * framebuffer);
    HRESULT ToggleFullscreen (HWND hwnd);
    HRESULT Resize (int width, int height);

    // Live-wire path for CRT params (brightness, scanlines, bloom,
    // color-bleed). EmulatorShell pushes a fresh `CrtParams` once per UI
    // frame (right before UploadAndPresent) so slider edits from the
    // Settings panel land on the next-rendered frame without ever
    // pausing the emulator (FR-041). Outside the live emulator path
    // (e.g., tests, headless boot) the field stays at its in-struct
    // defaults so the renderer behaves like a passthrough.
    void SetCrtParams (const CrtParams & params) { m_crtParams = params; }
    void SetTopInsetPx (int insetPx)             { m_topInsetPx = std::max (0, insetPx); }

    bool IsFullscreen() const { return m_fullscreen; }

    // Accessors for the current swap chain dimensions so the
    // EmulatorShell can populate `CrtParams::outputW/H` for the shader
    // constant buffer without piping the window size through a side
    // channel.
    int  GetBackBufferWidth  () const { return m_backBufferW; }
    int  GetBackBufferHeight () const { return m_backBufferH; }

    void Shutdown();

    // Accessors for the underlying device. Both
    // return non-owning pointers whose lifetime is bounded by
    // Initialize -> Shutdown on this renderer.
    ID3D11Device        * GetDevice  () const { return m_device.Get  (); }
    ID3D11DeviceContext * GetContext() const { return m_context.Get(); }

    // Hook point invoked by UploadAndPresent between the emulator
    // blit DrawIndexed and swapChain->Present. The native UI painter
    // installs its Render() bound to this hook so chrome content
    // composites on top of the framebuffer every frame. A null
    // hook (the default) skips the call entirely. Set once at
    // shell-init time; safe to clear back to nullptr at shutdown.
    void SetAfterBlitHook (std::function<void()> hook) { m_afterBlitHook = std::move (hook); }

private:
    HRESULT InitializeShaders();

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

    // CRT post-process pass. Owns the intermediate ping-pong RTs +
    // the per-effect HLSL pixel shaders. Initialized in Initialize once
    // the device is up; torn down in Shutdown. `Process` is invoked
    // unconditionally from UploadAndPresent -- a disabled effect maps
    // to a zero magnitude in CrtParams, which the shaders treat as a
    // pass-through (see CrtPostProcess.cpp). The intermediate RTs are
    // resized lazily inside Process() when the back buffer size changes.
    CrtPostProcess                   m_crtPost;
    CrtParams                        m_crtParams;
    int                              m_backBufferW = 0;
    int                              m_backBufferH = 0;
    int                              m_topInsetPx  = 0;

    int     m_texWidth    = 0;
    int     m_texHeight   = 0;
    bool    m_fullscreen  = false;

    RECT    m_windowedRect  = {};
    LONG    m_windowedStyle = 0;

    // Hook point. Invoked from UploadAndPresent after the
    // emulator blit DrawIndexed and before swapChain->Present so
    // the native chrome painter (or any other overlay) can draw
    // onto the back buffer.
    std::function<void()>  m_afterBlitHook;
};




