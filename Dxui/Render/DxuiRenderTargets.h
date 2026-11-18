#pragma once

#include "Pch.h"

class DxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTargets
//
//  The renderer's targets other than the back buffer: text drawn into a
//  texture the scene samples, a pass written to a command list and
//  replayed, and a pass drawn offscreen and composited.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiRenderTargets
{
public:
    explicit DxuiRenderTargets (DxuiTextRenderer & renderer);

    HRESULT  BeginDrawToTexture   (UINT widthPx, UINT heightPx);
    HRESULT  EndDrawToTexture     (ID3D11ShaderResourceView ** outSrv);
    void     GetDrawToTextureSize (UINT & outWidthPx, UINT & outHeightPx) const;

    HRESULT  BeginDrawDeferred ();
    HRESULT  EndDrawDeferred   ();
    HRESULT  FlushDeferred     ();

    HRESULT  BeginDrawOffscreen ();
    HRESULT  EndDrawComposite   ();

    void  ReleaseOffscreen () { m_offscreen.Reset(); m_offscreenW = 0; m_offscreenH = 0; }

    HRESULT  BeginShadowCapture ();
    HRESULT  EndShadowCapture   (float spreadDip, float blurDip, float opacity);

private:
    DxuiTextRenderer  & m_renderer;

    ComPtr<ID2D1Bitmap1>  m_offscreen;
    UINT                  m_offscreenW = 0;
    UINT                  m_offscreenH = 0;

    // The page pass's recording, live only between BeginDrawDeferred and
    // EndDrawDeferred. A command list cannot be reopened once closed, so this
    // is a fresh one per frame rather than a cached resource.
    ComPtr<ID2D1CommandList>  m_cmdList;

    // Set when the target is lost during a FlushDeferred. The rest of the pass
    // goes into a command list that EndDrawDeferred discards, so the draws
    // after the flush succeed and the rest of the frame's text is discarded.
    bool                      m_isDiscardingPass = false;

    HRESULT  StartRecording  ();
    HRESULT  ReplayRecording (HRESULT hrEnd, bool & outIsTargetLost);

    // The text-as-texture target, and the D3D texture behind it that the
    // scene samples. Kept across calls and only recreated when a larger
    // one is asked for, since a label is re-rendered only when it changes.
    ComPtr<ID3D11Texture2D>           m_textureTarget;
    ComPtr<ID3D11ShaderResourceView>  m_textureSrv;
    ComPtr<ID2D1Bitmap1>              m_textureBitmap;
    ComPtr<ID2D1Image>                m_savedTarget;
    UINT                              m_textureW = 0;
    UINT                              m_textureH = 0;

    // A shadow's command list, drawn into through a second context on the
    // renderer's device while the renderer's context is set aside, so the
    // main context's target and clips are not touched. The second context
    // is created once; the list is a fresh one per shadow.
    ComPtr<ID2D1DeviceContext>        m_shadowContext;
    ComPtr<ID2D1DeviceContext>        m_shadowSavedContext;
    ComPtr<ID2D1CommandList>          m_shadowList;
};