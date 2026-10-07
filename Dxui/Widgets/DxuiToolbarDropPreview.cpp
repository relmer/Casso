#include "Pch.h"

#include "Widgets/DxuiToolbarDropPreview.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDropPreview::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDropPreview::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler = scaler;

    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDropPreview::Paint
//
//  The same tint and outline a docked toolbar's band takes while it is
//  carried, so a drop reads the same from either.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDropPreview::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr uint32_t  kTintAlpha = 0x30u << 24;



    RECT      bounds = GetBounds();
    float     width  = (float) (bounds.right  - bounds.left);
    float     height = (float) (bounds.bottom - bounds.top);
    float     line   = (std::max) (1.0f, m_scaler.ToPxf (1.0f));
    uint32_t  tint   = (theme.Accent() & 0x00FFFFFFu) | kTintAlpha;



    UNREFERENCED_PARAMETER (text);

    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    painter.FillRect    ((float) bounds.left, (float) bounds.top, width, height, tint);
    painter.OutlineRect ((float) bounds.left, (float) bounds.top, width, height, line, theme.Accent());
}





