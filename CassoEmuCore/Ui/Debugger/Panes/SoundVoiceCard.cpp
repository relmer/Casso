#include "Pch.h"

#include "Ui/Debugger/Panes/SoundVoiceCard.h"
#include "Core/TextEncoding.h"
#include "Devices/Ssi263.h"
#include "Ui/Debugger/Panes/SoundBoardLayout.h"
#include "Ui/Debugger/Panes/SoundBoardTips.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::GetHeight
//
////////////////////////////////////////////////////////////////////////////////

int SoundVoiceCard::GetHeight (int columns) const
{
    int  rows = kValueCount / columns;



    return m_scaler.ToPx (kPadDip + kTitleDip + kPhonemeDip + kGapDip + kStripHeadDip + kStripDip + kHistoryDip + kGapDip + rows * kRegisterDip + kPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::GetMuteRect
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundVoiceCard::GetMuteRect (const RECT & card) const
{
    constexpr int  kMuteDip = 24;
    int            side     = m_scaler.ToPx (kMuteDip);
    int            pad      = m_scaler.ToPx (kPadDip);



    return { card.right - pad - side, card.top + pad, card.right - pad, card.top + pad + side };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::ScrollPhonemes
//
////////////////////////////////////////////////////////////////////////////////

void SoundVoiceCard::ScrollPhonemes (int count)
{
    size_t  oldest = (m_board.voice.has_value() && !m_board.voice->recent.empty()) ? m_board.voice->recent.size() - 1 : 0;
    int     back   = (int) m_phonemesBack + count;



    m_phonemesBack = (size_t) std::clamp (back, 0, (int) oldest);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::GetStripArea
//
//  The strip's heading, the strip and the history row under it, as
//  PaintVoice places them.
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundVoiceCard::GetStripArea (const RECT & card) const
{
    int  pad    = m_scaler.ToPx (kPadDip);
    int  top    = card.top + pad + m_scaler.ToPx (kTitleDip) + m_scaler.ToPx (kPhonemeDip) + m_scaler.ToPx (kGapDip);



    return RECT { card.left + pad, top, card.right - pad, top + m_scaler.ToPx (kStripHeadDip + kStripDip + kHistoryDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::GetValueColumns
//
//  Four where the mode's name fits in the first column and the rest beside it,
//  two where two first columns fit, and one where they don't, so no value runs
//  into the next one's label.
//
////////////////////////////////////////////////////////////////////////////////

int SoundVoiceCard::GetValueColumns (int clientWidth) const
{
    int  inner = clientWidth - 2 * m_scaler.ToPx (kPadDip);



    if (inner >= m_scaler.ToPx (kValueFirstDip + 3 * kValueCellDip))
    {
        return 4;
    }

    return (inner >= m_scaler.ToPx (2 * kValueFirstDip)) ? 2 : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::GetValueCell
//
//  A speech chip value's place, by its index in the order they are painted,
//  row by row under the strip of recent phonemes. The first column is the
//  widest, for the mode's name.
//
////////////////////////////////////////////////////////////////////////////////

RECT SoundVoiceCard::GetValueCell (const RECT & card, int index, int columns) const
{
    int  pad     = m_scaler.ToPx (kPadDip);
    int  left    = card.left + pad;
    int  inner   = card.right - pad - left;
    int  row     = index / columns;
    int  column  = index % columns;
    int  first   = (columns == 4) ? inner / 3 : inner / columns;
    int  other   = (columns > 1) ? (inner - first) / (columns - 1) : 0;
    int  from    = (column == 0) ? left : left + first + (column - 1) * other;
    int  to      = (column == 0) ? left + first : from + other;
    int  high    = m_scaler.ToPx (kRegisterDip);
    int  top     = card.top + pad + m_scaler.ToPx (kTitleDip + kPhonemeDip + kGapDip + kStripHeadDip + kStripDip + kHistoryDip + kGapDip);



    return RECT { from, top + row * high, to, top + (row + 1) * high };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::Paint
//
////////////////////////////////////////////////////////////////////////////////

void SoundVoiceCard::Paint (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & card, int columns) const
{
    constexpr int                                    kLeftDip     = 190;
    constexpr int                                    kLampWideDip = 86;
    constexpr int                                    kLampsDip    = 3 * kLampWideDip + 36;
    constexpr int                                    kBigDip      = 58;
    constexpr int                                    kWordDip     = 22;
    constexpr int                                    kCodeDip     = 20;
    constexpr int                                    kSegments    = 15;
    constexpr float                                  kBigScale    = 2.6f;
    constexpr float                                  kUnlitAlpha  = 0.18f;
    int                                              pad          = m_scaler.ToPx (kPadDip);
    int                                              gap          = m_scaler.ToPx (kGapDip);
    int                                              top          = card.top + pad + m_scaler.ToPx (kTitleDip);
    int                                              mainBottom   = top + m_scaler.ToPx (kPhonemeDip);
    int                                              stripTop     = mainBottom + gap + m_scaler.ToPx (kStripHeadDip);
    int                                              captionHigh  = m_scaler.ToPx (kStripHeadDip);
    int                                              stripBottom  = stripTop + m_scaler.ToPx (kStripDip);
    int                                              historyEnd   = stripBottom + m_scaler.ToPx (kHistoryDip);
    size_t                                           back         = 0;
    std::span<const DiagnosticsSoundVoice::Phoneme>  older;
    int                                              left         = card.left + pad;
    int                                              right        = card.right - pad;
    int                                              inner        = right - left;
    int                                              leftWide     = std::min (m_scaler.ToPx (kLeftDip), inner / 3);
    int                                              scopeLeft    = left + leftWide + gap;
    int                                              rest         = right - scopeLeft;
    int                                              scopeRight   = scopeLeft + (rest - gap) / 2;
    RECT                                             mute         = GetMuteRect (card);
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
    auto                                             cell         = [&] (int index) { return GetValueCell (card, index, columns); };



    big.sizeDip *= kBigScale;

    m_ink.PaintCard (p, card, L"SSI-263", std::format (L"Speech chip, slot {}", m_board.slot));

    m_ink.PaintLamp (p, lampX,                                         lampY, L"Power",      !voice.isPoweredDown, m_palette.voice, (float) m_scaler.ToPx (kLampWideDip));
    m_ink.PaintLamp (p, lampX + (float) m_scaler.ToPx (kLampWideDip),  lampY, L"Sounding",   voice.isSounding,     m_palette.voice, (float) m_scaler.ToPx (kLampWideDip));
    m_ink.PaintLamp (p, lampX + (float) m_scaler.ToPx (2 * kLampWideDip), lampY, L"Request", voice.isRequesting,   m_palette.voice, (float) m_scaler.ToPx (kLampWideDip));

    m_ink.PaintMuteButton (p, mute, kVoiceMuteChannel, voice.isMuted, m_palette.voice);
    m_ink.PaintString (p, L"M", mute, voice.isMuted ? p.theme.ErrorForeground() : m_palette.voice, p.theme.BodyBoldFont(), DxuiTextHAlign::Center);

    if (voice.isMuted)
    {
        p.painter.DrawLine ((float) mute.left + radius, (float) mute.bottom - radius, (float) mute.right - radius, (float) mute.top + radius, m_scaler.ToPxf (1.5f), p.theme.ErrorForeground());
    }

    m_ink.PaintString (p, name, { left, top, left + leftWide, top + m_scaler.ToPx (kBigDip) }, voice.isSounding ? color : p.theme.ForegroundMuted(), big);
    m_ink.PaintString (p, word, { left, top + m_scaler.ToPx (kBigDip), left + leftWide, top + m_scaler.ToPx (kBigDip + kWordDip) }, p.theme.Foreground(), p.theme.BodyFont());
    m_ink.PaintString (p, std::format (L"${:02X}", voice.phonemeCode),
                 { left, top + m_scaler.ToPx (kBigDip + kWordDip), left + leftWide, top + m_scaler.ToPx (kBigDip + kWordDip + kCodeDip) }, p.theme.ForegroundMuted(), p.theme.MonospaceFont());

    for (int s = 0; s < kSegments; s++)
    {
        bool  isLit = s < litSegments;

        p.painter.FillRect ((float) left + (float) s * segmentWide, meterTop, segmentWide - m_scaler.ToPxf (1.5f), meterHigh,
                            isLit ? SoundBoardInk::Mix (color, 0xFFFFFFFF, (float) s / (float) kSegments * 0.35f) : SoundBoardInk::WithAlpha (p.theme.ForegroundMuted(), kUnlitAlpha));
    }

    //  Each graph under a caption.
    m_ink.PaintString   (p, L"Waveform", { scopeLeft, top, scopeRight, top + captionHigh }, p.theme.ForegroundMuted(), p.theme.CaptionFont());
    m_ink.PaintString   (p, L"Formants", { scopeRight + gap, top, right, top + captionHigh }, p.theme.ForegroundMuted(), p.theme.CaptionFont());
    m_ink.PaintScope    (p, voice.scope, { scopeLeft, top + captionHigh, scopeRight, mainBottom }, color, 0.0, true);
    PaintFormants (p, voice, { scopeRight + gap, top + captionHigh, right, mainBottom });
    //  The wheel over the strip scrolls it back through the history, and the
    //  newest phoneme there isn't sounding.
    back  = std::min (m_phonemesBack, voice.recent.size() > 0 ? voice.recent.size() - 1 : 0);
    older = std::span<const DiagnosticsSoundVoice::Phoneme> (voice.recent).first (voice.recent.size() - back);

    m_ink.PaintString (p, (back == 0) ? std::wstring (L"Recent phonemes") : std::format (L"Recent phonemes, {} back", back),
                 { left, mainBottom + gap, right, stripTop }, p.theme.ForegroundMuted(), p.theme.CaptionFont());

    (void) PaintHistory (p, older, voice.isSounding && back == 0, { left, stripBottom, right, historyEnd });
    PaintStrip (p, older, voice.isSounding && back == 0, { left, stripTop, right, stripBottom });

    m_ink.PaintValue (p, cell (0), L"Mode",         TextEncoding::NarrowToWide (voice.mode), kVoiceLabelDip);
    m_ink.PaintValue (p, cell (1), L"Duration",     std::format (L"{:.1f} ms", voice.durationMs), kVoiceLabelDip);
    m_ink.PaintValue (p, cell (2), L"Frame",        std::format (L"{:.1f} ms", voice.frameMs), kVoiceLabelDip);
    m_ink.PaintValue (p, cell (3), L"Pitch",        SoundBoardLayout::GetFrequencyText (voice.pitchHz), kVoiceLabelDip);
    m_ink.PaintValue (p, cell (4), L"Formants",     std::format (L"{:.0f}, {:.0f}, {:.0f}", voice.formantsHz[0], voice.formantsHz[1], voice.formantsHz[2]), kVoiceLabelDip);
    m_ink.PaintValue (p, cell (5), L"Articulation", std::format (L"${:X}", voice.articulation), kVoiceLabelDip);
    m_ink.PaintValue (p, cell (6), L"Amplitude",    std::format (L"${:X}", voice.amplitude), kVoiceLabelDip);
    m_ink.PaintValue (p, cell (7), L"Filter",       SoundBoardLayout::GetFrequencyText (voice.filterHz), kVoiceLabelDip);

    //  What each part means, at the place it is drawn.
    for (const auto & [at, label] : std::initializer_list<std::pair<RECT, const wchar_t *>>
         {
             { cell (0), L"Mode" },     { cell (1), L"Duration" },     { cell (2), L"Frame" },     { cell (3), L"Pitch" },
             { cell (4), L"Formants" }, { cell (5), L"Articulation" }, { cell (6), L"Amplitude" }, { cell (7), L"Filter" },
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
        SoundBoardInk::AddSpot (p, at, SoundBoardTips::GetVoice (label, voice));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::PaintStrip
//
//  Each block is as wide as its phoneme lasted, at a fixed scale, so its
//  width doesn't change as it ages and moves left. No block is narrower than
//  the widest phoneme's label, so every block has its label. The oldest block
//  drawn is cut off at the strip's left end, with its label centered in the
//  part drawn, and a block longer than the strip fills it. The newest
//  phoneme is brightest while it sounds, and each older one fainter. A pause,
//  which falls between words, is a line in a slot as narrow as the label
//  row's rather than a block.
//
////////////////////////////////////////////////////////////////////////////////

void SoundVoiceCard::PaintStrip (const Painting & p, std::span<const DiagnosticsSoundVoice::Phoneme> recent, bool isSounding, const RECT & rect) const
{
    constexpr float       kRadiusDip    = 4.0f;
    constexpr float       kGapDip2      = 3.0f;
    constexpr float       kLabelPadDip  = 8.0f;
    constexpr float       kDipPerSecond = 250.0f;
    constexpr float       kOldest       = 0.10f;
    constexpr float       kNewest       = 0.45f;
    const DxuiFontHandle  font          = p.theme.CaptionFont();
    float                 least         = GetWidestPhonemeLabel (p, font) + m_scaler.ToPxf (kLabelPadDip);
    std::vector<float>    seconds;
    std::vector<float>    widths;



    //  A pause is a word break, as narrow as the label row's whatever its
    //  length: a rest between sung phrases left a wide black gap.
    for (const DiagnosticsSoundVoice::Phoneme & phoneme : recent)
    {
        seconds.push_back (phoneme.isPause ? 0.0f : phoneme.seconds);
        widths.push_back  (phoneme.isPause ? m_scaler.ToPxf (kWordBreakDip) : least);
    }

    p.painter.FillRoundedRect ((float) rect.left, (float) rect.top, (float) (rect.right - rect.left), (float) (rect.bottom - rect.top), m_scaler.ToPxf (kRadiusDip), p.screen);

    auto  placed = SoundBoardLayout::PlaceByDuration (seconds, widths,(float) (rect.right - rect.left), m_scaler.ToPxf (kGapDip2), m_scaler.ToPxf (kDipPerSecond));

    p.painter.PushClip ((float) rect.left, (float) rect.top, (float) (rect.right - rect.left), (float) (rect.bottom - rect.top));

    for (size_t i = 0; i < placed.size(); i++)
    {
        const SoundBoardLayout::StripPlace      & place   = placed[i];
        const DiagnosticsSoundVoice::Phoneme    & phoneme = recent[place.index];
        bool                                      isLast  = i + 1 == placed.size();
        float                                     age     = placed.size() > 1 ? (float) i / (float) (placed.size() - 1) : 1.0f;
        float                                     alpha   = kOldest + (kNewest - kOldest) * age;
        float                                     x       = (float) rect.left + place.x;
        float                                     shown   = std::max (x, (float) rect.left);
        float                                     end     = std::min (x + place.width, (float) rect.right);
        uint32_t                                  fill    = (isLast && isSounding) ? m_palette.voice : SoundBoardInk::WithAlpha (m_palette.voice, alpha);
        uint32_t                                  ink     = (isLast && isSounding) ? p.theme.Background() : p.theme.Foreground();

        SoundBoardInk::AddSpot (p, { (long) shown, rect.top, (long) end + 1, rect.bottom }, SoundBoardTips::GetPhoneme (phoneme));

        if (phoneme.isPause)
        {
            PaintWordBreak (p, (shown + end) * 0.5f, (float) rect.top, (float) rect.bottom);
            continue;
        }

        p.painter.FillRoundedRect (x, (float) rect.top, place.width, (float) (rect.bottom - rect.top), m_scaler.ToPxf (kRadiusDip), fill);
        m_ink.PaintString (p, TextEncoding::NarrowToWide (phoneme.name.substr (0, phoneme.name.find (' '))),
                     { (long) shown, rect.top, (long) end, rect.bottom }, ink, font, DxuiTextHAlign::Center);
    }

    p.painter.PopClip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::GetWidestPhonemeLabel
//
//  The widest of the speech chip's phoneme symbols in a font, measured again
//  only when the font's size changes.
//
////////////////////////////////////////////////////////////////////////////////

float SoundVoiceCard::GetWidestPhonemeLabel (const Painting & p, const DxuiFontHandle & font) const
{
    float  size = m_scaler.ToPxf (font.sizeDip);



    if (size == m_widestLabelSizePx)
    {
        return m_widestLabelPx;
    }

    m_widestLabelSizePx = size;
    m_widestLabelPx     = 0.0f;

    for (int code = 0; code < (int) Ssi263::kPhonemeCount; code++)
    {
        std::string   symbol = Ssi263::GetPhonemeName ((Byte) code);
        std::wstring  name   = TextEncoding::NarrowToWide (symbol.substr (0, symbol.find (' ')));
        float         wide   = 0.0f;
        float         high   = 0.0f;
        HRESULT       hr     = p.text.MeasureString (name.c_str(), size, font.face, wide, high);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_widestLabelPx = std::max (m_widestLabelPx, wide);
    }

    return m_widestLabelPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::PaintWordBreak
//
//  A pause between words: a line the speech chip's color, from top to
//  bottom.
//
////////////////////////////////////////////////////////////////////////////////

void SoundVoiceCard::PaintWordBreak (const Painting & p, float x, float top, float bottom) const
{
    constexpr float  kWidthDip = 2.0f;
    float            snapped   = std::floor (x) + 0.5f;



    p.painter.DrawLine (snapped, top, snapped, bottom, m_scaler.ToPxf (kWidthDip), m_palette.voice);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard::PaintHistory
//
//  The recent phonemes' labels in order along a row, newest at the right, as
//  many as fit, with an arrow under the newest while it sounds, so a
//  consonant can be read after it has passed. A pause is a line in place of
//  its label, between the words. Returns how many are drawn.
//
////////////////////////////////////////////////////////////////////////////////

size_t SoundVoiceCard::PaintHistory (const Painting & p, std::span<const DiagnosticsSoundVoice::Phoneme> recent, bool isSounding, const RECT & rect) const
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
        widths.push_back (phoneme.isPause ? m_scaler.ToPxf (kWordBreakDip) : wide);
    }

    count = SoundBoardLayout::FitHistory (widths, (float) (rect.right - rect.left), gap);

    for (size_t i = names.size(); i > names.size() - count; i--)
    {
        bool      isActive = i == names.size() && isSounding;
        uint32_t  ink      = isActive ? m_palette.voice : p.theme.Foreground();
        float     left     = x - widths[i - 1];

        if (recent[i - 1].isPause)
        {
            PaintWordBreak (p, left + widths[i - 1] * 0.5f, (float) rect.top, textBottom);
        }
        else
        {
            m_ink.PaintString (p, names[i - 1], { (long) left, rect.top, (long) x + 1, (long) textBottom }, ink, font, DxuiTextHAlign::Left);
        }

        SoundBoardInk::AddSpot (p, { (long) left, rect.top, (long) x + 1, rect.bottom }, SoundBoardTips::GetPhoneme (recent[i - 1]));

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
//  SoundVoiceCard::PaintFormants
//
//  The tract's response as a glowing curve over a filled area, on a logarithmic
//  axis, with the pitch's harmonics rising under it as a comb. A voiced vowel
//  reads as the comb under the three peaks. The glow goes under the fill and
//  the comb, and the bright line over them.
//
////////////////////////////////////////////////////////////////////////////////

void SoundVoiceCard::PaintFormants (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & rect) const
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

    for (int c = 0; c < columns; c++)
    {
        points.emplace_back (x + inset + (float) c * step, base - curve[(size_t) c] * plotH);
    }

    m_ink.PaintTrace (p, points, voice.isSounding ? color : SoundBoardInk::WithAlpha (color, fade), TraceLayer::Glow);

    for (double hz : kAxisHz)
    {
        float  ax = x + inset + SoundBoardLayout::GetFormantX (hz) * plotW;

        p.painter.DrawLine (ax, y + inset, ax, base, 1.0f, SoundBoardInk::WithAlpha (p.theme.ForegroundMuted(), kGridAlpha));
        m_ink.PaintString (p, (hz >= 1000.0) ? std::format (L"{:.0f}k", hz / 1000.0) : std::format (L"{:.0f}", hz),
                     { (long) ax - m_scaler.ToPx (kLabelDip), (long) base, (long) ax + m_scaler.ToPx (kLabelDip), (long) (y + h) },
                     p.theme.ForegroundMuted(), p.theme.CaptionFont(), DxuiTextHAlign::Center);
    }

    //  Under the line, a column at a time: a slanted band from the line at the
    //  column's left edge to the line at its right, over a fading fill from
    //  the lower of the two down. A fill with a flat top at each column's left
    //  height rose past the line in steps.
    for (int c = 0; c + 1 < columns; c++)
    {
        float     left    = x + inset + (float) c * step;
        float     right   = left + step;
        float     atLeft  = base - curve[(size_t) c] * plotH;
        float     atRight = base - curve[(size_t) c + 1] * plotH;
        float     lower   = std::max (atLeft, atRight);
        uint32_t  top     = SoundBoardInk::WithAlpha (color, kTopAlpha * fade);

        p.painter.FillConvexQuad (left, atLeft, right, atRight, right, lower, left, lower, top);
        p.painter.FillGradientRect (left, lower, step, base - lower, top, SoundBoardInk::WithAlpha (color, 0.02f));
    }

    if (voice.pitchHz >= kCombLeast && voice.isSounding)
    {
        for (double hz = voice.pitchHz; hz < SoundBoardLayout::kFormantMaxHz; hz += voice.pitchHz)
        {
            float   cx    = SoundBoardLayout::GetFormantX (hz);
            size_t  index = std::min ((size_t) (cx * (float) (columns - 1)), curve.size() - 1);
            float   gx    = x + inset + cx * plotW;

            p.painter.DrawLine (gx, base, gx, base - curve[index] * plotH, 1.0f, SoundBoardInk::WithAlpha (p.theme.Foreground(), kCombAlpha));
        }
    }

    m_ink.PaintTrace (p, points, voice.isSounding ? color : SoundBoardInk::WithAlpha (color, fade), TraceLayer::Core);
}





