#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PaddleBar
//
//  One paddle the view shows: what the guest calls it, "PDL1", and the value
//  it reads.
//
////////////////////////////////////////////////////////////////////////////////

struct PaddleBar
{
    static constexpr Byte  kCenter = 127;

    std::wstring  name;
    Byte          value = kCenter;

    bool operator== (const PaddleBar &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PaddleBarView
//
//  Where one of the game port's paddles is, drawn in its paddle's row as a
//  horizontal bar: a track from 0 on the left to 255 on the right, filled up
//  to the paddle's value, with a mark at the value, and the value to the
//  right of the track, right-aligned at the view's edge. The row's own label
//  gives the paddle, so the bar carries none. A paddle turns one knob, so a
//  bar says what it does where a stick's circle would suggest a second axis
//  that is not there.
//
////////////////////////////////////////////////////////////////////////////////

class PaddleBarView : public IDxuiControl
{
public:

    void               SetBar    (const PaddleBar & bar) { m_bar = bar; }
    const PaddleBar &  GetBar    () const                { return m_bar; }
    void               SetActive (bool isActive)         { m_isActive = isActive; }

    // The reading right of the track, and where along a track `width` wide
    // from `left` a value's mark falls.
    static std::wstring  FormatReading (const PaddleBar & bar);
    static float         GetMarkX      (float left, float width, Byte value);

    void  Layout    (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:

    DxuiDpiScaler  m_scaler;
    PaddleBar      m_bar;
    bool           m_isActive = false;
};
