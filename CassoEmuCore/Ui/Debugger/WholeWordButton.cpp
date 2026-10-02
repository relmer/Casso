#include "Pch.h"

#include "Ui/Debugger/WholeWordButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WholeWordButton::GetTrayParts
//
//  The bottom line runs under the label, kTrayDropDip below the button's
//  middle, and each end rises kTrayEndDip from it.
//
////////////////////////////////////////////////////////////////////////////////

std::array<WholeWordButton::TrayPart, 3> WholeWordButton::GetTrayParts (const RECT & bounds, const DxuiDpiScaler & scaler)
{
    float  centerX = (float) (bounds.left + bounds.right) * 0.5f;
    float  centerY = (float) (bounds.top + bounds.bottom) * 0.5f;
    float  half    = scaler.ToPxf (kTrayHalfWidthDip);
    float  stroke  = (std::max) (1.0f, std::round (scaler.ToPxf (kStrokeDip)));
    float  bottom  = std::round (centerY + scaler.ToPxf (kTrayDropDip));
    float  rise    = scaler.ToPxf (kTrayEndDip);
    float  left    = std::round (centerX - half);
    float  right   = std::round (centerX + half);



    return { TrayPart { left,           bottom,        right - left, stroke },
             TrayPart { left,           bottom - rise, stroke,       rise   },
             TrayPart { right - stroke, bottom - rise, stroke,       rise   } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  WholeWordButton::Layout
//
////////////////////////////////////////////////////////////////////////////////

void WholeWordButton::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    DxuiButton::Layout (boundsDip, scaler);
    m_trayScaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  WholeWordButton::Paint
//
//  The button as any other, then the tray in its label's color.
//
////////////////////////////////////////////////////////////////////////////////

void WholeWordButton::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    uint32_t  argb = theme.ButtonText();



    DxuiButton::Paint (painter, text, theme);

    if (!IsVisible())
    {
        return;
    }

    if (!IsEnabled())
    {
        argb = (argb & 0x00FFFFFFu) | (((argb >> 24) / 2) << 24);
    }

    for (const TrayPart & part : GetTrayParts (GetBounds(), m_trayScaler))
    {
        painter.FillRect (part.x, part.y, part.width, part.height, argb);
    }
}
