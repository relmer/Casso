#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





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

    int   GetFramebufferWidth  () const { return m_width;  }
    int   GetFramebufferHeight () const { return m_height; }
    bool  HasFramebuffer       () const { return !m_pixels.empty(); }

    //  Where the picture lands inside the bounds after the last layout.
    RECT  GetDestinationRect () const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Picture"; }

private:
    std::vector<uint32_t>  m_pixels;
    int                    m_width      = 0;
    int                    m_height     = 0;
    bool                   m_integer    = true;
    bool                   m_keepAspect = true;
};
