#include "Pch.h"

#include "Ui/Settings/SettingsInfoTip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureLabelPx
//
////////////////////////////////////////////////////////////////////////////////

int SettingsInfoTip::MeasureLabelPx (const DxuiLabel & label, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text)
{
    constexpr int  kGlyphWidthDp = 7;



    HRESULT  hr       = S_OK;
    float    widthPx  = 0.0f;
    float    heightPx = 0.0f;



    if (text != nullptr)
    {
        hr = text->MeasureString (label.GetText().c_str(), scaler.ToPxf (label.GetFontSizeDip()), DxuiTheme::kBodyFace, widthPx, heightPx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    // After a failed measurement the width is zero, as with no renderer.
    if (widthPx <= 0.0f)
    {
        widthPx = (float) (label.GetText().size() * scaler.ToPx (kGlyphWidthDp));
    }

    return (int) std::ceil (widthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRect
//
//  The glyph is centered in the square, so the square starts half its margin
//  before the glyph. It drops by a fraction of the label's font size to put
//  the ring's center on the capitals' center.
//
////////////////////////////////////////////////////////////////////////////////

RECT SettingsInfoTip::GetRect (const DxuiLabel & label, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text)
{
    // With the two boxes centered the ring sits high, because the icon
    // font's ink sits higher in its box than the body face's capitals do in
    // theirs. Measured from rendered captures.
    constexpr float  kInfoTipDropEm = 0.05f;



    RECT  row     = label.GetRect();
    int   size    = row.bottom - row.top;
    int   glyphPx = (int) std::ceil (scaler.ToPxf (DxuiInfoTip::kGlyphDip));
    int   glyphX  = row.left + MeasureLabelPx (label, scaler, text) + scaler.ToPx (kGapDp);
    int   drop    = (int) std::lround (scaler.ToPxf (label.GetFontSizeDip()) * kInfoTipDropEm);
    int   left    = glyphX - (size - glyphPx) / 2;
    RECT  rect    = { left, row.top + drop, left + size, row.top + drop + size };



    return rect;
}
