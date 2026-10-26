#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsInfoTip
//
//  Where a Settings page puts an info tip: just past the end of a label's
//  text, measured in the face and size the label draws in.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsInfoTip
{
public:
    static constexpr int  kGapDp = 4;

    // How wide the label's text draws. With no renderer, or a measurement
    // that fails, an average glyph width per character is used instead.
    static int   MeasureLabelPx (const DxuiLabel & label, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text);

    // A row-high square whose glyph starts kGapDp past the end of the label's
    // text.
    static RECT  GetRect        (const DxuiLabel & label, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text);
};
