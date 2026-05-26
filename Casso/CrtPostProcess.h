#pragma once

#include "Pch.h"

#include "Config/GlobalUserPrefs.h"
#include "Ui/ThemeLoader.h"  // ThemeCrtDefaults



////////////////////////////////////////////////////////////////////////////////
//
//  CrtParams
//
//  Plain-data view of the CRT post-process uniforms uploaded to the shader
//  constant buffer every frame. The shape mirrors the `CrtCb` cbuffer in
//  `Casso/Shaders/CRT/*.hlsl` so a layout mismatch is a localised diff.
//
//  `Enabled` booleans on `GlobalUserPrefs::Crt` are folded down into the
//  numeric field: a disabled effect produces a zero magnitude, which the
//  shaders treat as a pass-through. This keeps the GPU pipeline static
//  (always the full chain) and avoids per-frame pipeline reconfiguration
//  in `CrtPostProcess::Process`.
//
////////////////////////////////////////////////////////////////////////////////

struct CrtParams
{
    float    brightness         = 1.0f;
    float    scanlineIntensity  = 0.0f;
    float    bloomRadius        = 0.0f;
    float    bloomStrength      = 0.0f;
    float    colorBleedWidth    = 0.0f;
    float    outputW            = 1.0f;
    float    outputH            = 1.0f;
    float    contrast           = 1.0f;
    float    gamma              = 2.2f;
    float    persistence        = 0.0f;
};



////////////////////////////////////////////////////////////////////////////////
//
//  MakeCrtParams
//
////////////////////////////////////////////////////////////////////////////////

CrtParams  MakeCrtParams      (const GlobalUserPrefs::Crt & prefsCrt,
                               size_t                       modeIndex,
                               const ThemeCrtDefaults     * themeDefaults,
                               float                        outputW,
                               float                        outputH);



////////////////////////////////////////////////////////////////////////////////
//
//  ComputeLetterboxRect
//
////////////////////////////////////////////////////////////////////////////////

RECT       ComputeLetterboxRect (int backBufferW, int backBufferH);
RECT       ComputeLetterboxRectInRect (const RECT & contentRect);
RECT       ComputeAspectFitRectInRect (const RECT & contentRect,
                                       int          aspectW,
                                       int          aspectH);



////////////////////////////////////////////////////////////////////////////////
//
//  CrtPostProcess
//
//  Owns the GPU resources for the CRT shader chain:
//
//      input  : ID3D11ShaderResourceView* over the emulator framebuffer
//      output : ID3D11RenderTargetView*   over the swap chain back buffer
//
//  Pipeline (each step is a fullscreen triangle):
//
//      1. brightness pass   srv -> ppMain[0]   (viewport = letterbox rect)
//      2. scanlines pass    ppMain[0] -> ppMain[1]
//      3. bloom horizontal  ppMain[1] -> ppBloom[0]
//      4. bloom vertical    ppBloom[0] -> ppBloom[1]
//      5. bloom composite   (ppMain[1] + bloom*ppBloom[1]) -> ppMain[0]
//      6. color bleed pass  ppMain[0] -> ppMain[1]
//      7. final copy        ppMain[1] -> back buffer
//
//  All ping-pong RTs are sized to the back buffer. `Process` reallocates
//  them when the back buffer size changes.
//
////////////////////////////////////////////////////////////////////////////////

class CrtPostProcess
{
public:
    CrtPostProcess();
    ~CrtPostProcess();

    HRESULT  Initialize (ID3D11Device        * device,
                         ID3D11DeviceContext * context);
    HRESULT  Process    (ID3D11ShaderResourceView * srcSrv,
                         ID3D11RenderTargetView   * dstRtv,
                         const CrtParams          & params,
                         const RECT               & viewportRect,
                         int                        backBufferW,
                         int                        backBufferH);
    void     Shutdown   ();

private:

    HRESULT  EnsureSize         (int width, int height);
    HRESULT  CompilePixelShader (const char * src,
                                 ID3D11PixelShader ** out);
    HRESULT  UploadConstants    (const CrtParams & params);
    void     DrawFullscreen     (ID3D11RenderTargetView   * rt,
                                 ID3D11ShaderResourceView * srv0,
                                 ID3D11ShaderResourceView * srv1,
                                 ID3D11PixelShader        * ps,
                                 int                        viewportW,
                                 int                        viewportH,
                                 const RECT               * subViewport);

    ID3D11Device         * m_device  = nullptr;
    ID3D11DeviceContext  * m_context = nullptr;

    ComPtr<ID3D11VertexShader>  m_vs;
    ComPtr<ID3D11InputLayout>   m_inputLayout;
    ComPtr<ID3D11Buffer>        m_vertexBuffer;
    ComPtr<ID3D11Buffer>        m_indexBuffer;
    ComPtr<ID3D11Buffer>        m_constantBuffer;
    ComPtr<ID3D11SamplerState>  m_sampler;
    ComPtr<ID3D11BlendState>    m_blendOpaque;

    ComPtr<ID3D11PixelShader>  m_psBrightness;
    ComPtr<ID3D11PixelShader>  m_psScanlines;
    ComPtr<ID3D11PixelShader>  m_psBloomH;
    ComPtr<ID3D11PixelShader>  m_psBloomV;
    ComPtr<ID3D11PixelShader>  m_psBloomComp;
    ComPtr<ID3D11PixelShader>  m_psColorBleed;
    ComPtr<ID3D11PixelShader>  m_psPersistence;
    ComPtr<ID3D11PixelShader>  m_psGamma;
    ComPtr<ID3D11PixelShader>  m_psCopy;

    // Ping-pong RTs sized to back buffer; recreated by EnsureSize on resize.
    int                              m_width  = 0;
    int                              m_height = 0;
    ComPtr<ID3D11Texture2D>          m_ppMainTex[2];
    ComPtr<ID3D11RenderTargetView>   m_ppMainRtv[2];
    ComPtr<ID3D11ShaderResourceView> m_ppMainSrv[2];
    ComPtr<ID3D11Texture2D>          m_ppBloomTex[2];
    ComPtr<ID3D11RenderTargetView>   m_ppBloomRtv[2];
    ComPtr<ID3D11ShaderResourceView> m_ppBloomSrv[2];

    // Persistence carry-over RT. Holds the post-bloom-composite result
    // of the previous frame so the persistence pass can mix it with the
    // current frame's pre-gamma output. Separate from the ping-pong
    // pool so it doesn't get clobbered between frames.
    ComPtr<ID3D11Texture2D>          m_persistenceTex;
    ComPtr<ID3D11RenderTargetView>   m_persistenceRtv;
    ComPtr<ID3D11ShaderResourceView> m_persistenceSrv;
    bool                             m_persistencePrimed = false;
};
