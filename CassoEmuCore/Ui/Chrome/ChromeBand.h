#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBand
//
//  Zero-render IDxuiControl whose only job is to carry a docked chrome
//  band's pixel thickness in its GetBounds() so DxuiDockLayout can arrange
//  the emulator viewport around the title bar, nav strip, and drive bar.
//  Never painted -- the shell's chrome / the host own chrome rendering; these
//  bands exist purely to feed the dock's inset math (replacing the old
//  LayoutManager edge-contributor model).
//
////////////////////////////////////////////////////////////////////////////////

class ChromeBand : public IDxuiControl
{
public:
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
};
