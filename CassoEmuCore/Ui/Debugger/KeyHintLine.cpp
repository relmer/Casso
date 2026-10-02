#include "Pch.h"

#include "Ui/Debugger/KeyHintLine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyHintLine::SetPairs
//
////////////////////////////////////////////////////////////////////////////////

void KeyHintLine::SetPairs (std::vector<Pair> pairs)
{
    m_pairs = std::move (pairs);
    SetText (JoinPairs (m_pairs));
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyHintLine::JoinPairs
//
////////////////////////////////////////////////////////////////////////////////

std::wstring KeyHintLine::JoinPairs (const std::vector<Pair> & pairs)
{
    std::wstring  joined;



    for (const Pair & pair : pairs)
    {
        if (!joined.empty())
        {
            joined.append ((size_t) kPairSpaces, L' ');
        }

        joined += pair.key + L" " + pair.description;
    }

    return joined;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyHintLine::Layout
//
////////////////////////////////////////////////////////////////////////////////

void KeyHintLine::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    DxuiLabel::Layout (boundsDip, scaler);
    m_hintScaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyHintLine::Paint
//
//  Each pair left to right in the body face: the key in the accent, a short
//  gap, the description in the text color, then a wide gap before the next
//  pair. A pair past the right edge is clipped with the rest of the line.
//
////////////////////////////////////////////////////////////////////////////////

void KeyHintLine::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT         hr      = S_OK;
    DxuiFontHandle  font    = theme.BodyFont();
    const RECT    & bounds  = GetRect();
    float           x       = (float) bounds.left;
    float           top     = (float) bounds.top;
    float           height  = (float) (bounds.bottom - bounds.top);
    float           right   = (float) bounds.right;
    float           sizePx  = m_hintScaler.ToPxf (font.sizeDip);
    float           keyGap  = m_hintScaler.ToPxf (kKeyGapDip);
    float           pairGap = m_hintScaler.ToPxf (kPairGapDip);
    uint32_t        keyArgb = theme.Accent();
    uint32_t        argb    = theme.Foreground();



    (void) painter;

    for (const Pair & pair : m_pairs)
    {
        for (const auto & [part, color, gap] : { std::tuple { &pair.key, keyArgb, keyGap }, std::tuple { &pair.description, argb, pairGap } })
        {
            float  width          = 0.0f;
            float  measuredHeight = 0.0f;

            if (x >= right)
            {
                break;
            }

            hr = text.MeasureString (part->c_str(), sizePx, font.face, width, measuredHeight);
            IGNORE_RETURN_VALUE (hr, S_OK);

            hr = text.DrawString (part->c_str(), x, top, (std::max) (0.0f, right - x), height, color, sizePx, font.face,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);

            x += width + gap;
        }
    }
}
