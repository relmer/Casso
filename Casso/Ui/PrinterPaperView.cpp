#include "Pch.h"

#include "PrinterPaperView.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"




static constexpr uint32_t  s_kMat    = 0xFF26282C;   // dark mat behind the paper
static constexpr uint32_t  s_kShadow = 0x55000000;   // soft paper drop shadow
static constexpr uint32_t  s_kBorder = 0xFF14161A;   // thin paper edge




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPaperView::SetImage
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPaperView::SetImage (std::vector<uint32_t> && bgra, int srcW, int srcH)
{
    if (srcW <= 0 || srcH <= 0)
    {
        Clear();
        return;
    }

    m_bgra = std::move (bgra);
    m_srcW = srcW;
    m_srcH = srcH;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPaperView::Clear
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPaperView::Clear()
{
    m_bgra.clear();
    m_srcW = 0;
    m_srcH = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPaperView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPaperView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_bounds = boundsDip;
    m_dpi    = scaler.Dpi();

    SetBounds (boundsDip);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPaperView::Paint
//
//  Dark mat fill, then the printout blitted scale-to-fit and centred (with a
//  soft shadow + edge). Empty state is just the mat.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPaperView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (theme);

    HRESULT        hr     = S_OK;
    DxuiDpiScaler  scaler;
    float          x      = (float) m_bounds.left;
    float          y      = (float) m_bounds.top;
    float          w      = (float) (m_bounds.right  - m_bounds.left);
    float          h      = (float) (m_bounds.bottom - m_bounds.top);

    if (w <= 0.0f || h <= 0.0f)
    {
        return;
    }

    scaler.SetDpi (m_dpi);

    painter.FillRect (x, y, w, h, s_kMat);

    if (!HasImage())
    {
        return;
    }

    {
        float  margin = scaler.Pxf (12.0f);
        float  availW = w - margin * 2.0f;
        float  availH = h - margin * 2.0f;
        float  scale  = 0.0f;
        float  dW     = 0.0f;
        float  dH     = 0.0f;
        float  dX     = 0.0f;
        float  dY     = 0.0f;

        if (availW <= 0.0f || availH <= 0.0f)
        {
            return;
        }

        scale = (std::min) (availW / (float) m_srcW, availH / (float) m_srcH);

        if (scale <= 0.0f)
        {
            scale = 1.0f;
        }

        dW = (float) m_srcW * scale;
        dH = (float) m_srcH * scale;
        dX = x + (w - dW) * 0.5f;
        dY = y + (h - dH) * 0.5f;

        painter.FillRect    (dX + scaler.Pxf (3.0f), dY + scaler.Pxf (3.0f), dW, dH, s_kShadow);
        painter.OutlineRect (dX, dY, dW, dH, 1.0f, s_kBorder);

        IGNORE_RETURN_VALUE (hr, text.DrawIconBitmap (m_bgra.data(), m_srcW, m_srcH, dX, dY, dW, dH));
    }
}
