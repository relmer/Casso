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
    m_pairRects.clear();
    m_hover   = -1;
    m_pressed = -1;

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
//  pair. A pair past the right edge is clipped with the rest of the line. The
//  pair under the pointer, or pressed, is filled behind its text first.
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
    int             pad     = m_hintScaler.ToPx ((int) kPadDip);
    uint32_t        keyArgb = theme.Accent();
    uint32_t        argb    = theme.Foreground();
    size_t          i       = 0;



    m_pairRects.assign (m_pairs.size(), RECT {});

    for (i = 0; i < m_pairs.size(); i++)
    {
        float  keyWidth  = 0.0f;
        float  textWidth = 0.0f;
        float  measured  = 0.0f;
        float  start     = x;

        if (x >= right)
        {
            break;
        }

        hr = text.MeasureString (m_pairs[i].key.c_str(), sizePx, font.face, keyWidth, measured);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.MeasureString (m_pairs[i].description.c_str(), sizePx, font.face, textWidth, measured);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_pairRects[i] = RECT { (LONG) std::floor (start) - pad, bounds.top, (LONG) std::ceil (start + keyWidth + keyGap + textWidth) + pad, bounds.bottom };

        if ((int) i == m_pressed || ((int) i == m_hover && m_pressed < 0))
        {
            painter.FillRoundedRect ((float) m_pairRects[i].left, (float) m_pairRects[i].top, (float) (m_pairRects[i].right - m_pairRects[i].left),
                                     (float) (m_pairRects[i].bottom - m_pairRects[i].top), (float) pad,
                                     ((int) i == m_pressed) ? theme.ButtonPressed() : theme.ButtonHover());
        }

        hr = text.DrawString (m_pairs[i].key.c_str(), x, top, (std::max) (0.0f, right - x), height, keyArgb, sizePx, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += keyWidth + keyGap;

        if (x >= right)
        {
            break;
        }

        hr = text.DrawString (m_pairs[i].description.c_str(), x, top, (std::max) (0.0f, right - x), height, argb, sizePx, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += textWidth + pairGap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyHintLine::GetPairAt
//
////////////////////////////////////////////////////////////////////////////////

int KeyHintLine::GetPairAt (POINT pt) const
{
    size_t  i = 0;



    if (!IsVisible())
    {
        return -1;
    }

    for (i = 0; i < m_pairRects.size(); i++)
    {
        const RECT  & r = m_pairRects[i];

        if (pt.x >= r.left && pt.x < r.right && pt.y >= r.top && pt.y < r.bottom)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyHintLine::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool KeyHintLine::OnMouse (const DxuiMouseEvent & ev)
{
    int   at      = GetPairAt (ev.positionDip);
    int   pressed = m_pressed;
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        handled = at != m_hover;
        m_hover = at;
        break;

    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left && at >= 0)
        {
            m_pressed = at;
            handled   = true;
        }

        break;

    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left && pressed >= 0)
        {
            m_pressed = -1;
            handled   = true;

            if (at == pressed && m_onPick)
            {
                m_onPick (m_pairs[(size_t) at].id);
            }
        }

        break;

    default:
        break;
    }

    return handled;
}
