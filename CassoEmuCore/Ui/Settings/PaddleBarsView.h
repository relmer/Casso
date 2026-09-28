#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"





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
//  PaddleBarsView
//
//  Where the game port's paddles are, drawn as a horizontal bar for each: a
//  track from 0 on the left to 255 on the right, filled up to the paddle's
//  value, with a mark at the value and a label above, "PDL1  108". A paddle
//  turns one knob, so a bar says what it does where a stick's circle would
//  suggest a second axis that is not there.
//
////////////////////////////////////////////////////////////////////////////////

class PaddleBarsView : public IDxuiControl
{
public:

    void                             SetBars   (const std::vector<PaddleBar> & bars) { m_bars = bars; }
    const std::vector<PaddleBar> &   GetBars   () const                              { return m_bars; }
    void                             SetActive (bool isActive)                       { m_isActive = isActive; }

    // The label above a bar, and where along a track `width` wide from `left`
    // a value's mark falls.
    static std::wstring  FormatLabel  (const PaddleBar & bar);
    static float         GetMarkX     (float left, float width, Byte value);

    void  Layout    (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:

    DxuiDpiScaler           m_scaler;
    std::vector<PaddleBar>  m_bars;
    bool                    m_isActive = false;
};
