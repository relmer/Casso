#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDropPreview
//
//  Where a floating toolbar dragged over a window's dock bands goes if the
//  button comes up there: an accent tint over the band it would join, or
//  over the row a new band would take, with an accent outline, painted above
//  the window's content while the drag lasts. It takes no input.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbarDropPreview : public IDxuiControl
{
public:
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    DxuiDpiScaler  m_scaler;
};
