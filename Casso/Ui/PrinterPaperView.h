#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPaperView
//
//  A Dxui content control that displays a rendered printout: a premultiplied
//  BGRA image blitted scale-to-fit and centred on a dark mat, via
//  IDxuiTextRenderer::DrawIconBitmap (the same GPU-cached blit the caption bar
//  uses for the app icon). Shared by the printer panel and print preview; the
//  owner hands it the rendered strip (or a single page) through SetImage.
//
//  The pixel buffer is kept stable between SetImage calls so the renderer's
//  bitmap cache stays hot across frames.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterPaperView : public IDxuiControl
{
public:
    PrinterPaperView  () = default;
    ~PrinterPaperView () override = default;

    // Take ownership of a premultiplied-BGRA image (srcW x srcH pixels).
    void  SetImage  (std::vector<uint32_t> && bgra, int srcW, int srcH);
    void  Clear     ();
    bool  HasImage  () const { return m_srcW > 0 && m_srcH > 0 && !m_bgra.empty (); }

    void  Paint  (IDxuiPainter        & painter,
                  IDxuiTextRenderer   & text,
                  const IDxuiTheme    & theme) override;

    void  Layout (const RECT          & boundsDip,
                  const DxuiDpiScaler & scaler) override;

private:
    std::vector<uint32_t>  m_bgra;
    int                    m_srcW   = 0;
    int                    m_srcH   = 0;
    RECT                   m_bounds = {};
    UINT                   m_dpi    = 96;
};
