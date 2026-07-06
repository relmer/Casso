#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  SettingsCompositor
//
//  The Settings live-preview post-process, installed as a DxuiRenderTarget
//  compose hook. DxuiRenderTarget renders the settings panel to an offscreen
//  content texture and hands this class that texture's SRV plus the window's
//  back-buffer RTV; the compositor produces the final frame:
//
//    * Transparency active (the user is dragging a Display / CRT control):
//      separable Gaussian blur (H then V) of the panel, then a compose pass
//      that reveals the emulator region as fully transparent (see-through to
//      the Casso window behind the popup) and keeps the focused control sharp
//      (feathered), with the rest blurred + dimmed.
//
//    * Transparency inactive: a single compose pass with the focus rect set to
//      the whole frame, i.e. the panel drawn sharp + opaque (equivalent to the
//      old direct render) so an open, non-previewing Settings window is solid.
//
//  This is the post-process only: the panel + caption + (when open) the color-
//  picker modal overlay are already in `contentSrv`, painted by the base's
//  PaintContent. Transparency (a live drag) and the color-picker modal are
//  mutually exclusive -- you cannot drag a slider while the modal owns input --
//  so the overlay is only ever present on the inactive (sharp) path and never
//  needs a separate crisp-on-blur pass.
//
//  Non-owning device / context (owned by the window's DxuiHwndSource); UI
//  thread only.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsCompositor
{
public:
    SettingsCompositor  () = default;
    ~SettingsCompositor ();

    HRESULT  Initialize (ID3D11Device * device, ID3D11DeviceContext * context);
    void     Shutdown   ();
    bool     IsInitialized () const { return m_initialized; }

    //  Per-frame preview state, driven from SettingsPreviewController. `active`
    //  turns on the blur + emulator-reveal; the rects are in client pixels.
    void     SetTransparencyState (bool active, RECT emuRectClient, RECT focusRectClient);

    //  DxuiRenderTarget compose hook: content already rendered offscreen ->
    //  blur + compose -> back buffer. Signature matches DxuiRenderTarget::ComposeHook.
    void     Compose (ID3D11ShaderResourceView * contentSrv,
                      ID3D11RenderTargetView   * backBufferRtv,
                      int                        widthPx,
                      int                        heightPx);

private:
    struct SettingsBlurParams
    {
        float  radiusPx = 0.0f;
        float  outputW  = 0.0f;
        float  outputH  = 0.0f;
        float  _pad     = 0.0f;
    };


    struct SettingsComposeParams
    {
        float  emuRectClient[4]   = {};
        float  focusRectClient[4] = {};
        float  outputW            = 0.0f;
        float  outputH            = 0.0f;
        float  dimFactor          = 0.0f;
        float  featherPx          = 0.0f;
    };


    HRESULT  CompilePixelShader   (int resourceId, const char * sourceName, ID3D11PixelShader ** out);
    HRESULT  CreateResources      ();
    HRESULT  EnsureBlurTextures   (int widthPx, int heightPx);
    void     ReleaseBlurTextures  ();
    HRESULT  UploadBlurParams     (const SettingsBlurParams & params);
    HRESULT  UploadComposeParams  (const SettingsComposeParams & params);
    void     DrawFullscreen       (ID3D11RenderTargetView   * rt,
                                   ID3D11ShaderResourceView * srv0,
                                   ID3D11ShaderResourceView * srv1,
                                   ID3D11PixelShader        * ps,
                                   ID3D11Buffer             * constantBuffer,
                                   int                        widthPx,
                                   int                        heightPx);

    ID3D11Device                    * m_device      = nullptr;   // non-owning
    ID3D11DeviceContext             * m_context     = nullptr;   // non-owning
    bool                              m_initialized = false;

    ComPtr<ID3D11VertexShader>        m_vs;
    ComPtr<ID3D11InputLayout>         m_inputLayout;
    ComPtr<ID3D11Buffer>              m_vertexBuffer;
    ComPtr<ID3D11Buffer>              m_indexBuffer;
    ComPtr<ID3D11Buffer>              m_blurConstantBuffer;
    ComPtr<ID3D11Buffer>              m_composeConstantBuffer;
    ComPtr<ID3D11SamplerState>        m_sampler;
    ComPtr<ID3D11BlendState>          m_blendOpaque;
    ComPtr<ID3D11PixelShader>         m_psGaussianH;
    ComPtr<ID3D11PixelShader>         m_psGaussianV;
    ComPtr<ID3D11PixelShader>         m_psCompose;

    //  Blur intermediates (H then V), sized to the back buffer. The "sharp"
    //  source is the base's offscreen content texture (passed in as contentSrv),
    //  so unlike the legacy renderer there is no full-panel texture here.
    ComPtr<ID3D11Texture2D>           m_blurHTex;
    ComPtr<ID3D11RenderTargetView>    m_blurHRtv;
    ComPtr<ID3D11ShaderResourceView>  m_blurHSrv;
    ComPtr<ID3D11Texture2D>           m_blurVTex;
    ComPtr<ID3D11RenderTargetView>    m_blurVRtv;
    ComPtr<ID3D11ShaderResourceView>  m_blurVSrv;
    int                               m_blurWidthPx  = 0;
    int                               m_blurHeightPx = 0;

    bool                              m_transparencyActive = false;
    RECT                              m_emuRectClient      = {};
    RECT                              m_focusRectClient    = {};
};
