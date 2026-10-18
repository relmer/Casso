#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyHintLine
//
//  A line of keys and what each does, as "Space step into   O step over". Each
//  key is drawn in the theme's accent and its description in the theme's
//  text color, with a wide space between one pair and the next. The label's
//  text is the whole line, for a reader that takes the text alone.
//
//  Each pair is also a button: the pointer over it fills it with the
//  button hover color, and a click on it, pressed and released over the same
//  pair, calls the pick handler with the pair's id, as its key would.
//
////////////////////////////////////////////////////////////////////////////////

class KeyHintLine : public DxuiLabel
{
public:
    struct Pair
    {
        std::wstring  key;
        std::wstring  description;
        int           id          = 0;
    };

    using PickFn = std::function<void (int id)>;

    KeyHintLine() = default;
    ~KeyHintLine() override = default;

    void                       SetPairs  (std::vector<Pair> pairs);
    const std::vector<Pair> &  GetPairs  () const        { return m_pairs; }
    void                       SetOnPick (PickFn onPick) { m_onPick = std::move (onPick); }

    //  The pair under a point, in the coordinates the line was painted in, or
    //  -1; the rects are those of the last paint.
    int   GetPairAt (POINT pt) const;

    //  A move changes the pair under the pointer, and is handled only when it
    //  did; a left press on a pair, and its release, are handled.
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    //  The text of a line of pairs, each key and its description apart by
    //  one space and each pair from the next by kPairSpaces.
    static std::wstring  JoinPairs (const std::vector<Pair> & pairs);

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    static constexpr int    kPairSpaces  = 3;
    static constexpr float  kKeyGapDip   = 5.0f;
    static constexpr float  kPairGapDip  = 22.0f;

    static constexpr float  kPadDip = 3.0f;    // the fill's margin around a pair

private:
    std::vector<Pair>           m_pairs;
    DxuiDpiScaler               m_hintScaler;
    PickFn                      m_onPick;
    mutable std::vector<RECT>   m_pairRects;    // each pair's button, as last painted
    int                         m_hover   = -1;
    int                         m_pressed = -1;
};
