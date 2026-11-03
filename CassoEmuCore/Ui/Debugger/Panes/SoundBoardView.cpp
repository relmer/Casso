#include "Pch.h"

#include "Ui/Debugger/Panes/SoundBoardView.h"

#include "Core/TextEncoding.h"
#include "Devices/Ay8910.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/Panes/SoundBoardLayout.h"
#include "Ui/Debugger/Panes/SoundBoardTips.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::SetBoard
//
//  A meter's peak falls a little with each payload, so a level that drops has
//  a mark above it for a moment.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::SetBoard (const DiagnosticsSoundBoard & board)
{
    constexpr float  kPeakFall = 0.93f;
    size_t           at        = 0;



    m_board = board;

    for (const DiagnosticsSoundHalf & half : m_board.halves)
    {
        for (const DiagnosticsSoundChannel & channel : half.psg.channels)
        {
            if (at < m_peaks.size())
            {
                m_peaks[at] = std::max ((float) channel.level, m_peaks[at] * kPeakFall);
                at++;
            }
        }
    }

    Reflow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetVoiceHeight
//
////////////////////////////////////////////////////////////////////////////////

int SoundBoardView::GetVoiceHeight() const
{
    return m_scaler.ToPx (kPadDip + kTitleDip + kPhonemeDip + kGapDip + kStripHeadDip + kStripDip + kHistoryDip + kGapDip + kValueRows * kRegisterDip + kPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetHalfHeight
//
////////////////////////////////////////////////////////////////////////////////

int SoundBoardView::GetHalfHeight() const
{
    return m_scaler.ToPx (kPadDip + kTitleDip + DiagnosticsSoundPsg::kChannelCount * (kChannelDip + kChannelGapDip)
                          + kEnvelopeDip + kGapDip + kRegisterRows * kRegisterDip + kPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetClientWidth
//
////////////////////////////////////////////////////////////////////////////////

int SoundBoardView::GetClientWidth() const
{
    int  gaps = 2 * m_scaler.ToPx (kGapDip);
    int  bar  = m_hasScrollbar ? m_scaler.ToPx (kScrollbarDip) : 0;



    return std::max (0, (int) (m_boundsDip.right - m_boundsDip.left) - gaps - bar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::Reflow
//
//  Places the cards, then the parts with tips. A scrollbar narrows the cards,
//  which can change how tall they are together, so a placement without one
//  comes first, and one with it follows only if that is too tall.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::Reflow()
{
    int               gap     = m_scaler.ToPx (kGapDip);
    int               height  = (int) (m_boundsDip.bottom - m_boundsDip.top) - 2 * gap;
    int               voice   = m_board.voice.has_value() ? GetVoiceHeight() + gap : 0;
    int               columns = 1;
    int               below   = 0;
    std::vector<int>  heights (m_board.halves.size(), GetHalfHeight());



    for (bool hasBar : { false, true })
    {
        m_hasScrollbar = hasBar;
        columns        = SoundBoardLayout::GetColumnCount (GetClientWidth(), m_scaler.ToPx (kLeastCardDip), gap, (int) heights.size());
        m_halfCards    = heights.empty() ? std::vector<RECT>() : SoundBoardLayout::PlaceCards (heights, columns, GetClientWidth(), gap, below);

        if (voice + below <= height)
        {
            break;
        }
    }

    m_contentHeight = voice + below;
    m_voiceCard     = { 0, 0, GetClientWidth(), voice - gap };

    for (RECT & card : m_halfCards)
    {
        OffsetRect (&card, 0, voice);
    }

    m_regions.clear();

    if (m_board.voice.has_value())
    {
        BuildVoiceRegions (*m_board.voice, m_voiceCard);
    }

    for (size_t i = 0; i < m_board.halves.size() && i < m_halfCards.size(); i++)
    {
        BuildHalfRegions (m_board.halves[i], m_halfCards[i]);
    }

    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::SyncScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::SyncScrollbar()
{
    constexpr int   kArrowStepDip = 40;
    int             bar           = m_scaler.ToPx (kScrollbarDip);
    int             page          = std::max (0, (int) (m_boundsDip.bottom - m_boundsDip.top));
    DxuiScrollInfo  info;



    m_scrollTop = std::clamp (m_scrollTop, 0, std::max (0, m_contentHeight + 2 * m_scaler.ToPx (kGapDip) - page));

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = m_contentHeight + 2 * m_scaler.ToPx (kGapDip);
    info.nPage = (UINT) page;
    info.nPos  = m_scrollTop;

    m_scrollbar.Configure (DxuiScrollbar::Orientation::Vertical, bar, bar, m_scaler.ToPx (kArrowStepDip));
    m_scrollbar.SetTrack  ({ m_boundsDip.right - bar, m_boundsDip.top, m_boundsDip.right, m_boundsDip.bottom });
    m_scrollbar.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::ScrollTo
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::ScrollTo (int top)
{
    m_scrollTop = top;
    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::ScrollPhonemes
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::ScrollPhonemes (int count)
{
    size_t  oldest = (m_board.voice.has_value() && !m_board.voice->recent.empty()) ? m_board.voice->recent.size() - 1 : 0;
    int     back   = (int) m_phonemesBack + count;



    m_phonemesBack = (size_t) std::clamp (back, 0, (int) oldest);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::IsOverPhonemeStrip
//
//  The strip and its row of labels.
//
////////////////////////////////////////////////////////////////////////////////

bool SoundBoardView::IsOverPhonemeStrip (POINT clientPx) const
{
    RECT  strip = {};



    if (!m_board.voice.has_value())
    {
        return false;
    }

    strip = ToClient (GetStripArea (m_voiceCard));
    return PtInRect (&strip, clientPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::ToClient
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundBoardView::ToClient (const RECT & content) const
{
    RECT  placed = content;
    int   gap    = m_scaler.ToPx (kGapDip);



    OffsetRect (&placed, m_boundsDip.left + gap, m_boundsDip.top + gap - m_scrollTop);
    return placed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetChannelRect
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundBoardView::GetChannelRect (const RECT & card, int channel) const
{
    int  pad = m_scaler.ToPx (kPadDip);
    int  top = card.top + pad + m_scaler.ToPx (kTitleDip) + channel * m_scaler.ToPx (kChannelDip + kChannelGapDip);



    return { card.left + pad, top, card.right - pad, top + m_scaler.ToPx (kChannelDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetMuteOfChannel
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundBoardView::GetMuteOfChannel (const RECT & row) const
{
    constexpr int  kMuteDip = 28;
    int            side     = m_scaler.ToPx (kMuteDip);
    int            top      = (row.top + row.bottom - side) / 2;



    return { row.left, top, row.left + side, top + side };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetMuteOfVoice
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundBoardView::GetMuteOfVoice (const RECT & card) const
{
    constexpr int  kMuteDip = 24;
    int            side     = m_scaler.ToPx (kMuteDip);
    int            pad      = m_scaler.ToPx (kPadDip);



    return { card.right - pad - side, card.top + pad, card.right - pad, card.top + pad + side };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetEnvelopeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundBoardView::GetEnvelopeRect (const RECT & card) const
{
    RECT  last = GetChannelRect (card, DiagnosticsSoundPsg::kChannelCount - 1);
    int   top  = last.bottom + m_scaler.ToPx (kChannelGapDip);



    return { last.left, top, last.right, top + m_scaler.ToPx (kEnvelopeDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetMuteRect
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundBoardView::GetMuteRect (int channel) const
{
    for (const Region & region : m_regions)
    {
        if (region.muteChannel == channel)
        {
            return ToClient (region.rect);
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::AddRegion
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::AddRegion (const RECT & rect, std::wstring tip, int muteChannel)
{
    m_regions.push_back ({ rect, std::move (tip), muteChannel });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::BuildHalfRegions
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::BuildHalfRegions (const DiagnosticsSoundHalf & half, const RECT & card)
{
    static constexpr wchar_t  kNames[]  = { L'A', L'B', L'C' };
    constexpr int             kChannels = DiagnosticsSoundPsg::kChannelCount;
    constexpr int             kRegsEach = 2;
    RECT                      envelope  = GetEnvelopeRect (card);
    int                       pad       = m_scaler.ToPx (kPadDip);



    for (int c = 0; c < kChannels; c++)
    {
        const DiagnosticsSoundChannel  & channel = half.psg.channels[c];
        RECT                             row     = GetChannelRect (card, c);
        int                              fine    = c * kRegsEach;
        int                              level   = Ay8910::kRegAmpA + c;

        AddRegion (GetMuteOfChannel (row),
                   std::format (L"{} channel {}", channel.isMuted ? L"Unmute" : L"Mute", kNames[c]),
                   half.index * kChannels + c);

        AddRegion (row, std::format (L"Channel {}: tone period ${:04X} (R{}, R{}), mixer ${:02X} (R7), amplitude {}{} (R{})",
                                     kNames[c], channel.period, fine, fine + 1, half.psg.mixer, channel.level,
                                     channel.usesEnvelope ? L" from the envelope" : L"", level));
    }

    AddRegion (envelope, std::format (L"Envelope period ${:04X} (R11, R12), R13 ${:02X}. Noise period ${:02X} (R6)",
                                      half.psg.envelopePeriod, half.psg.envelopeRegister, half.psg.noisePeriod));

    AddRegion ({ card.left, card.top, card.right, card.top + pad + m_scaler.ToPx (kTitleDip) },
               std::format (L"6522 #{} at ${:04X}. IFR ${:02X}, IER ${:02X}", half.index + 1, half.baseAddress, half.via.ifr, half.via.ier));

    AddRegion ({ envelope.left, envelope.bottom, envelope.right, card.bottom },
               std::format (L"ORB ${:02X} and DDRB ${:02X} set the AY's bus lines. ACR ${:02X}, PCR ${:02X}", half.via.orb, half.via.ddrb, half.via.acr, half.via.pcr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::BuildVoiceRegions
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::BuildVoiceRegions (const DiagnosticsSoundVoice & voice, const RECT & card)
{
    AddRegion (GetMuteOfVoice (card), voice.isMuted ? L"Unmute the speech chip" : L"Mute the speech chip", kVoiceMuteChannel);

    AddRegion (GetStripArea (card), L"Recent phonemes: the ones before the one playing, oldest at the left. Each block is as wide as the phoneme lasted, and the row under it has their labels, with an arrow under the one playing. The wheel scrolls back through the last 256.");

    AddRegion (card, std::format (L"Phoneme ${:02X}, duration ${:X}, rate ${:X}, inflection ${:03X}, articulation ${:X}, amplitude ${:X}, filter ${:02X}",
                                  voice.phonemeCode, voice.durationSelect, voice.rateSelect, voice.inflection, voice.articulation, voice.amplitude, voice.filterRegister));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::TryGetTipAt
//
//  A mute's region comes ahead of the parts the last paint drew, and those
//  ahead of the larger regions around them.
//
////////////////////////////////////////////////////////////////////////////////

bool SoundBoardView::TryGetTipAt (POINT clientPx, std::wstring & outTip) const
{
    if (!PtInRect (&m_boundsDip, clientPx) || IsOverScrollbar (clientPx))
    {
        return false;
    }

    for (bool isMute : { true, false })
    {
        for (const Region & region : m_regions)
        {
            RECT  placed = ToClient (region.rect);

            if ((region.muteChannel >= 0) == isMute && PtInRect (&placed, clientPx))
            {
                outTip = region.tip;
                return true;
            }
        }

        for (const Spot & spot : m_spots)
        {
            if (isMute && PtInRect (&spot.rect, clientPx))
            {
                outTip = spot.tip;
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::AddSpot
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::AddSpot (const Painting & p, const RECT & rect, std::wstring tip)
{
    p.spots.push_back ({ rect, std::move (tip) });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::IsOverScrollbar
//
////////////////////////////////////////////////////////////////////////////////

bool SoundBoardView::IsOverScrollbar (POINT clientPx) const
{
    return m_hasScrollbar && m_scrollbar.HitTest (clientPx.x, clientPx.y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;

    Reflow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::Tick
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::Tick (int64_t nowMs)
{
    (void) m_scrollbar.Tick (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool SoundBoardView::OnMouse (const DxuiMouseEvent & ev)
{
    constexpr int  kWheelDip = 48;
    POINT          at        = ev.positionDip;
    bool           isOver    = PtInRect (&m_boundsDip, at) != FALSE;



    if (ev.kind == DxuiMouseEventKind::Up && m_scrollbar.IsDragging())
    {
        m_scrollbar.OnMouseUp();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        if (m_scrollbar.IsDragging())
        {
            m_scrollbar.OnMouseMove (at.x, at.y);
            ScrollTo (m_scrollbar.GetScrollPos());
            return true;
        }

        (void) m_scrollbar.SetHover (IsOverScrollbar (at), at);
        return false;
    }

    if (!isOver)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && IsOverPhonemeStrip (at))
    {
        ScrollPhonemes ((int) std::lround (ev.wheelDelta * (float) kPhonemesPerNotch));
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal)
    {
        ScrollTo (m_scrollTop - (int) std::lround (ev.wheelDelta * (float) m_scaler.ToPx (kWheelDip)));
        return true;
    }

    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left)
    {
        return false;
    }

    if (IsOverScrollbar (at))
    {
        m_scrollbar.OnMouseDown (at.x, at.y);
        ScrollTo (m_scrollbar.GetScrollPos());
        return true;
    }

    for (const Region & region : m_regions)
    {
        RECT  placed  = ToClient (region.rect);
        bool  isMuted = false;

        if (region.muteChannel < 0 || !PtInRect (&placed, at))
        {
            continue;
        }

        if (region.muteChannel == kVoiceMuteChannel)
        {
            isMuted = m_board.voice.has_value() && m_board.voice->isMuted;
        }
        else
        {
            isMuted = m_board.halves[(size_t) region.muteChannel / DiagnosticsSoundPsg::kChannelCount]
                             .psg.channels[(size_t) region.muteChannel % DiagnosticsSoundPsg::kChannelCount].isMuted;
        }

        if (m_onMute)
        {
            m_onMute (region.muteChannel, !isMuted);
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::Mix
//
////////////////////////////////////////////////////////////////////////////////

uint32_t SoundBoardView::Mix (uint32_t from, uint32_t to, float amount)
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
//  SoundBoardView::WithAlpha
//
////////////////////////////////////////////////////////////////////////////////

uint32_t SoundBoardView::WithAlpha (uint32_t argb, float alpha)
{
    constexpr uint32_t  kRgbMask    = 0x00FFFFFF;
    constexpr int       kAlphaShift = 24;
    constexpr float     kFull       = 255.0f;



    return (argb & kRgbMask) | ((uint32_t) std::lround (alpha * kFull) << kAlphaShift);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float     kScreenDark  = 0.55f;
    constexpr float     kScreenLight = 0.09f;
    constexpr float     kCardLift    = 0.035f;
    constexpr uint32_t  kBlack       = 0xFF000000;
    uint32_t            background   = theme.TextViewBackground();
    Painting          p            = { painter, text, theme,
                                       Mix (background, kBlack, DebuggerTextColors::IsDark (background) ? kScreenDark : kScreenLight),
                                       Mix (theme.ContentBackground(), theme.Foreground(), kCardLift),
                                       theme.ContentEdge(), m_spots };



    m_spots.clear();

    if (!m_visible)
    {
        return;
    }

    painter.PushClip ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top));

    if (m_board.voice.has_value())
    {
        PaintVoice (p, *m_board.voice, ToClient (m_voiceCard));
    }

    for (size_t i = 0; i < m_board.halves.size() && i < m_halfCards.size(); i++)
    {
        PaintHalf (p, m_board.halves[i], ToClient (m_halfCards[i]));
    }

    painter.PopClip();

    if (m_hasScrollbar)
    {
        m_scrollbar.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintString
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintString (
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
//  SoundBoardView::PaintCard
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintCard (const Painting & p, const RECT & card, const std::wstring & title, const std::wstring & detail) const
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
//  SoundBoardView::PaintLamp
//
//  An unlit lamp is an outline. A lit one has a halo around it.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintLamp (const Painting & p, float x, float y, const wchar_t * label, bool isOn, uint32_t color, float width) const
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
//  SoundBoardView::PaintValue
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintValue (const Painting & p, const RECT & rect, const std::wstring & label, const std::wstring & value, int labelDip) const
{
    int  split = std::min (rect.left + m_scaler.ToPx (labelDip), (rect.left + rect.right) / 2);



    PaintString (p, label, { rect.left, rect.top, split,      rect.bottom }, p.theme.ForegroundMuted(), p.theme.CaptionFont());
    PaintString (p, value, { split,     rect.top, rect.right, rect.bottom }, p.theme.Foreground(),      p.theme.MonospaceFont());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintTrace
//
//  Wide faint passes under a thin bright one make the phosphor's glow.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintTrace (const Painting & p, const std::vector<std::pair<float, float>> & points, uint32_t color) const
{
    struct Pass
    {
        float  widthDip;
        float  alpha;
    };

    static constexpr Pass  kPasses[]   = { { 5.0f, 0.10f }, { 3.0f, 0.22f }, { 1.3f, 1.0f } };
    constexpr int          kAlphaShift = 24;
    constexpr float        kFull       = 255.0f;
    float                  strength    = (float) (color >> kAlphaShift) / kFull;



    for (const Pass & pass : kPasses)
    {
        for (size_t i = 1; i < points.size(); i++)
        {
            p.painter.DrawLine (points[i - 1].first, points[i - 1].second, points[i].first, points[i].second, m_scaler.ToPxf (pass.widthDip), WithAlpha (color, pass.alpha * strength));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintScope
//
//  An AY channel's samples run from 0 to its top level, so a quiet channel is a
//  small trace. A speech chip's run either side of 0 and fill the screen.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintScope (const Painting & p, const std::vector<float> & scope, const RECT & rect, uint32_t color, double hz, bool isBipolar) const
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
    size_t                                window       = SoundBoardLayout::GetScopeWindow (hz, m_board.sampleRate);
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

    for (const SoundBoardLayout::ScopeColumn & column : SoundBoardLayout::GetScopeColumns (scope, first, window, std::max (1, (int) (w - 2.0f * inset) / m_scaler.ToPx (kColumnDip)), low, high))
    {
        float  cx = x + inset + (float) points.size() / 2.0f * (float) m_scaler.ToPx (kColumnDip);

        points.emplace_back (cx, y + h - inset - column.high * (h - 2.0f * inset));
        points.emplace_back (cx, y + h - inset - column.low  * (h - 2.0f * inset));
    }

    p.painter.PushClip (x, y, w, h);
    PaintTrace (p, points, color);
    p.painter.PopClip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintChannel
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintChannel (const Painting & p, const DiagnosticsSoundPsg & psg, const RECT & row, int index, int muteChannel) const
{
    static constexpr const wchar_t * kNames[]    = { L"A", L"B", L"C" };
    const DiagnosticsSoundChannel  & channel      = psg.channels[(size_t) index];
    Byte                             amplitude    = psg.registers[(size_t) (8 + index)];
    constexpr int                    kNoteDip     = 98;
    constexpr int                    kSourceDip   = 18;
    constexpr int                    kMeterDip    = 118;
    constexpr int                    kGapPartDip  = 8;
    constexpr int                    kNoteRowDip  = 22;
    constexpr float                  kMuteMix     = 0.65f;
    constexpr float                  kPillRadius  = 4.0f;
    constexpr float                  kPillAlpha   = 0.16f;
    constexpr float                  kUnlitAlpha  = 0.18f;
    constexpr int                    kSegments    = 15;
    constexpr float                  kPeakDim     = 0.8f;
    constexpr float                  kSilentAlpha = 0.35f;
    RECT                             mute         = GetMuteOfChannel (row);
    uint32_t                         color        = channel.isMuted ? Mix (m_palette.channels[index], p.theme.ForegroundMuted(), kMuteMix) : m_palette.channels[index];
    int                              gap          = m_scaler.ToPx (kGapPartDip);
    int                              noteLeft     = mute.right + gap;
    int                              noteRight    = noteLeft + m_scaler.ToPx (kNoteDip);
    int                              sourceLeft   = noteRight + gap;
    int                              sourceRight  = sourceLeft + m_scaler.ToPx (kSourceDip);
    int                              meterLeft    = sourceRight + gap;
    int                              meterRight   = meterLeft + m_scaler.ToPx (kMeterDip);
    int                              middle       = (row.top + row.bottom) / 2;
    int                              source       = m_scaler.ToPx (kSourceDip);
    float                            radius       = m_scaler.ToPxf (kPillRadius);
    std::wstring                     note         = SoundBoardLayout::GetNoteText (channel.frequencyHz);
    DxuiFontHandle                   big          = p.theme.BodyBoldFont();
    bool                             isSilent     = channel.level == 0 || (!channel.isToneOn && !channel.isNoiseOn);
    float                            segmentWide  = (float) m_scaler.ToPx (kMeterDip) / (float) kSegments;
    float                            meterTop     = (float) middle + m_scaler.ToPxf (2.0f);
    float                            meterHigh    = m_scaler.ToPxf (12.0f);
    float                            peak         = (muteChannel < (int) m_peaks.size()) ? m_peaks[(size_t) muteChannel] : 0.0f;



    big.sizeDip *= 1.25f;

    p.painter.FillRoundedRect ((float) mute.left, (float) mute.top, (float) (mute.right - mute.left), (float) (mute.bottom - mute.top), radius,
                               channel.isMuted ? WithAlpha (p.theme.ErrorForeground(), kPillAlpha) : WithAlpha (color, kPillAlpha));
    PaintString (p, kNames[index], mute, channel.isMuted ? p.theme.ErrorForeground() : color, p.theme.BodyBoldFont(), DxuiTextHAlign::Center);

    if (channel.isMuted)
    {
        p.painter.DrawLine ((float) mute.left + radius, (float) mute.bottom - radius, (float) mute.right - radius, (float) mute.top + radius, m_scaler.ToPxf (1.5f), p.theme.ErrorForeground());
    }

    if (isSilent)
    {
        PaintString (p, L"Silent", { noteLeft, row.top, noteRight, row.bottom }, p.theme.ForegroundDisabled(), p.theme.BodyFont());
    }
    else
    {
        PaintString (p, note, { noteLeft, middle - m_scaler.ToPx (kNoteRowDip), noteRight, middle }, p.theme.Foreground(), big);
        PaintString (p, SoundBoardLayout::GetFrequencyText (channel.frequencyHz), { noteLeft, middle, noteRight, middle + m_scaler.ToPx (kNoteRowDip) }, p.theme.ForegroundMuted(), p.theme.CaptionFont());
    }

    AddSpot (p, { noteLeft, row.top, noteRight, row.bottom }, SoundBoardTips::GetNote (index, channel));
    AddSpot (p, { meterLeft, row.top, meterRight, row.bottom }, SoundBoardTips::GetLevel (index, channel, amplitude));
    AddSpot (p, { meterRight + gap, row.top, row.right, row.bottom }, SoundBoardTips::GetScope (index));

    for (int s = 0; s < 2; s++)
    {
        bool  isOn = (s == 0) ? channel.isToneOn : channel.isNoiseOn;
        RECT  pill = { sourceLeft, middle - source + s * (source + 2), sourceRight, middle + s * (source + 2) };

        AddSpot (p, pill, SoundBoardTips::GetSource (index, s == 1, channel, psg.mixer));

        p.painter.FillRoundedRect ((float) pill.left, (float) pill.top, (float) source, (float) source, radius, isOn ? WithAlpha (color, kPillAlpha * 2.0f) : WithAlpha (p.theme.ForegroundMuted(), kPillAlpha / 2.0f));
        PaintString (p, (s == 0) ? L"T" : L"N", pill, isOn ? color : p.theme.ForegroundDisabled(), p.theme.CaptionFont(), DxuiTextHAlign::Center);
    }

    PaintString (p, channel.usesEnvelope ? std::format (L"{:>2} env", channel.level) : std::format (L"{:>2}", channel.level),
                 { meterLeft, middle - m_scaler.ToPx (kNoteRowDip), meterRight, middle }, p.theme.Foreground(), p.theme.MonospaceFont());

    for (int s = 0; s < kSegments; s++)
    {
        float  sx     = (float) meterLeft + (float) s * segmentWide;
        bool   isLit  = s < channel.level;
        bool   isPeak = !isLit && (int) peak == s + 1 && !channel.isMuted;
        uint32_t  fill  = isLit ? Mix (color, 0xFFFFFFFF, (float) s / (float) kSegments * 0.35f)
                                : (isPeak ? WithAlpha (color, kPeakDim) : WithAlpha (p.theme.ForegroundMuted(), kUnlitAlpha));

        p.painter.FillRect (sx, meterTop, segmentWide - m_scaler.ToPxf (1.5f), meterHigh, fill);
    }

    PaintScope (p, channel.scope, { meterRight + gap, row.top, row.right, row.bottom }, isSilent ? WithAlpha (color, kSilentAlpha) : color, channel.isToneOn && !channel.isNoiseOn ? channel.frequencyHz : 0.0, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintEnvelope
//
//  The envelope's level over its ramps, with its present level as a line
//  across and a dot where the envelope is along the ramps, its R13 bits, its
//  ramp time and the noise frequency beside it.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintEnvelope (const Painting & p, const DiagnosticsSoundPsg & psg, const RECT & rect) const
{
    static constexpr const wchar_t * kBits[]    = { L"CONT", L"ATT", L"ALT", L"HOLD" };
    constexpr Byte                        kTopBit      = 0x08;
    constexpr int                         kPictureDip  = 170;
    constexpr int                         kGapDip2     = 12;
    constexpr int                         kLineDip     = 22;
    constexpr int                         kBitDip      = 42;
    constexpr float                       kInsetDip    = 6.0f;
    constexpr float                       kRadiusDip   = 4.0f;
    constexpr float                       kMarkAlpha   = 0.55f;
    constexpr float                       kMaxLevel    = 15.0f;
    constexpr float                       kPillAlpha   = 0.20f;
    int                                   pictureRight = rect.left + m_scaler.ToPx (kPictureDip);
    int                                   textLeft     = pictureRight + m_scaler.ToPx (kGapDip2);
    int                                   textMiddle   = (textLeft + rect.right) / 2;
    int                                   line         = m_scaler.ToPx (kLineDip);
    float                                 inset        = m_scaler.ToPxf (kInsetDip);
    float                                 x            = (float) rect.left;
    float                                 y            = (float) rect.top;
    float                                 w            = (float) (pictureRight - rect.left);
    float                                 h            = (float) (rect.bottom - rect.top);
    uint32_t                              color        = m_palette.voice;
    float                                 markY        = y + h - inset - (float) psg.envelopeLevel / kMaxLevel * (h - 2.0f * inset);
    std::vector<std::pair<float, float>>  points;



    p.painter.FillRoundedRect (x, y, w, h, m_scaler.ToPxf (kRadiusDip), p.screen);

    for (const SoundBoardLayout::Point & point : SoundBoardLayout::GetEnvelopePoints (psg.envelopeRegister))
    {
        points.emplace_back (x + inset + point.x * (w - 2.0f * inset), y + h - inset - point.y * (h - 2.0f * inset));
    }

    PaintTrace (p, points, color);
    p.painter.DrawLine (x + inset, markY, x + w - inset, markY, 1.0f, WithAlpha (p.theme.Foreground(), kMarkAlpha));
    p.painter.FillCircle (x + inset + GetEnvelopeMarkX (psg) * (w - 2.0f * inset), markY, m_scaler.ToPxf (3.0f), p.theme.Foreground());

    PaintValue (p, { textLeft, rect.top, textMiddle, rect.top + line }, L"Ramp", std::format (L"{:.1f} ms", psg.envelopeRampMs));
    PaintValue (p, { textMiddle, rect.top, rect.right, rect.top + line }, L"R13", std::format (L"${:02X} ({})", psg.envelopeRegister, psg.envelopeRegister));
    PaintValue (p, { textLeft, rect.top + line, rect.right, rect.top + 2 * line }, L"Noise", SoundBoardLayout::GetFrequencyText (psg.noiseHz));

    AddSpot (p, { rect.left, rect.top, pictureRight, rect.bottom }, SoundBoardTips::GetEnvelopePicture (psg));
    AddSpot (p, { textLeft, rect.top, textMiddle, rect.top + line }, SoundBoardTips::GetRamp (psg));
    AddSpot (p, { textMiddle, rect.top, rect.right, rect.top + line }, SoundBoardTips::GetShape (psg));
    AddSpot (p, { textLeft, rect.top + line, rect.right, rect.top + 2 * line }, SoundBoardTips::GetNoise (psg));

    for (int b = 0; b < 4; b++)
    {
        bool  isSet = (psg.envelopeRegister & (kTopBit >> b)) != 0;
        int   left  = textLeft + b * m_scaler.ToPx (kBitDip);
        RECT  pill  = { left, rect.top + 2 * line + 2, left + m_scaler.ToPx (kBitDip) - 4, rect.bottom - 2 };

        p.painter.FillRoundedRect ((float) pill.left, (float) pill.top, (float) (pill.right - pill.left), (float) (pill.bottom - pill.top), m_scaler.ToPxf (kRadiusDip),
                                   isSet ? WithAlpha (color, kPillAlpha) : WithAlpha (p.theme.ForegroundMuted(), kPillAlpha / 3.0f));
        PaintString (p, kBits[b], pill, isSet ? color : p.theme.ForegroundDisabled(), p.theme.CaptionFont(), DxuiTextHAlign::Center);
        AddSpot     (p, pill, SoundBoardTips::GetShapeBit (b, psg.envelopeRegister));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetEnvelopeMarkX
//
//  Where the envelope is along its picture's four ramps, from 0 to 1: the
//  ramp it is on, from the ramps finished since R13 was written, and how far
//  along that ramp its level is. A level stopped at its end is at the
//  picture's right, where its line runs on.
//
////////////////////////////////////////////////////////////////////////////////

float SoundBoardView::GetEnvelopeMarkX (const DiagnosticsSoundPsg & psg)
{
    constexpr float  kMaxLevel = 15.0f;
    constexpr Byte   kAttack   = 0x04;
    constexpr Byte   kAlt      = 0x02;
    int              ramp      = psg.envelopeRamps % SoundBoardLayout::kEnvelopeRamps;
    bool             isRising  = (psg.envelopeRegister & kAttack) != 0;



    if (psg.isEnvelopeHeld)
    {
        return 1.0f;
    }

    //  With ALT, each ramp after the first runs the other way.
    if ((psg.envelopeRegister & kAlt) != 0 && (ramp % 2) == 1)
    {
        isRising = !isRising;
    }

    return ((float) ramp + (isRising ? (float) psg.envelopeLevel : kMaxLevel - (float) psg.envelopeLevel) / kMaxLevel) / (float) SoundBoardLayout::kEnvelopeRamps;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintVia
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintVia (const Painting & p, const DiagnosticsSoundHalf & half, const RECT & rect) const
{
    constexpr int                kColumns = 4;
    const DiagnosticsSoundVia  & via      = half.via;
    const DiagnosticsSoundPsg  & psg      = half.psg;
    int                          row      = m_scaler.ToPx (kRegisterDip);
    int                          wide     = (rect.right - rect.left) / kColumns;
    uint32_t                     lit      = m_palette.voice;
    float                        lampY    = (float) rect.top + (float) row * 4.5f;
    auto                         cell     = [&] (int y, int x, int span = 1)
    {
        return RECT { rect.left + x * wide, rect.top + y * row, rect.left + (x + span) * wide, rect.top + (y + 1) * row };
    };
    auto                         hex      = [] (int value) { return std::format (L"${:02X}", value); };
    auto                         word     = [] (int value) { return std::format (L"${:04X}", value); };
    auto                         field    = [&] (const RECT & at, const wchar_t * label, const std::wstring & value)
    {
        PaintValue (p, at, label, value);
        AddSpot    (p, at, SoundBoardTips::GetVia (label, half));
    };
    auto                         lamp     = [&] (int x, const wchar_t * label, bool isOn)
    {
        PaintLamp (p, (float) (rect.left + x * wide), lampY, label, isOn, lit, (float) wide);
        AddSpot   (p, { rect.left + x * wide, rect.top + 4 * row, rect.left + (x + 1) * wide, rect.top + 5 * row }, SoundBoardTips::GetVia (label, half));
    };



    p.painter.DrawLine ((float) rect.left, (float) rect.top - 2.0f, (float) rect.right, (float) rect.top - 2.0f, 1.0f, p.edge);

    field (cell (0, 0),    L"Bus",    TextEncoding::NarrowToWide (half.busState));
    field (cell (0, 1),    L"Writes", std::format (L"{}", psg.writeCount));
    field (cell (0, 2, 2), L"Last",   psg.hasLastWrite ? std::format (L"R{}=${:02X}", psg.lastWriteReg, psg.lastWriteValue) : std::wstring (L"--"));

    field (cell (1, 0), L"ORA",  hex (via.ora));
    field (cell (1, 1), L"ORB",  hex (via.orb));
    field (cell (1, 2), L"DDRA", hex (via.ddra));
    field (cell (1, 3), L"DDRB", hex (via.ddrb));

    field (cell (2, 0), L"ACR", hex (via.acr));
    field (cell (2, 1), L"PCR", hex (via.pcr));
    field (cell (2, 2), L"IFR", hex (via.ifr));
    field (cell (2, 3), L"IER", hex (via.ier));

    field (cell (3, 0), L"T1",    word (via.t1Count));
    field (cell (3, 1), L"Latch", word (via.t1Latch));
    field (cell (3, 2), L"T2",    word (via.t2Count));
    field (cell (3, 3), L"Rate",  (via.t1RateHz > 0.0) ? SoundBoardLayout::GetFrequencyText (via.t1RateHz) : std::wstring (L"--"));

    lamp (0, L"Run",    via.isRunning);
    lamp (1, L"Fired",  via.isFired);
    lamp (2, L"T1 IRQ", via.isT1Irq);

    PaintRegisters (p, psg, { rect.left, rect.top + 5 * row, rect.right, rect.top + 7 * row });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintRegisters
//
//  The AY's sixteen registers as written, in two rows of eight, so a value a
//  program sets can be found by its number.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintRegisters (const Painting & p, const DiagnosticsSoundPsg & psg, const RECT & rect) const
{
    constexpr int  kPerRow  = 8;
    constexpr int  kNameDip = 28;
    int            row      = (rect.bottom - rect.top) / 2;
    int            wide     = (rect.right - rect.left) / kPerRow;



    for (int r = 0; r < SoundBoardTips::kRegisterCount; r++)
    {
        RECT  at = { rect.left + (r % kPerRow) * wide, rect.top + (r / kPerRow) * row, rect.left + (r % kPerRow + 1) * wide, rect.top + (r / kPerRow + 1) * row };

        PaintValue (p, at, std::format (L"R{}", r), std::format (L"{:02X}", psg.registers[(size_t) r]), kNameDip);
        AddSpot    (p, at, SoundBoardTips::GetRegister (r, psg.registers[(size_t) r]));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintHalf
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintHalf (const Painting & p, const DiagnosticsSoundHalf & half, const RECT & card) const
{
    constexpr int     kChannels = DiagnosticsSoundPsg::kChannelCount;
    constexpr int     kLampDip  = 56;
    RECT              envelope  = GetEnvelopeRect (card);
    int               pad       = m_scaler.ToPx (kPadDip);
    float             lampX     = (float) (card.right - pad - m_scaler.ToPx (kLampDip));
    float             lampY     = (float) card.top + (float) pad + m_scaler.ToPxf (kTitleDip / 2.0f);



    PaintCard (p, card, std::format (L"AY #{}", half.index + 1), std::format (L"6522 #{}, slot {}, ${:04X}", half.index + 1, m_board.slot, half.baseAddress));
    PaintLamp (p, lampX, lampY, L"IRQ", half.via.isIrq, p.theme.ErrorForeground(), (float) m_scaler.ToPx (kLampDip));
    AddSpot   (p, { (long) lampX, card.top + pad, card.right - pad, card.top + pad + m_scaler.ToPx (kTitleDip) }, SoundBoardTips::GetVia (L"IRQ", half));

    for (int c = 0; c < kChannels; c++)
    {
        PaintChannel (p, half.psg, GetChannelRect (card, c), c, half.index * kChannels + c);
    }

    PaintEnvelope (p, half.psg, envelope);
    PaintVia (p, half, { envelope.left, envelope.bottom + m_scaler.ToPx (kGapDip), envelope.right, card.bottom - pad });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintStrip
//
//  The newest phoneme is brightest while it sounds, and each older one fainter.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintStrip (const Painting & p, std::span<const DiagnosticsSoundVoice::Phoneme> recent, bool isSounding, const RECT & rect, size_t count) const
{
    constexpr float            kRadiusDip     = 4.0f;
    constexpr float            kGapDip2       = 3.0f;
    constexpr float            kLabelPadDip   = 8.0f;
    constexpr float            kLeastBlockDip = 3.0f;
    constexpr float            kOldest        = 0.10f;
    constexpr float            kNewest        = 0.45f;
    const DxuiFontHandle       font           = p.theme.CaptionFont();
    std::vector<float>         seconds;
    std::vector<float>         least;
    std::vector<float>         labels;
    std::vector<std::wstring>  names;
    size_t                     first          = recent.size() - std::min (count, recent.size());



    //  The phonemes the history row has labels for, so the two cover the same
    //  stretch of speech.
    for (size_t i = first; i < recent.size(); i++)
    {
        const DiagnosticsSoundVoice::Phoneme  & phoneme = recent[i];
        std::wstring                            name    = TextEncoding::NarrowToWide (phoneme.name.substr (0, phoneme.name.find (' ')));
        float                                   wide    = 0.0f;
        float                                   high    = 0.0f;
        HRESULT                                 hr      = p.text.MeasureString (name.c_str(), m_scaler.ToPxf (font.sizeDip), font.face, wide, high);

        IGNORE_RETURN_VALUE (hr, S_OK);

        //  A block can be narrow enough for every phoneme of the history row to
        //  have one; its label is drawn only where it fits.
        seconds.push_back (phoneme.seconds);
        least.push_back (m_scaler.ToPxf (kLeastBlockDip));
        labels.push_back (wide + m_scaler.ToPxf (kLabelPadDip));
        names.push_back (std::move (name));
    }

    p.painter.FillRoundedRect ((float) rect.left, (float) rect.top, (float) (rect.right - rect.left), (float) (rect.bottom - rect.top), m_scaler.ToPxf (kRadiusDip), p.screen);

    auto  placed = SoundBoardLayout::FillStrip (seconds, least, (float) (rect.right - rect.left), m_scaler.ToPxf (kGapDip2));

    for (size_t i = 0; i < placed.size(); i++)
    {
        const SoundBoardLayout::StripPlace  & place  = placed[i];
        bool                                  isLast = i + 1 == placed.size();
        float                                 age    = placed.size() > 1 ? (float) i / (float) (placed.size() - 1) : 1.0f;
        float                                 alpha  = kOldest + (kNewest - kOldest) * age;
        float                                 x      = (float) rect.left + place.x;
        uint32_t                              fill   = (isLast && isSounding) ? m_palette.voice : WithAlpha (m_palette.voice, alpha);
        uint32_t                              ink    = (isLast && isSounding) ? p.theme.Background() : p.theme.Foreground();

        p.painter.FillRoundedRect (x, (float) rect.top, place.width, (float) (rect.bottom - rect.top), m_scaler.ToPxf (kRadiusDip), fill);
        AddSpot (p, { (long) x, rect.top, (long) (x + place.width) + 1, rect.bottom }, SoundBoardTips::GetPhoneme (recent[first + place.index]));

        if (place.width >= labels[place.index])
        {
            PaintString (p, names[place.index], { (long) x, rect.top, (long) (x + place.width), rect.bottom }, ink, font, DxuiTextHAlign::Center);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintHistory
//
//  The recent phonemes' labels in order along a row, newest at the right, as
//  many as fit, with an arrow under the newest while it sounds, so a
//  consonant can be read after it has passed. Returns how many are drawn.
//
////////////////////////////////////////////////////////////////////////////////

size_t SoundBoardView::PaintHistory (const Painting & p, std::span<const DiagnosticsSoundVoice::Phoneme> recent, bool isSounding, const RECT & rect) const
{
    constexpr float            kGapDip2   = 10.0f;
    constexpr float            kArrowDip  = 5.0f;
    const DxuiFontHandle       font       = p.theme.MonospaceFont();
    float                      gap        = m_scaler.ToPxf (kGapDip2);
    float                      arrow      = m_scaler.ToPxf (kArrowDip);
    float                      textBottom = (float) rect.bottom - arrow * 2.0f;
    std::vector<std::wstring>  names;
    std::vector<float>         widths;
    size_t                     count      = 0;
    float                      x          = (float) rect.right;



    for (const DiagnosticsSoundVoice::Phoneme & phoneme : recent)
    {
        std::wstring  name = TextEncoding::NarrowToWide (phoneme.name.substr (0, phoneme.name.find (' ')));
        float         wide = 0.0f;
        float         high = 0.0f;
        HRESULT       hr   = p.text.MeasureString (name.c_str(), m_scaler.ToPxf (font.sizeDip), font.face, wide, high);

        IGNORE_RETURN_VALUE (hr, S_OK);

        names.push_back (std::move (name));
        widths.push_back (wide);
    }

    count = SoundBoardLayout::FitHistory (widths, (float) (rect.right - rect.left), gap);

    for (size_t i = names.size(); i > names.size() - count; i--)
    {
        bool      isActive = i == names.size() && isSounding;
        uint32_t  ink      = isActive ? m_palette.voice : p.theme.Foreground();
        float     left     = x - widths[i - 1];

        PaintString (p, names[i - 1], { (long) left, rect.top, (long) x + 1, (long) textBottom }, ink, font, DxuiTextHAlign::Left);
        AddSpot     (p, { (long) left, rect.top, (long) x + 1, rect.bottom }, SoundBoardTips::GetPhoneme (recent[i - 1]));

        if (isActive)
        {
            float  mid = left + widths[i - 1] * 0.5f;

            p.painter.FillConvexQuad (mid,         textBottom,
                                      mid + arrow, textBottom + arrow * 1.5f,
                                      mid - arrow, textBottom + arrow * 1.5f,
                                      mid,         textBottom,
                                      m_palette.voice);
        }

        x = left - gap;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetStripArea
//
//  The strip's heading, the strip and the history row under it, as
//  PaintVoice places them.
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundBoardView::GetStripArea (const RECT & card) const
{
    int  pad    = m_scaler.ToPx (kPadDip);
    int  top    = card.top + pad + m_scaler.ToPx (kTitleDip) + m_scaler.ToPx (kPhonemeDip) + m_scaler.ToPx (kGapDip);



    return RECT { card.left + pad, top, card.right - pad, top + m_scaler.ToPx (kStripHeadDip + kStripDip + kHistoryDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintFormants
//
//  The tract's response as a glowing curve over a filled area, on a logarithmic
//  axis, with the pitch's harmonics rising under it as a comb. A voiced vowel
//  reads as the comb under the three peaks.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintFormants (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & rect) const
{
    static constexpr double               kAxisHz[]  = { 200.0, 500.0, 1000.0, 2000.0, 5000.0 };
    constexpr float                       kRadiusDip = 4.0f;
    constexpr float                       kInsetDip  = 6.0f;
    constexpr int                         kLabelDip  = 16;
    constexpr int                         kColumnDip = 3;
    constexpr float                       kTopAlpha  = 0.45f;
    constexpr float                       kGridAlpha = 0.12f;
    constexpr float                       kCombAlpha = 0.55f;
    constexpr double                      kCombLeast = 60.0;
    float                                 x          = (float) rect.left;
    float                                 y          = (float) rect.top;
    float                                 w          = (float) (rect.right - rect.left);
    float                                 h          = (float) (rect.bottom - rect.top);
    float                                 inset      = m_scaler.ToPxf (kInsetDip);
    float                                 label      = (float) m_scaler.ToPx (kLabelDip);
    float                                 plotW      = w - 2.0f * inset;
    float                                 plotH      = h - inset - label;
    float                                 base       = y + inset + plotH;
    int                                   columns    = std::max (2, (int) plotW / m_scaler.ToPx (kColumnDip));
    float                                 step       = plotW / (float) (columns - 1);
    auto                                  curve      = SoundBoardLayout::GetFormantCurve (voice.formantsHz, columns);
    uint32_t                              color      = m_palette.voice;
    float                                 fade       = voice.isSounding ? 1.0f : 0.45f;
    std::vector<std::pair<float, float>>  points;



    p.painter.FillRoundedRect (x, y, w, h, m_scaler.ToPxf (kRadiusDip), p.screen);

    for (double hz : kAxisHz)
    {
        float  ax = x + inset + SoundBoardLayout::GetFormantX (hz) * plotW;

        p.painter.DrawLine (ax, y + inset, ax, base, 1.0f, WithAlpha (p.theme.ForegroundMuted(), kGridAlpha));
        PaintString (p, (hz >= 1000.0) ? std::format (L"{:.0f}k", hz / 1000.0) : std::format (L"{:.0f}", hz),
                     { (long) ax - m_scaler.ToPx (kLabelDip), (long) base, (long) ax + m_scaler.ToPx (kLabelDip), (long) (y + h) },
                     p.theme.ForegroundMuted(), p.theme.CaptionFont(), DxuiTextHAlign::Center);
    }

    for (int c = 0; c + 1 < columns; c++)
    {
        float  top = base - curve[(size_t) c] * plotH;

        p.painter.FillGradientRect (x + inset + (float) c * step, top, step, base - top, WithAlpha (color, kTopAlpha * fade), WithAlpha (color, 0.02f));
    }

    if (voice.pitchHz >= kCombLeast && voice.isSounding)
    {
        for (double hz = voice.pitchHz; hz < SoundBoardLayout::kFormantMaxHz; hz += voice.pitchHz)
        {
            float   cx    = SoundBoardLayout::GetFormantX (hz);
            size_t  index = std::min ((size_t) (cx * (float) (columns - 1)), curve.size() - 1);
            float   gx    = x + inset + cx * plotW;

            p.painter.DrawLine (gx, base, gx, base - curve[index] * plotH, 1.0f, WithAlpha (p.theme.Foreground(), kCombAlpha));
        }
    }

    for (int c = 0; c < columns; c++)
    {
        points.emplace_back (x + inset + (float) c * step, base - curve[(size_t) c] * plotH);
    }

    PaintTrace (p, points, voice.isSounding ? color : WithAlpha (color, fade));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::PaintVoice
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::PaintVoice (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & card) const
{
    constexpr int                                    kLeftDip     = 190;
    constexpr int                                    kLampWideDip = 86;
    constexpr int                                    kLampsDip    = 3 * kLampWideDip + 36;
    constexpr int                                    kBigDip      = 58;
    constexpr int                                    kWordDip     = 22;
    constexpr int                                    kCodeDip     = 20;
    constexpr int                                    kColumns     = 4;
    constexpr int                                    kSegments    = 15;
    constexpr float                                  kBigScale    = 2.6f;
    constexpr int                                    kFirstExtra  = 3;
    constexpr float                                  kUnlitAlpha  = 0.18f;
    int                                              pad          = m_scaler.ToPx (kPadDip);
    int                                              gap          = m_scaler.ToPx (kGapDip);
    int                                              top          = card.top + pad + m_scaler.ToPx (kTitleDip);
    int                                              mainBottom   = top + m_scaler.ToPx (kPhonemeDip);
    int                                              stripTop     = mainBottom + gap + m_scaler.ToPx (kStripHeadDip);
    int                                              captionHigh  = m_scaler.ToPx (kStripHeadDip);
    int                                              stripBottom  = stripTop + m_scaler.ToPx (kStripDip);
    int                                              historyEnd   = stripBottom + m_scaler.ToPx (kHistoryDip);
    size_t                                           shown        = 0;
    size_t                                           back         = 0;
    std::span<const DiagnosticsSoundVoice::Phoneme>  older;
    int                                              left         = card.left + pad;
    int                                              right        = card.right - pad;
    int                                              inner        = right - left;
    int                                              leftWide     = std::min (m_scaler.ToPx (kLeftDip), inner / 3);
    int                                              scopeLeft    = left + leftWide + gap;
    int                                              rest         = right - scopeLeft;
    int                                              scopeRight   = scopeLeft + (rest - gap) / 2;
    int                                              rowHigh      = m_scaler.ToPx (kRegisterDip);
    int                                              wide         = inner / kColumns;
    int                                              valuesTop    = historyEnd + gap;
    RECT                                             mute         = GetMuteOfVoice (card);
    size_t                                           space        = voice.phoneme.find (' ');
    std::wstring                                     name         = TextEncoding::NarrowToWide (voice.phoneme.substr (0, space));
    std::wstring                                     word         = (space == std::string::npos) ? std::wstring() : TextEncoding::NarrowToWide (voice.phoneme.substr (space + 1));
    DxuiFontHandle                                   big          = p.theme.HeadingFont();
    uint32_t                                         color        = voice.isMuted ? p.theme.ForegroundMuted() : m_palette.voice;
    float                                            lampY        = (float) card.top + (float) pad + m_scaler.ToPxf (kTitleDip / 2.0f) - m_scaler.ToPxf (2.0f);
    float                                            lampX        = (float) (mute.left - m_scaler.ToPx (kLampsDip));
    float                                            segmentWide  = (float) leftWide / (float) kSegments;
    float                                            meterTop     = (float) (top + m_scaler.ToPx (kBigDip + kWordDip + kCodeDip)) + m_scaler.ToPxf (4.0f);
    float                                            meterHigh    = m_scaler.ToPxf (14.0f);
    float                                            radius       = m_scaler.ToPxf (4.0f);
    int                                              litSegments  = (int) std::lround (voice.level * kSegments);
    int                                              firstWide    = wide + wide / kFirstExtra;
    int                                              otherWide    = (inner - firstWide) / (kColumns - 1);
    auto              cell         = [&] (int y, int x)
    {
        int  from = (x == 0) ? left : left + firstWide + (x - 1) * otherWide;
        int  to   = (x == 0) ? left + firstWide : from + otherWide;

        return RECT { from, valuesTop + y * rowHigh, to, valuesTop + (y + 1) * rowHigh };
    };



    big.sizeDip *= kBigScale;

    PaintCard (p, card, L"SSI-263", std::format (L"Speech chip, slot {}", m_board.slot));

    PaintLamp (p, lampX,                                         lampY, L"Power",      !voice.isPoweredDown, m_palette.voice, (float) m_scaler.ToPx (kLampWideDip));
    PaintLamp (p, lampX + (float) m_scaler.ToPx (kLampWideDip),  lampY, L"Sounding",   voice.isSounding,     m_palette.voice, (float) m_scaler.ToPx (kLampWideDip));
    PaintLamp (p, lampX + (float) m_scaler.ToPx (2 * kLampWideDip), lampY, L"Request", voice.isRequesting,   m_palette.voice, (float) m_scaler.ToPx (kLampWideDip));

    p.painter.FillRoundedRect ((float) mute.left, (float) mute.top, (float) (mute.right - mute.left), (float) (mute.bottom - mute.top), radius,
                               WithAlpha (voice.isMuted ? p.theme.ErrorForeground() : m_palette.voice, 0.16f));
    PaintString (p, L"M", mute, voice.isMuted ? p.theme.ErrorForeground() : m_palette.voice, p.theme.BodyBoldFont(), DxuiTextHAlign::Center);

    if (voice.isMuted)
    {
        p.painter.DrawLine ((float) mute.left + radius, (float) mute.bottom - radius, (float) mute.right - radius, (float) mute.top + radius, m_scaler.ToPxf (1.5f), p.theme.ErrorForeground());
    }

    PaintString (p, name, { left, top, left + leftWide, top + m_scaler.ToPx (kBigDip) }, voice.isSounding ? color : p.theme.ForegroundMuted(), big);
    PaintString (p, word, { left, top + m_scaler.ToPx (kBigDip), left + leftWide, top + m_scaler.ToPx (kBigDip + kWordDip) }, p.theme.Foreground(), p.theme.BodyFont());
    PaintString (p, std::format (L"${:02X}", voice.phonemeCode),
                 { left, top + m_scaler.ToPx (kBigDip + kWordDip), left + leftWide, top + m_scaler.ToPx (kBigDip + kWordDip + kCodeDip) }, p.theme.ForegroundMuted(), p.theme.MonospaceFont());

    for (int s = 0; s < kSegments; s++)
    {
        bool  isLit = s < litSegments;

        p.painter.FillRect ((float) left + (float) s * segmentWide, meterTop, segmentWide - m_scaler.ToPxf (1.5f), meterHigh,
                            isLit ? Mix (color, 0xFFFFFFFF, (float) s / (float) kSegments * 0.35f) : WithAlpha (p.theme.ForegroundMuted(), kUnlitAlpha));
    }

    //  Each graph under a caption.
    PaintString   (p, L"Waveform", { scopeLeft, top, scopeRight, top + captionHigh }, p.theme.ForegroundMuted(), p.theme.CaptionFont());
    PaintString   (p, L"Formants", { scopeRight + gap, top, right, top + captionHigh }, p.theme.ForegroundMuted(), p.theme.CaptionFont());
    PaintScope    (p, voice.scope, { scopeLeft, top + captionHigh, scopeRight, mainBottom }, color, 0.0, true);
    PaintFormants (p, voice, { scopeRight + gap, top + captionHigh, right, mainBottom });
    //  The wheel over the strip scrolls it back through the history, and the
    //  newest phoneme there isn't sounding.
    back  = std::min (m_phonemesBack, voice.recent.size() > 0 ? voice.recent.size() - 1 : 0);
    older = std::span<const DiagnosticsSoundVoice::Phoneme> (voice.recent).first (voice.recent.size() - back);

    PaintString (p, (back == 0) ? std::wstring (L"Recent phonemes") : std::format (L"Recent phonemes, {} back", back),
                 { left, mainBottom + gap, right, stripTop }, p.theme.ForegroundMuted(), p.theme.CaptionFont());

    shown = PaintHistory (p, older, voice.isSounding && back == 0, { left, stripBottom, right, historyEnd });
    PaintStrip (p, older, voice.isSounding && back == 0, { left, stripTop, right, stripBottom }, shown);

    PaintValue (p, cell (0, 0), L"Mode",         TextEncoding::NarrowToWide (voice.mode), kVoiceLabelDip);
    PaintValue (p, cell (0, 1), L"Duration",     std::format (L"{:.1f} ms", voice.durationMs), kVoiceLabelDip);
    PaintValue (p, cell (0, 2), L"Frame",        std::format (L"{:.1f} ms", voice.frameMs), kVoiceLabelDip);
    PaintValue (p, cell (0, 3), L"Pitch",        SoundBoardLayout::GetFrequencyText (voice.pitchHz), kVoiceLabelDip);
    PaintValue (p, cell (1, 0), L"Formants",     std::format (L"{:.0f}, {:.0f}, {:.0f}", voice.formantsHz[0], voice.formantsHz[1], voice.formantsHz[2]), kVoiceLabelDip);
    PaintValue (p, cell (1, 1), L"Articulation", std::format (L"${:X}", voice.articulation), kVoiceLabelDip);
    PaintValue (p, cell (1, 2), L"Amplitude",    std::format (L"${:X}", voice.amplitude), kVoiceLabelDip);
    PaintValue (p, cell (1, 3), L"Filter",       SoundBoardLayout::GetFrequencyText (voice.filterHz), kVoiceLabelDip);

    //  What each part means, at the place it is drawn.
    for (const auto & [at, label] : std::initializer_list<std::pair<RECT, const wchar_t *>>
         {
             { cell (0, 0), L"Mode" },     { cell (0, 1), L"Duration" },     { cell (0, 2), L"Frame" },     { cell (0, 3), L"Pitch" },
             { cell (1, 0), L"Formants" }, { cell (1, 1), L"Articulation" }, { cell (1, 2), L"Amplitude" }, { cell (1, 3), L"Filter" },
             { RECT { left, top, left + leftWide, (long) meterTop }, L"Phoneme" },
             { RECT { left, (long) meterTop, left + leftWide, mainBottom }, L"Level" },
             { RECT { scopeLeft, top, scopeRight, mainBottom }, L"Waveform" },
             { RECT { scopeRight + gap, top, right, mainBottom }, L"Formants" },
             { RECT { left, mainBottom + gap, right, stripTop }, L"Recent phonemes" },
             { RECT { (long) lampX, card.top + pad, (long) lampX + m_scaler.ToPx (kLampWideDip), top }, L"Power" },
             { RECT { (long) lampX + m_scaler.ToPx (kLampWideDip), card.top + pad, (long) lampX + m_scaler.ToPx (2 * kLampWideDip), top }, L"Sounding" },
             { RECT { (long) lampX + m_scaler.ToPx (2 * kLampWideDip), card.top + pad, (long) lampX + m_scaler.ToPx (3 * kLampWideDip), top }, L"Request" },
         })
    {
        AddSpot (p, at, SoundBoardTips::GetVoice (label, voice));
    }
}
