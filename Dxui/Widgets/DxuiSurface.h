#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSurface
//
//  The simplest leaf control: fills its bounds with one theme colour. Used
//  to back a chrome band (e.g. the drive bar) so the band reads as the
//  panel surface rather than whatever the composite left underneath. The
//  token is selected via a small enum so the fill always tracks the theme.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSurface : public IDxuiControl
{
public:
    enum class Token { Background, BackgroundElevated, Caption };

    void  SetToken (Token token) { m_token = token; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        (void) scaler;
        SetBounds (boundsDip);
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        (void) text;

        uint32_t  argb = Resolve (theme);

        painter.FillRect ((float) m_boundsDip.left,
                          (float) m_boundsDip.top,
                          (float) (m_boundsDip.right  - m_boundsDip.left),
                          (float) (m_boundsDip.bottom - m_boundsDip.top),
                          argb);
    }

private:
    uint32_t  Resolve (const IDxuiTheme & theme) const
    {
        switch (m_token)
        {
            case Token::BackgroundElevated: return theme.BackgroundElevated();
            case Token::Caption:            return theme.CaptionBackground();
            default:                        return theme.Background();
        }
    }

    Token  m_token = Token::Background;
};
