#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DwriteTextRenderer
//
//  Direct2D-on-Direct3D11 text renderer. Owns a Direct2D device +
//  context bound to a back-buffer surface acquired from the swap chain
//  via `IDXGISurface`, plus a DirectWrite factory and a tiny cache of
//  text formats keyed by `(family, weight, size, dpi)`. Geometry
//  emitted between `BeginDraw` and `EndDraw` composites on top of
//  whatever the D3D pipeline drew earlier in the same frame.
//
//  Lifetime: `Initialize` allocates the Direct2D factory + device
//  against the shared `ID3D11Device` (which MUST have been created
//  with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`). `BindBackBuffer` rebinds
//  the target bitmap whenever the swap chain resizes. `OnDeviceLost`
//  drops every D2D resource so a subsequent `OnDeviceRestored` can
//  rebuild against the new device.
//
////////////////////////////////////////////////////////////////////////////////

class DwriteTextRenderer
{
public:
    DwriteTextRenderer  () = default;
    ~DwriteTextRenderer ();

    HRESULT  Initialize       (ID3D11Device * pDevice);
    void     Shutdown         ();

    HRESULT  BindBackBuffer   (IDXGISurface * pBackBufferSurface,
                               UINT           dpiX,
                               UINT           dpiY);
    void     UnbindBackBuffer ();

    HRESULT  BeginDraw        ();
    HRESULT  EndDraw          ();

    HRESULT  DrawString       (const wchar_t * text,
                               float           xDip,
                               float           yDip,
                               float           widthDip,
                               float           heightDip,
                               uint32_t        argbColor,
                               float           fontSizeDip,
                               const wchar_t * fontFamily);

    HRESULT  OnDeviceLost     ();
    HRESULT  OnDeviceRestored (ID3D11Device * pDevice);

    bool     IsTargetBound    () const { return m_targetBound; }

private:
    struct TextFormatKey
    {
        std::wstring  family;
        float         sizeDip = 0.0f;

        bool operator < (const TextFormatKey & other) const
        {
            if (family != other.family) { return family < other.family; }
            return sizeDip < other.sizeDip;
        }
    };


    HRESULT  EnsureTextFormat (const wchar_t                * family,
                               float                          fontSizeDip,
                               IDWriteTextFormat           ** outFormat);


    ComPtr<ID2D1Factory1>             m_d2dFactory;
    ComPtr<ID2D1Device>               m_d2dDevice;
    ComPtr<ID2D1DeviceContext>        m_d2dContext;
    ComPtr<ID2D1Bitmap1>              m_target;

    ComPtr<IDWriteFactory>            m_dwriteFactory;

    std::map<TextFormatKey,
             ComPtr<IDWriteTextFormat>>  m_formatCache;

    bool                              m_targetBound = false;
    bool                              m_drawing     = false;
};
