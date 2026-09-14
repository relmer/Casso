#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "DxuiScrollbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView
//
//  An opaque BGRA picture, centered in its bounds.
//
//  INTEGER SCALING WHEN IT FITS, because a pixel-art picture scaled by 1.7
//  smears every other column. When even 1x does not fit, the picture shrinks
//  to fit, keeping its aspect when asked to. The buffer is copied in, so the
//  caller's storage need not outlive the view.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiFramebufferView : public IDxuiControl
{
public:
    DxuiFramebufferView() = default;
    ~DxuiFramebufferView() override = default;

    void  SetFramebuffer (const uint32_t * bgra, int width, int height);
    void  Clear          ();
    void  SetScaling     (bool integer, bool keepAspect) { m_integer = integer; m_keepAspect = keepAspect; }

    //  A multiple of the fitted size. Past 1 the picture is larger than its
    //  bounds and is cut off at their edges, still centered.
    void   SetZoom (float zoom) { m_zoom = (zoom > 0.0f) ? zoom : 1.0f; ClampPan(); }
    float  GetZoom () const     { return m_zoom; }

    //  A picture larger than its bounds pans: by dragging it, by its
    //  scrollbars, and by the wheel or a touchpad in either direction. The
    //  pan is measured from the centered position, so zooming keeps the
    //  middle of the view where it was.
    bool  CanPan        () const;
    bool  IsInteracting () const { return m_panning || m_vertScroll.IsDragging() || m_horzScroll.IsDragging(); }

    int   GetFramebufferWidth  () const { return m_width;  }
    int   GetFramebufferHeight () const { return m_height; }
    bool  HasFramebuffer       () const { return !m_pixels.empty(); }

    //  Where the picture lands inside the bounds after the last layout.
    RECT  GetDestinationRect () const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Picture"; }

private:
    void  GetScaledSize  (int & outWidth, int & outHeight) const;
    void  ClampPan       ();
    void  SyncScrollbars ();

    static constexpr int  s_kScrollbarWidthDip = 10;
    static constexpr int  s_kWheelStepDip      = 48;

    int                    m_panX       = 0;
    int                    m_panY       = 0;
    bool                   m_panning    = false;
    POINT                  m_panFrom    = {};
    int                    m_panFromX   = 0;
    int                    m_panFromY   = 0;
    DxuiDpiScaler          m_scaler;
    DxuiScrollbar          m_vertScroll;
    DxuiScrollbar          m_horzScroll;
    std::vector<uint32_t>  m_pixels;
    int                    m_width      = 0;
    int                    m_height     = 0;
    bool                   m_integer    = true;
    bool                   m_keepAspect = true;
    float                  m_zoom       = 1.0f;
};
