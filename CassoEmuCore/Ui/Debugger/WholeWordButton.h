#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WholeWordButton
//
//  The find widget's whole-word option: "ab" over a tray, a line with its
//  ends turned up, as Visual Studio Code's find widget draws it.
//
////////////////////////////////////////////////////////////////////////////////

class WholeWordButton : public DxuiButton
{
public:
    WholeWordButton() : DxuiButton (L"ab") {}
    ~WholeWordButton() override = default;

    //  The tray's bottom line and its two ends, in the coordinates the
    //  button paints in: left, top, width and height of each.
    struct TrayPart
    {
        float  x      = 0.0f;
        float  y      = 0.0f;
        float  width  = 0.0f;
        float  height = 0.0f;
    };

    static std::array<TrayPart, 3>  GetTrayParts (const RECT & bounds, const DxuiDpiScaler & scaler);

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    static constexpr float  kTrayHalfWidthDip = 7.0f;
    static constexpr float  kTrayDropDip      = 8.0f;
    static constexpr float  kTrayEndDip       = 3.5f;
    static constexpr float  kStrokeDip        = 1.0f;

private:
    DxuiDpiScaler  m_trayScaler;
};
