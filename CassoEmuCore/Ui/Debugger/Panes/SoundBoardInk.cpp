#include "Pch.h"

#include "Ui/Debugger/Panes/SoundBoardInk.h"
#include "Ui/Debugger/Panes/SoundBoardLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::Mix
//
////////////////////////////////////////////////////////////////////////////////

uint32_t SoundBoardInk::Mix (uint32_t from, uint32_t to, float amount)
{
    uint32_t  out = 0;



    for (int shift = 0; shift <= 24; shift += 8)
    {
        float  a = (float) ((from >> shift) & 0xFF);
        float  b = (float) ((to   >> shift) & 0xFF);

        out |= (uint32_t) std::lround (a + (b - a) * amount) << shift;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::WithAlpha
//
////////////////////////////////////////////////////////////////////////////////

uint32_t SoundBoardInk::WithAlpha (uint32_t argb, float alpha)
{
    constexpr uint32_t  kRgbMask    = 0x00FFFFFF;
    constexpr int       kAlphaShift = 24;
    constexpr float     kFull       = 255.0f;



    return (argb & kRgbMask) | ((uint32_t) std::lround (alpha * kFull) << kAlphaShift);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::AddSpot
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardInk::AddSpot (const Painting & p, const RECT & rect, std::wstring tip)
{
    p.spots.push_back ({ rect, std::move (tip) });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::PaintString
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardInk::PaintString (
    const Painting        & p,
    const std::wstring    & value,
    const RECT            & rect,
    uint32_t                color,
    const DxuiFontHandle  & font,
    DxuiTextHAlign          align) const
{
    HRESULT  hr = p.text.DrawString (value.c_str(), (float) rect.left, (float) rect.top, (float) (rect.right - rect.left), (float) (rect.bottom - rect.top),
                                     color, m_scaler.ToPxf (font.sizeDip), font.face, align, DxuiTextVAlign::Center, font.weight, false);



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::PaintCard
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardInk::PaintCard (const Painting & p, const RECT & card, const std::wstring & title, const std::wstring & detail) const
{
    constexpr float  kRadiusDip = 7.0f;
    constexpr float  kEdgeDip   = 1.0f;
    constexpr int    kTitleWide = 90;
    float            radius     = m_scaler.ToPxf (kRadiusDip);
    float            x          = (float) card.left;
    float            y          = (float) card.top;
    float            w          = (float) (card.right - card.left);
    float            h          = (float) (card.bottom - card.top);
    int              pad        = m_scaler.ToPx (kPadDip);
    int              left       = card.left + pad;
    int              top        = card.top + pad;
    int              bottom     = top + m_scaler.ToPx (kTitleDip);
    int              split      = left + m_scaler.ToPx (kTitleWide);



    p.painter.FillRoundedRect    (x, y, w, h, radius, p.card);
    p.painter.OutlineRoundedRect (x, y, w, h, radius, m_scaler.ToPxf (kEdgeDip), p.edge);

    PaintString (p, title,  { left,  top, split,                 bottom }, p.theme.HeadingForeground(), p.theme.BodyBoldFont());
    PaintString (p, detail, { split, top, card.right - pad,      bottom }, p.theme.ForegroundMuted(),   p.theme.CaptionFont());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::PaintLamp
//
//  An unlit lamp is an outline. A lit one has a halo around it.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardInk::PaintLamp (const Painting & p, float x, float y, const wchar_t * label, bool isOn, uint32_t color, float width) const
{
    constexpr float  kRadiusDip    = 4.0f;
    constexpr float  kHaloDip      = 8.0f;
    constexpr float  kHaloAlpha    = 0.28f;
    constexpr float  kUnlitAlpha   = 0.35f;
    constexpr float  kLabelGapDip  = 8.0f;
    float            radius        = m_scaler.ToPxf (kRadiusDip);
    float            cx            = x + radius;
    float            labelX        = cx + radius + m_scaler.ToPxf (kLabelGapDip);



    if (isOn)
    {
        p.painter.FillCircle (cx, y, m_scaler.ToPxf (kHaloDip), WithAlpha (color, kHaloAlpha));
        p.painter.FillCircle (cx, y, radius, color);
    }
    else
    {
        p.painter.FillCircle (cx, y, radius, WithAlpha (p.theme.ForegroundMuted(), kUnlitAlpha));
    }

    PaintString (p, label,
                 { (long) labelX, (long) (y - m_scaler.ToPxf (kRegisterDip / 2.0f)), (long) (x + width), (long) (y + m_scaler.ToPxf (kRegisterDip / 2.0f)) },
                 isOn ? p.theme.Foreground() : p.theme.ForegroundMuted(), p.theme.CaptionFont());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::PaintValue
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardInk::PaintValue (const Painting & p, const RECT & rect, const std::wstring & label, const std::wstring & value, int labelDip) const
{
    int  split = std::min (rect.left + m_scaler.ToPx (labelDip), (rect.left + rect.right) / 2);



    PaintString (p, label, { rect.left, rect.top, split,      rect.bottom }, p.theme.ForegroundMuted(), p.theme.CaptionFont());
    PaintString (p, value, { split,     rect.top, rect.right, rect.bottom }, p.theme.Foreground(),      p.theme.MonospaceFont());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::PaintMuteButton
//
//  A mute is a button: an edge around its fill, a brighter fill under the
//  pointer, and the focus ring while the keyboard has it, so it doesn't
//  read as a label.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardInk::PaintMuteButton (const Painting & p, const RECT & rect, int channel, bool isMuted, uint32_t color) const
{
    constexpr float  kRadiusDip   = 4.0f;
    constexpr float  kEdgeDip     = 1.0f;
    constexpr float  kRingGapDip  = 2.0f;
    constexpr float  kFillAlpha   = 0.16f;
    constexpr float  kHoverAlpha  = 0.32f;
    constexpr float  kEdgeAlpha   = 0.60f;
    float            radius       = m_scaler.ToPxf (kRadiusDip);
    float            x            = (float) rect.left;
    float            y            = (float) rect.top;
    float            w            = (float) (rect.right - rect.left);
    float            h            = (float) (rect.bottom - rect.top);
    float            ring         = m_scaler.ToPxf (kRingGapDip);
    uint32_t         ink          = isMuted ? p.theme.ErrorForeground() : color;



    p.painter.FillRoundedRect    (x, y, w, h, radius, WithAlpha (ink, channel == p.hoverMute ? kHoverAlpha : kFillAlpha));
    p.painter.OutlineRoundedRect (x, y, w, h, radius, m_scaler.ToPxf (kEdgeDip), WithAlpha (ink, kEdgeAlpha));

    if (channel == p.focusedMute)
    {
        p.painter.OutlineRoundedRect (x - ring, y - ring, w + 2.0f * ring, h + 2.0f * ring, radius + ring,
                                      m_scaler.ToPxf (p.theme.FocusRingWidthDip()), p.theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::PaintTrace
//
//  Wide faint passes under a thin bright one make the phosphor's glow. Each
//  segment is a line with round ends, so two overlap at the point they
//  share, and a pass drawn see-through was twice as bright there, which
//  dotted the curve. Each pass is drawn solid instead, in the color it blends
//  to over the screen and the passes under it: the same color, with no
//  overlap to see.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardInk::PaintTrace (const Painting & p, const std::vector<std::pair<float, float>> & points, uint32_t color, TraceLayer layer) const
{
    struct Pass
    {
        float  widthDip;
        float  alpha;
    };

    static constexpr Pass  kPasses[]   = { { 5.0f, 0.10f }, { 3.0f, 0.22f }, { 1.3f, 1.0f } };
    constexpr int          kAlphaShift = 24;
    constexpr uint32_t     kOpaque     = 0xFF000000;
    constexpr float        kFull       = 255.0f;
    constexpr size_t       kCore       = std::size (kPasses) - 1;
    float                  strength    = (float) (color >> kAlphaShift) / kFull;
    uint32_t               under       = p.screen | kOpaque;



    for (size_t pass = 0; pass < std::size (kPasses); pass++)
    {
        bool  isDrawn = (pass == kCore) ? layer != TraceLayer::Glow : layer != TraceLayer::Core;

        under = Mix (under, color | kOpaque, kPasses[pass].alpha * strength);

        for (size_t i = 1; i < points.size() && isDrawn; i++)
        {
            p.painter.DrawLine (points[i - 1].first, points[i - 1].second, points[i].first, points[i].second, m_scaler.ToPxf (kPasses[pass].widthDip), under);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardInk::PaintScope
//
//  An AY channel's samples run from 0 to its top level, so a quiet channel is a
//  small trace. A speech chip's run either side of 0 and fill the screen.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardInk::PaintScope (const Painting & p, const std::vector<float> & scope, const RECT & rect, uint32_t color, double hz, bool isBipolar) const
{
    constexpr float                       kRadiusDip   = 4.0f;
    constexpr float                       kInsetDip    = 4.0f;
    constexpr float                       kGridAlpha   = 0.10f;
    constexpr float                       kLeastPeak   = 0.02f;
    constexpr int                         kGridColumns = 8;
    constexpr int                         kColumnDip   = 2;
    float                                 x            = (float) rect.left;
    float                                 y            = (float) rect.top;
    float                                 w            = (float) (rect.right - rect.left);
    float                                 h            = (float) (rect.bottom - rect.top);
    float                                 inset        = m_scaler.ToPxf (kInsetDip);
    float                                 low          = 0.0f;
    float                                 high         = 1.0f;
    size_t                                window       = SoundBoardLayout::GetScopeWindow (hz, p.sampleRate);
    size_t                                first        = 0;
    std::vector<std::pair<float, float>>  points;



    p.painter.FillRoundedRect (x, y, w, h, m_scaler.ToPxf (kRadiusDip), p.screen);

    for (int g = 1; g < kGridColumns; g++)
    {
        float  gx = x + w * (float) g / (float) kGridColumns;

        p.painter.DrawLine (gx, y, gx, y + h, 1.0f, WithAlpha (p.theme.ForegroundMuted(), kGridAlpha));
    }

    p.painter.DrawLine (x, y + h * 0.5f, x + w, y + h * 0.5f, 1.0f, WithAlpha (p.theme.ForegroundMuted(), kGridAlpha * 2.0f));

    if (scope.empty())
    {
        return;
    }

    if (isBipolar)
    {
        for (float sample : scope)
        {
            high = std::max (high == 1.0f ? kLeastPeak : high, std::abs (sample));
        }

        low = -high;
    }

    first = SoundBoardLayout::FindTrigger (scope, window);

    //  A speech chip's waveform is a curve, so it's a line through each
    //  pixel's mean. An AY's is a square wave or noise, so each column has
    //  its lowest and highest sample, so a square wave's edges are drawn
    //  upright.
    if (isBipolar)
    {
        std::vector<float>  trace = SoundBoardLayout::GetScopeTrace (scope, first, window, std::max (2, (int) (w - 2.0f * inset)), low, high);

        for (size_t c = 0; c < trace.size(); c++)
        {
            points.emplace_back (x + inset + (float) c, y + h - inset - trace[c] * (h - 2.0f * inset));
        }
    }
    else
    {
        for (const SoundBoardLayout::ScopeColumn & column : SoundBoardLayout::GetScopeColumns (scope, first, window, std::max (1, (int) (w - 2.0f * inset) / m_scaler.ToPx (kColumnDip)), low, high))
        {
            float  cx = x + inset + (float) points.size() / 2.0f * (float) m_scaler.ToPx (kColumnDip);

            points.emplace_back (cx, y + h - inset - column.high * (h - 2.0f * inset));
            points.emplace_back (cx, y + h - inset - column.low  * (h - 2.0f * inset));
        }
    }

    p.painter.PushClip (x, y, w, h);
    PaintTrace (p, points, color);
    p.painter.PopClip();
}





