#include "Pch.h"

#include "Ui/Debugger/Panes/SoundBoardView.h"
#include "Core/TextEncoding.h"
#include "Devices/Ay8910.h"
#include "Devices/Ssi263.h"
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
    int               voice   = 0;
    int               columns = 1;
    int               below   = 0;
    std::vector<int>  heights (m_board.halves.size(), GetHalfHeight());



    for (bool hasBar : { false, true })
    {
        m_hasScrollbar = hasBar;
        voice          = m_board.voice.has_value() ? m_voice.GetHeight (GetValueColumns()) + gap : 0;
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

    strip = ToClient (m_voice.GetStripArea (m_voiceCard));
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

        AddRegion (row, std::format (L"The tone period is ${:04X} (R{} and R{}), the mixer ${:02X} (R7) and the amplitude {}{} (R{}).",
                                     channel.period, fine, fine + 1, half.psg.mixer, channel.level,
                                     channel.usesEnvelope ? L", from the envelope" : L"", level));
    }

    AddRegion (envelope, std::format (L"The envelope period is ${:04X} (R11 and R12), its pattern ${:02X} (R13), and the noise period ${:02X} (R6).",
                                      half.psg.envelopePeriod, half.psg.envelopeRegister, half.psg.noisePeriod));

    AddRegion ({ card.left, card.top, card.right, card.top + pad + m_scaler.ToPx (kTitleDip) },
               std::format (L"The 6522's interrupt flags are ${:02X} and its interrupt enable ${:02X}.", half.via.ifr, half.via.ier));

    AddRegion ({ envelope.left, envelope.bottom, envelope.right, card.bottom },
               L"The 6522 that drives this AY: its ports, its timers and the AY's bus, then the AY's sixteen registers as written.");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::BuildVoiceRegions
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::BuildVoiceRegions (const DiagnosticsSoundVoice & voice, const RECT & card)
{
    AddRegion (m_voice.GetMuteRect (card), voice.isMuted ? L"Unmute the speech chip" : L"Mute the speech chip", kVoiceMuteChannel);

    AddRegion (m_voice.GetStripArea (card), SoundBoardTips::GetVoice (L"Recent phonemes", voice));

    AddRegion (card, std::format (L"The registers' fields: phoneme ${:02X}, duration ${:X}, rate ${:X}, inflection ${:03X}, articulation ${:X}, amplitude ${:X} and filter ${:02X}.",
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
        m_hoverMute = isOver ? FindMuteAt (at) : -1;
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Leave)
    {
        m_hoverMute = -1;
        return false;
    }

    if (!isOver)
    {
        return false;
    }

    //  Over the phoneme strip, the wheel tilted or turned with Shift moves
    //  through the phonemes, as either moves a view sideways; the plain wheel
    //  scrolls the board. The oldest phonemes are at the left, so a tilt to
    //  the left, or Shift with the wheel up, goes back to them.
    if (ev.kind == DxuiMouseEventKind::Wheel && (ev.wheelHorizontal || ev.shift) && IsOverPhonemeStrip (at))
    {
        ScrollPhonemes ((int) std::lround ((ev.wheelHorizontal ? -ev.wheelDelta : ev.wheelDelta) * (float) kPhonemesPerNotch));
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

    if (int channel = FindMuteAt (at); channel >= 0)
    {
        std::vector<int>  mutes = GetMuteChannels();
        auto              found = std::ranges::find (mutes, channel);

        m_focusMute = (int) (found - mutes.begin());
        ToggleMute (channel);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::FindMuteAt
//
//  The channel of the mute under a point, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SoundBoardView::FindMuteAt (POINT clientPx) const
{
    for (const Region & region : m_regions)
    {
        RECT  placed = ToClient (region.rect);

        if (region.muteChannel >= 0 && PtInRect (&placed, clientPx))
        {
            return region.muteChannel;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetMuteChannels
//
//  The mutes in Tab order: the speech chip's, then each AY's channels.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> SoundBoardView::GetMuteChannels() const
{
    std::vector<int>  channels;



    for (const Region & region : m_regions)
    {
        if (region.muteChannel >= 0)
        {
            channels.push_back (region.muteChannel);
        }
    }

    return channels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::IsChannelMuted
//
////////////////////////////////////////////////////////////////////////////////

bool SoundBoardView::IsChannelMuted (int channel) const
{
    size_t  half  = (size_t) channel / DiagnosticsSoundPsg::kChannelCount;
    size_t  index = (size_t) channel % DiagnosticsSoundPsg::kChannelCount;



    if (channel == kVoiceMuteChannel)
    {
        return m_board.voice.has_value() && m_board.voice->isMuted;
    }

    return half < m_board.halves.size() && m_board.halves[half].psg.channels[index].isMuted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::ToggleMute
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::ToggleMute (int channel)
{
    if (m_onMute)
    {
        m_onMute (channel, !IsChannelMuted (channel));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetFocusedMute
//
//  The channel of the mute with the keyboard focus, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SoundBoardView::GetFocusedMute() const
{
    std::vector<int>  mutes = GetMuteChannels();



    return (m_isFocused && m_focusMute >= 0 && m_focusMute < (int) mutes.size()) ? mutes[(size_t) m_focusMute] : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::OnFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::OnFocusChanged (bool focused)
{
    m_isFocused = focused;

    if (focused && (m_focusMute < 0 || m_focusMute >= (int) GetMuteChannels().size()))
    {
        m_focusMute = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::OnFocusEntered
//
//  Tab brings the focus to the first mute, and Shift+Tab to the last.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::OnFocusEntered (bool forward)
{
    int  count = (int) GetMuteChannels().size();



    m_focusMute = (forward || count == 0) ? 0 : count - 1;
    RevealFocusedMute();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::OnKey
//
//  Each mute is a button in the Tab order. Tab and Shift+Tab move between
//  them and on to the next control after the last; the arrows move between
//  them and stop at the ends; Space and Enter press the one focused.
//
////////////////////////////////////////////////////////////////////////////////

bool SoundBoardView::OnKey (const DxuiKeyEvent & ev)
{
    int   count = (int) GetMuteChannels().size();
    int   step  = 0;



    if (ev.kind != DxuiKeyEventKind::Down || count == 0 || ev.ctrl || ev.alt)
    {
        return false;
    }

    m_focusMute = std::clamp (m_focusMute, 0, count - 1);

    switch (ev.vk)
    {
    case VK_SPACE:
    case VK_RETURN:
        ToggleMute (GetMuteChannels()[(size_t) m_focusMute]);
        return true;

    case VK_TAB:
        step = ev.shift ? -1 : 1;

        if (m_focusMute + step < 0 || m_focusMute + step >= count)
        {
            return false;
        }

        break;

    case VK_LEFT:
    case VK_UP:
        step = -1;
        break;

    case VK_RIGHT:
    case VK_DOWN:
        step = 1;
        break;

    default:
        return false;
    }

    m_focusMute = std::clamp (m_focusMute + step, 0, count - 1);
    RevealFocusedMute();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::RevealFocusedMute
//
//  Scrolls the view just far enough that the mute the keyboard focused is
//  in view.
//
////////////////////////////////////////////////////////////////////////////////

void SoundBoardView::RevealFocusedMute()
{
    std::vector<int>  mutes = GetMuteChannels();
    int               gap   = m_scaler.ToPx (kGapDip);
    int               page  = (int) (m_boundsDip.bottom - m_boundsDip.top) - 2 * gap;



    for (const Region & region : m_regions)
    {
        if (m_focusMute < 0 || m_focusMute >= (int) mutes.size() || region.muteChannel != mutes[(size_t) m_focusMute])
        {
            continue;
        }

        if (region.rect.top < m_scrollTop)
        {
            ScrollTo (region.rect.top);
        }
        else if (region.rect.bottom > m_scrollTop + page)
        {
            ScrollTo (region.rect.bottom - page);
        }

        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetAccessibleName
//
//  The view, and the mute the keyboard focused in it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardView::GetAccessibleName() const
{
    int  focused = GetFocusedMute();



    for (const Region & region : m_regions)
    {
        if (focused >= 0 && region.muteChannel == focused)
        {
            return (focused == kVoiceMuteChannel) ? std::format (L"Mockingboard, {} button", region.tip)
                                                  : std::format (L"Mockingboard, {} of AY #{} button", region.tip, focused / DiagnosticsSoundPsg::kChannelCount + 1);
        }
    }

    return L"Mockingboard";
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
                                       SoundBoardInk::Mix (background, kBlack, DebuggerTextColors::IsDark (background) ? kScreenDark : kScreenLight),
                                       SoundBoardInk::Mix (theme.ContentBackground(), theme.Foreground(), kCardLift),
                                       theme.ContentEdge(), m_spots, m_board.sampleRate, m_hoverMute, GetFocusedMute() };



    m_spots.clear();

    if (!m_visible)
    {
        return;
    }

    painter.PushClip ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top));

    if (m_board.voice.has_value())
    {
        m_voice.Paint (p, *m_board.voice, ToClient (m_voiceCard), GetValueColumns());
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
    uint32_t                         color        = channel.isMuted ? SoundBoardInk::Mix (m_palette.channels[index], p.theme.ForegroundMuted(), kMuteMix) : m_palette.channels[index];
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

    m_ink.PaintMuteButton (p, mute, muteChannel, channel.isMuted, color);
    m_ink.PaintString (p, kNames[index], mute, channel.isMuted ? p.theme.ErrorForeground() : color, p.theme.BodyBoldFont(), DxuiTextHAlign::Center);

    if (channel.isMuted)
    {
        p.painter.DrawLine ((float) mute.left + radius, (float) mute.bottom - radius, (float) mute.right - radius, (float) mute.top + radius, m_scaler.ToPxf (1.5f), p.theme.ErrorForeground());
    }

    if (isSilent)
    {
        m_ink.PaintString (p, L"Silent", { noteLeft, row.top, noteRight, row.bottom }, p.theme.ForegroundDisabled(), p.theme.BodyFont());
    }
    else
    {
        m_ink.PaintString (p, note, { noteLeft, middle - m_scaler.ToPx (kNoteRowDip), noteRight, middle }, p.theme.Foreground(), big);
        m_ink.PaintString (p, SoundBoardLayout::GetFrequencyText (channel.frequencyHz), { noteLeft, middle, noteRight, middle + m_scaler.ToPx (kNoteRowDip) }, p.theme.ForegroundMuted(), p.theme.CaptionFont());
    }

    SoundBoardInk::AddSpot (p, { noteLeft, row.top, noteRight, row.bottom }, SoundBoardTips::GetNote (index, channel));
    SoundBoardInk::AddSpot (p, { meterLeft, row.top, meterRight, row.bottom }, SoundBoardTips::GetLevel (index, channel, amplitude));
    SoundBoardInk::AddSpot (p, { meterRight + gap, row.top, row.right, row.bottom }, SoundBoardTips::GetScope (index));

    for (int s = 0; s < 2; s++)
    {
        bool  isOn = (s == 0) ? channel.isToneOn : channel.isNoiseOn;
        RECT  pill = { sourceLeft, middle - source + s * (source + 2), sourceRight, middle + s * (source + 2) };

        SoundBoardInk::AddSpot (p, pill, SoundBoardTips::GetSource (index, s == 1, channel, psg.mixer));

        p.painter.FillRoundedRect ((float) pill.left, (float) pill.top, (float) source, (float) source, radius, isOn ? SoundBoardInk::WithAlpha (color, kPillAlpha * 2.0f) : SoundBoardInk::WithAlpha (p.theme.ForegroundMuted(), kPillAlpha / 2.0f));
        m_ink.PaintString (p, (s == 0) ? L"T" : L"N", pill, isOn ? color : p.theme.ForegroundDisabled(), p.theme.CaptionFont(), DxuiTextHAlign::Center);
    }

    m_ink.PaintString (p, channel.usesEnvelope ? std::format (L"{:>2} env", channel.level) : std::format (L"{:>2}", channel.level),
                 { meterLeft, middle - m_scaler.ToPx (kNoteRowDip), meterRight, middle }, p.theme.Foreground(), p.theme.MonospaceFont());

    for (int s = 0; s < kSegments; s++)
    {
        float  sx     = (float) meterLeft + (float) s * segmentWide;
        bool   isLit  = s < channel.level;
        bool   isPeak = !isLit && (int) peak == s + 1 && !channel.isMuted;
        uint32_t  fill  = isLit ? SoundBoardInk::Mix (color, 0xFFFFFFFF, (float) s / (float) kSegments * 0.35f)
                                : (isPeak ? SoundBoardInk::WithAlpha (color, kPeakDim) : SoundBoardInk::WithAlpha (p.theme.ForegroundMuted(), kUnlitAlpha));

        p.painter.FillRect (sx, meterTop, segmentWide - m_scaler.ToPxf (1.5f), meterHigh, fill);
    }

    m_ink.PaintScope (p, channel.scope, { meterRight + gap, row.top, row.right, row.bottom }, isSilent ? SoundBoardInk::WithAlpha (color, kSilentAlpha) : color, channel.isToneOn && !channel.isNoiseOn ? channel.frequencyHz : 0.0, false);
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

    m_ink.PaintTrace (p, points, color);
    p.painter.DrawLine (x + inset, markY, x + w - inset, markY, 1.0f, SoundBoardInk::WithAlpha (p.theme.Foreground(), kMarkAlpha));
    p.painter.FillCircle (x + inset + GetEnvelopeMarkX (psg) * (w - 2.0f * inset), markY, m_scaler.ToPxf (3.0f), p.theme.Foreground());

    m_ink.PaintValue (p, { textLeft, rect.top, textMiddle, rect.top + line }, L"Ramp", SoundBoardLayout::GetMillisecondsText (psg.envelopeRampMs));
    m_ink.PaintValue (p, { textMiddle, rect.top, rect.right, rect.top + line }, L"R13", std::format (L"${:02X} ({})", psg.envelopeRegister, psg.envelopeRegister));
    m_ink.PaintValue (p, { textLeft, rect.top + line, rect.right, rect.top + 2 * line }, L"Noise", SoundBoardLayout::GetFrequencyText (psg.noiseHz));

    SoundBoardInk::AddSpot (p, { rect.left, rect.top, pictureRight, rect.bottom }, SoundBoardTips::GetEnvelopePicture (psg));
    SoundBoardInk::AddSpot (p, { textLeft, rect.top, textMiddle, rect.top + line }, SoundBoardTips::GetRamp (psg));
    SoundBoardInk::AddSpot (p, { textMiddle, rect.top, rect.right, rect.top + line }, SoundBoardTips::GetShape (psg));
    SoundBoardInk::AddSpot (p, { textLeft, rect.top + line, rect.right, rect.top + 2 * line }, SoundBoardTips::GetNoise (psg));

    for (int b = 0; b < 4; b++)
    {
        bool  isSet = (psg.envelopeRegister & (kTopBit >> b)) != 0;
        int   left  = textLeft + b * m_scaler.ToPx (kBitDip);
        RECT  pill  = { left, rect.top + 2 * line + 2, left + m_scaler.ToPx (kBitDip) - 4, rect.bottom - 2 };

        p.painter.FillRoundedRect ((float) pill.left, (float) pill.top, (float) (pill.right - pill.left), (float) (pill.bottom - pill.top), m_scaler.ToPxf (kRadiusDip),
                                   isSet ? SoundBoardInk::WithAlpha (color, kPillAlpha) : SoundBoardInk::WithAlpha (p.theme.ForegroundMuted(), kPillAlpha / 3.0f));
        m_ink.PaintString (p, kBits[b], pill, isSet ? color : p.theme.ForegroundDisabled(), p.theme.CaptionFont(), DxuiTextHAlign::Center);
        SoundBoardInk::AddSpot     (p, pill, SoundBoardTips::GetShapeBit (b, psg.envelopeRegister));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardView::GetEnvelopeMarkX
//
//  Where the envelope is along its picture's four ramps, from 0 to 1: the
//  ramp it is on, from the ramps finished since R13 was written, and how far
//  along that ramp its level is. A level stopped at its end moves on along
//  the flat line at the same pace, one ramp for every 16 steps, and stays at
//  the picture's right once it gets there.
//
////////////////////////////////////////////////////////////////////////////////

float SoundBoardView::GetEnvelopeMarkX (const DiagnosticsSoundPsg & psg)
{
    constexpr float  kMaxLevel     = 15.0f;
    constexpr float  kStepsPerRamp = 16.0f;
    constexpr Byte   kAttack       = 0x04;
    constexpr Byte   kAlt          = 0x02;
    float            ramps         = (float) SoundBoardLayout::kEnvelopeRamps;
    int              ramp          = psg.envelopeRamps % SoundBoardLayout::kEnvelopeRamps;
    bool             isRising      = (psg.envelopeRegister & kAttack) != 0;



    if (psg.isEnvelopeHeld)
    {
        return std::min (1.0f, ((float) psg.envelopeRamps + (float) psg.envelopeHeldSteps / kStepsPerRamp) / ramps);
    }

    //  With ALT, each ramp after the first runs the other way.
    if ((psg.envelopeRegister & kAlt) != 0 && (ramp % 2) == 1)
    {
        isRising = !isRising;
    }

    return ((float) ramp + (isRising ? (float) psg.envelopeLevel : kMaxLevel - (float) psg.envelopeLevel) / kMaxLevel) / ramps;
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
        m_ink.PaintValue (p, at, label, value);
        SoundBoardInk::AddSpot    (p, at, SoundBoardTips::GetVia (label, half));
    };
    auto                         lamp     = [&] (int x, const wchar_t * label, bool isOn)
    {
        m_ink.PaintLamp (p, (float) (rect.left + x * wide), lampY, label, isOn, lit, (float) wide);
        SoundBoardInk::AddSpot   (p, { rect.left + x * wide, rect.top + 4 * row, rect.left + (x + 1) * wide, rect.top + 5 * row }, SoundBoardTips::GetVia (label, half));
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

        m_ink.PaintValue (p, at, std::format (L"R{}", r), std::format (L"{:02X}", psg.registers[(size_t) r]), kNameDip);
        SoundBoardInk::AddSpot    (p, at, SoundBoardTips::GetRegister (r, psg.registers[(size_t) r]));
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



    m_ink.PaintCard (p, card, std::format (L"AY #{}", half.index + 1), std::format (L"6522 #{}, slot {}, ${:04X}", half.index + 1, m_board.slot, half.baseAddress));
    m_ink.PaintLamp (p, lampX, lampY, L"IRQ", half.via.isIrq, p.theme.ErrorForeground(), (float) m_scaler.ToPx (kLampDip));
    SoundBoardInk::AddSpot   (p, { (long) lampX, card.top + pad, card.right - pad, card.top + pad + m_scaler.ToPx (kTitleDip) }, SoundBoardTips::GetVia (L"IRQ", half));

    for (int c = 0; c < kChannels; c++)
    {
        PaintChannel (p, half.psg, GetChannelRect (card, c), c, half.index * kChannels + c);
    }

    PaintEnvelope (p, half.psg, envelope);
    PaintVia (p, half, { envelope.left, envelope.bottom + m_scaler.ToPx (kGapDip), envelope.right, card.bottom - pad });
}





