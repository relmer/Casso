#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCustomVisual
//
//  A leaf control that renders Direct3D 11 content of its own into its
//  bounds, through IDxuiPainter::DrawCustom: over what the controls before it
//  painted, under what the controls after it paint and under all text. The
//  owner gives the draw; the control only places it.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiCustomVisual : public IDxuiControl
{
public:
    void  SetDraw (DxuiCustomDraw draw) { m_draw = std::move (draw); }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        (void) scaler;
        SetBounds (boundsDip);
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        (void) text;
        (void) theme;

        if (m_draw)
        {
            painter.DrawCustom (m_boundsDip, m_draw);
        }
    }

private:
    DxuiCustomDraw  m_draw;
};
