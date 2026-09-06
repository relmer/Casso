#include "Pch.h"

#include "Core/DxuiFocusRing.h"

#include "Core/DxuiDpiScaler.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"




//  The one ring, in DIP. Horizontal padding is larger than vertical because
//  the eye reads the gap at the ends of a line of text, where the stroke sits
//  beside a glyph, more readily than the gap above and below it.
static constexpr float  s_kPadXDip      = 6.0f;
static constexpr float  s_kPadYDip      = 3.0f;
static constexpr float  s_kRadiusDip    = 6.0f;
static constexpr float  s_kThicknessDip = 1.5f;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFocusRing::Around
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusRing::Around (IDxuiPainter        & painter,
                            float                 contentLeftPx,
                            float                 contentTopPx,
                            float                 contentWidthPx,
                            float                 contentHeightPx,
                            const DxuiDpiScaler & scaler,
                            uint32_t              argbColor)
{
    float  padX = scaler.ToPxf (s_kPadXDip);
    float  padY = scaler.ToPxf (s_kPadYDip);



    //  Nothing to ring. A zero-area content box comes from a control asked to
    //  paint before its first layout, and a ring on it would be a stray mark
    //  in the corner of the panel.
    if (contentWidthPx <= 0.0f || contentHeightPx <= 0.0f)
    {
        return;
    }

    painter.OutlineRoundedRect (contentLeftPx - padX,
                                contentTopPx  - padY,
                                contentWidthPx  + padX * 2.0f,
                                contentHeightPx + padY * 2.0f,
                                scaler.ToPxf (s_kRadiusDip),
                                scaler.ToPxf (s_kThicknessDip),
                                argbColor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFocusRing::AroundRun
//
//  Measures the label and rings the control from its glyph through the end of
//  that label.
//
//  A FAILED measure falls back to the glyph alone rather than to the bounds:
//  a ring that is too small still points at the right control, where one
//  stretched to the layout column points at the column.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusRing::AroundRun (IDxuiPainter        & painter,
                               IDxuiTextRenderer   & text,
                               const std::wstring  & run,
                               float                 fontDip,
                               const wchar_t       * fontFamily,
                               float                 contentLeftPx,
                               float                 runLeftPx,
                               float                 bandTopPx,
                               float                 bandHeightPx,
                               float                 minHeightPx,
                               const DxuiDpiScaler & scaler,
                               uint32_t              argbColor)
{
    HRESULT  hr        = S_OK;
    float    runW      = 0.0f;
    float    runH      = 0.0f;
    float    contentW  = 0.0f;
    float    contentH  = 0.0f;



    if (!run.empty())
    {
        hr = text.MeasureString (run.c_str(), fontDip, fontFamily, runW, runH);

        if (FAILED (hr))
        {
            runW = 0.0f;
            runH = 0.0f;
        }
    }

    //  No text to measure, or none that measured: the ring is the glyph.
    contentW = (runW > 0.0f) ? (runLeftPx - contentLeftPx) + runW : minHeightPx;
    contentH = (runH > minHeightPx) ? runH : minHeightPx;

    Around (painter, contentLeftPx, bandTopPx + (bandHeightPx - contentH) * 0.5f,
            contentW, contentH, scaler, argbColor);
}
