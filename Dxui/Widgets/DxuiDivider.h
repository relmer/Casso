#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDivider
//
//  A horizontal rule: one line in the theme's divider color, centered
//  vertically in its rect and running the rect's full width. The owner sets
//  the rect on every layout, so the rule follows the container when it is
//  resized. Pure render: no hit testing and no focus.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDivider : public IDxuiControl
{
public:
    void  SetRect (const RECT & rect) { SetBounds (rect); }
    void  SetDpi  (UINT dpi)          { m_dpi = dpi; }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        SetBounds (boundsDip);
        m_dpi = scaler.GetDpi();
    }

    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    static constexpr int  kBaseDpi = 96;

    UINT  m_dpi = kBaseDpi;
};
