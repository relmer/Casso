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
    float    gamma              = 1.0f;
    float    persistence        = 0.0f;
    // D3D11 requires constant buffer sizes to be a multiple of 16
    // bytes; the 10 fields above pack to 40 bytes, so pad to 48.
    // These slots are intentionally unused by every shader.
    float    _pad0              = 0.0f;
    float    _pad1              = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MakeCrtParams
//
//  Resolves the CRT shader parameters for one monitor mode from three layered
//  sources.
//
//  The layering is the whole job: a user override wins, otherwise the active
//  theme's crtDefaults apply, otherwise the built-in defaults. That is what
//  lets a theme ship a distinctive look while a user who has customized
//  anything keeps their settings across a theme change -- the per-mode
//  userOverride flag is what decides which side of that line a value falls on.
//
//  Parameters are resolved PER MODE, since color and the three monochrome
//  phosphors want different scanline and bloom strengths.
//
//  Output dimensions are taken because several parameters are expressed
//  relative to the rendered size, so the same settings look the same at any
//  window size.
//
//  Declared as a free function so the resolution rules can be unit-tested
//  without a device, a window, or a theme manager.
//
////////////////////////////////////////////////////////////////////////////////

CrtParams  MakeCrtParams      (const GlobalUserPrefs::Crt & prefsCrt,
                               size_t                       modeIndex,
                               const ThemeCrtDefaults     * themeDefaults,
                               float                        outputW,
                               float                        outputH);





////////////////////////////////////////////////////////////////////////////////
//
//  Letterbox and aspect-fit geometry
//
//  Where the emulator image lands inside a larger surface, preserving its
//  aspect ratio.
//
//  Three overloads exist because the callers differ in what they know. One
//  fits against raw back-buffer dimensions, one against an arbitrary content
//  rect (the desk scene's monitor recess, a print preview), and one takes an
//  explicit aspect for content that is not the Apple II framebuffer.
//
//  All three are free functions rather than renderer methods, so the fit
//  arithmetic -- the part that is easy to get subtly wrong and that shows up
//  immediately as a stretched or offset image -- is unit-testable with no GPU
//  at all.
//
//  They return a rect rather than mutating renderer state, so a caller can ask
//  where the image WOULD go, which is what hit-testing and the screen-rect
//  cache both need.
//
////////////////////////////////////////////////////////////////////////////////

RECT       ComputeLetterboxRect (int backBufferW, int backBufferH);
RECT       ComputeLetterboxRectInRect (const RECT & contentRect);
RECT       ComputeAspectFitRectInRect (const RECT & contentRect,
                                       int          aspectW,
                                       int          aspectH);


//
//  Where a fitted rect lands in a texture's UV space -- the desk scene samples
//  the CRT chain's offscreen output on the monitor glass through this subrect,
//  so the fit arithmetic stays shared between the direct-to-backbuffer path
//  and the texture path. Free function for the same reason as the fits above:
//  unit-testable with no GPU.
//
struct CrtUvRect
{
    float  u0 = 0.0f;
    float  v0 = 0.0f;
    float  u1 = 1.0f;
    float  v1 = 1.0f;
};

CrtUvRect  ComputeUvRectForFit (const RECT & fittedRect, int textureW, int textureH);





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

    static constexpr UINT  kMaxBoundPsSrvSlots = 2;

    // Nested rather than file-scope: a bare struct in a .cpp has external
    // linkage, so two translation units defining different types under one
    // name is an ODR violation the linker will not report. SettingsCompositor
    // declares its own ShaderSource, which is exactly that collision.
    struct ShaderSource
    {
        const void * pData  = nullptr;
        size_t       cbData = 0;
    };

    struct CrtVertex
    {
        float x;
        float y;
        float u;
        float v;
    };

    static HRESULT  LoadShaderSource (int resourceId, ShaderSource * outSource);

    HRESULT  EnsureSize         (int width, int height);
    HRESULT  CompilePixelShader (int                  resourceId,
                                  const char         * sourceName,
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
