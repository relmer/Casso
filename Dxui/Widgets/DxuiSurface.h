#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSurface
//
//  The simplest leaf control: fills its bounds with one theme color. Used
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

    //  How much of the theme color actually lands, 0 through 1. A surface is
    //  normally opaque -- it exists so a band reads as the panel rather than
    //  as whatever the composite left underneath -- but a surface used as a
    //  SCRIM under an overlay wants the picture to carry on showing through
    //  it, dimmed enough that the words on top stay legible.
    //
    //  1 by default, so every existing surface fills exactly as it did.
    void  SetOpacity (float opacity)
    {
        m_opacity = (opacity < 0.0f) ? 0.0f : ((opacity > 1.0f) ? 1.0f : opacity);
    }

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
        uint32_t  argb  = 0;
        float     alpha = 0.0f;

        switch (m_token)
        {
            case Token::BackgroundElevated: argb = theme.BackgroundElevated(); break;
            case Token::Caption:            argb = theme.CaptionBackground(); break;
            default:                        argb = theme.Background();       break;
        }

        //  Scaled rather than replaced: a theme is free to hand back a color
        //  that is already partly transparent, and a scrim over one of those
        //  should end up thinner than the theme meant, not thicker.
        alpha = (float) ((argb >> 24) & 0xFFu) * m_opacity;

        return (argb & 0x00FFFFFFu) | ((uint32_t) (alpha + 0.5f) << 24);
    }

    Token  m_token   = Token::Background;
    float  m_opacity = 1.0f;
};
