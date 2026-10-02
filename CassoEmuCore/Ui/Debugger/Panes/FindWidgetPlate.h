#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetPlate
//
//  The box a pane's find widget floats in, at the top right of the pane's
//  text, as Visual Studio Code draws its find widget: a raised fill with a
//  thin edge. The widget's controls are the window's children and paint over it.
//
////////////////////////////////////////////////////////////////////////////////

class FindWidgetPlate : public IDxuiControl
{
public:
    FindWidgetPlate  () = default;
    ~FindWidgetPlate () override = default;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Panel; }
    std::wstring        GetAccessibleName () const override { return L"Find"; }

private:
    DxuiDpiScaler  m_scaler;
};
