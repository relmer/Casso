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
////////////////////////////////////////////////////////////////////////////////

class KeyHintLine : public DxuiLabel
{
public:
    struct Pair
    {
        std::wstring  key;
        std::wstring  description;
    };

    KeyHintLine() = default;
    ~KeyHintLine() override = default;

    void                       SetPairs (std::vector<Pair> pairs);
    const std::vector<Pair> &  GetPairs () const { return m_pairs; }

    //  The text of a line of pairs, each key and its description apart by
    //  one space and each pair from the next by kPairSpaces.
    static std::wstring  JoinPairs (const std::vector<Pair> & pairs);

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    static constexpr int    kPairSpaces  = 3;
    static constexpr float  kKeyGapDip   = 5.0f;
    static constexpr float  kPairGapDip  = 22.0f;

private:
    std::vector<Pair>  m_pairs;
    DxuiDpiScaler      m_hintScaler;
};
