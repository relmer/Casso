#pragma once

#include "Pch.h"

#include "CrtPostProcess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPictureRenderer
//
//  A picture drawn through Casso's CRT post-process at its drawn size,
//  on the preview window's own device, and read back as pixels for the
//  preview to draw one to one. A preview changes only when its picture, its
//  size or its monitor does, so a read back costs nothing that matters, and
//  the preview's view stays a plain picture view.
//
//  The post-process ends with its targets bound; they are unbound after each
//  picture, so the window's own drawing finds the context as it left it.
//
////////////////////////////////////////////////////////////////////////////////

class CrtPictureRenderer
{
public:
    CrtPictureRenderer () = default;
    ~CrtPictureRenderer ();

    CrtPictureRenderer (const CrtPictureRenderer &)             = delete;
    CrtPictureRenderer & operator= (const CrtPictureRenderer &) = delete;

    //  `source` is `sourceW` by `sourceH` BGRA, drawn `width` by `height`.
    //  The result is larger by `outMargin` on every side: black, as Casso's
    //  bars around its picture are, with the picture's glow spilling into it,
    //  so the picture's own edges glow as they do on Casso's screen.
    HRESULT  Render (ID3D11Device                * device,
                     ID3D11DeviceContext         * context,
                     const std::vector<uint32_t> & source,
                     int                           sourceW,
                     int                           sourceH,
                     int                           width,
                     int                           height,
                     const CrtParams             & params,
                     std::vector<uint32_t>       & outBgra,
                     int                         & outMargin);

    //  How far past a picture, in drawn pixels, the bloom and the color bleed
    //  sample, for a picture `scaleX` and `scaleY` drawn pixels to an
    //  emulated one, with one more for the filtering of each sample.
    static int  GetGlowMargin (const CrtParams & params, float scaleX, float scaleY);

private:
    //  The bloom blurs' steps each way, and the color bleed's most emulated
    //  pixels, as the CRT shaders take them.
    static constexpr float  s_kBloomTaps = 4.0f;
    static constexpr float  s_kBleedTaps = 8.0f;

    HRESULT  Bind         (ID3D11Device * device, ID3D11DeviceContext * context);
    HRESULT  EnsureSource (int width, int height);
    HRESULT  EnsureTarget (int width, int height);
    void     Release      ();

    ComPtr<ID3D11Device>              m_device;
    ComPtr<ID3D11DeviceContext>       m_context;
    CrtPostProcess                    m_crt;
    bool                              m_crtReady = false;
    ComPtr<ID3D11Texture2D>           m_source;
    ComPtr<ID3D11ShaderResourceView>  m_sourceView;
    int                               m_sourceW  = 0;
    int                               m_sourceH  = 0;
    ComPtr<ID3D11Texture2D>           m_target;
    ComPtr<ID3D11RenderTargetView>    m_targetView;
    ComPtr<ID3D11Texture2D>           m_staging;
    int                               m_targetW  = 0;
    int                               m_targetH  = 0;
};
