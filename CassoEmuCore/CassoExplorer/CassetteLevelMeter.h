#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteLevelMeter
//
//  A sound input's level as a bar across its rect, in the accent color, or
//  in the error color when the level is too quiet for the cassette link to
//  read or loud enough to clip. Pure render: no hit testing and no focus.
//
////////////////////////////////////////////////////////////////////////////////

class CassetteLevelMeter : public IDxuiControl
{
public:
    static constexpr float  kQuiet = 0.05f;
    static constexpr float  kLoud  = 0.95f;

    void   SetLevel     (float level) { m_level = std::clamp (level, 0.0f, 1.0f); }
    float  GetLevel     () const      { return m_level; }

    //  Whether the level is outside what the link reads well.
    static bool  IsOutOfRange (float level) { return level < kQuiet || level > kLoud; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        (void) scaler;
        SetBounds (boundsDip);
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        constexpr float  kInset = 0.25f;

        RECT   bounds = GetBounds();
        float  width  = (float) (bounds.right - bounds.left);
        float  height = (float) (bounds.bottom - bounds.top);
        float  top    = (float) bounds.top + height * kInset;
        float  bar    = height * (1.0f - 2.0f * kInset);

        (void) text;

        if (width <= 0.0f || bar <= 0.0f)
        {
            return;
        }

        painter.OutlineRect ((float) bounds.left, top, width, bar, 1.0f, theme.Border());
        painter.FillRect    ((float) bounds.left, top, width * m_level, bar, IsOutOfRange (m_level) ? theme.ErrorForeground() : theme.Accent());
    }

private:
    float  m_level = 0.0f;
};
