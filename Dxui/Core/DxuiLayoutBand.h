#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLayoutBand
//
//  A control that holds nothing and draws nothing: it is here to carry a rect
//  through a layout. Stamp the thickness a band needs, hand it to a
//  DxuiDockLayout among the other children, and read back the rect the layout
//  gave it to place the widget that band stands for. A band docked Fill takes
//  what the others leave and needs no thickness.
//
//  Bands are never parented, painted or hit-tested, so a window can dock the
//  edges of its chrome without the widgets themselves becoming children of
//  one panel.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiLayoutBand : public IDxuiControl
{
public:
    //  The extent on the docked axis, in whatever units the layout runs in.
    //  Square, so one band works docked to any side.
    void  SetThickness (int thickness)  { SetBounds (RECT { 0, 0, thickness, thickness }); }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        UNREFERENCED_PARAMETER (scaler);
        SetBounds (boundsDip);
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        UNREFERENCED_PARAMETER (painter);
        UNREFERENCED_PARAMETER (text);
        UNREFERENCED_PARAMETER (theme);
    }
};
