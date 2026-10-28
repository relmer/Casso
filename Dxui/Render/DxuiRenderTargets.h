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

    HRESULT  BeginDrawOffscreen ();
    HRESULT  EndDrawComposite   ();

    void  ReleaseOffscreen () { m_offscreen.Reset(); m_offscreenW = 0; m_offscreenH = 0; }

private:
    DxuiTextRenderer  & m_renderer;

    ComPtr<ID2D1Bitmap1>  m_offscreen;
    UINT                  m_offscreenW = 0;
    UINT                  m_offscreenH = 0;

    // The page pass's recording, live only between BeginDrawDeferred and
    // EndDrawDeferred. A command list cannot be reopened once closed, so this
    // is a fresh one per frame rather than a cached resource.
    ComPtr<ID2D1CommandList>  m_cmdList;

    // The text-as-texture target, and the D3D texture behind it that the
    // scene samples. Kept across calls and only recreated when a larger
    // one is asked for, since a label is re-rendered only when it changes.
    ComPtr<ID3D11Texture2D>           m_textureTarget;
    ComPtr<ID3D11ShaderResourceView>  m_textureSrv;
    ComPtr<ID2D1Bitmap1>              m_textureBitmap;
    ComPtr<ID2D1Image>                m_savedTarget;
    UINT                              m_textureW = 0;
    UINT                              m_textureH = 0;
};