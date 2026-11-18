#pragma once

#include "Debugger/DiagnosticsSnapshot.h"
#include "Ui/Debugger/Panes/SoundBoardInk.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundVoiceCard
//
//  The sound board's card for a speech chip: the phoneme playing, its
//  waveform and its formants, a strip of the phonemes before it and their
//  labels, and the chip's values under them. The strip scrolls back through
//  the phonemes the chip has.
//
////////////////////////////////////////////////////////////////////////////////

class SoundVoiceCard
{
public:
    using Painting   = SoundBoardInk::Painting;
    using Palette    = SoundBoardInk::Palette;
    using TraceLayer = SoundBoardInk::TraceLayer;

    //  The channel the speech chip's mute has.
    static constexpr int  kVoiceMuteChannel = 6;

    SoundVoiceCard (const SoundBoardInk & ink, const DxuiDpiScaler & scaler, const DiagnosticsSoundBoard & board, const Palette & palette) :
        m_ink     (ink),
        m_scaler  (scaler),
        m_board   (board),
        m_palette (palette)
    {
    }

    //  The card's height with its values in `columns` columns, and the
    //  number of columns the values fit in across a width.
    int     GetHeight        (int columns) const;
    int     GetValueColumns  (int clientWidth) const;

    //  Where the mute is on a card, and the strip's heading, the strip and
    //  the row of labels under it.
    RECT    GetMuteRect      (const RECT & card) const;
    RECT    GetStripArea     (const RECT & card) const;

    void    ScrollPhonemes   (int count);
    size_t  GetPhonemesBack  () const { return m_phonemesBack; }

    void    Paint            (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & card, int columns) const;

private:
    static constexpr int  kGapDip        = SoundBoardInk::kGapDip;
    static constexpr int  kPadDip        = SoundBoardInk::kPadDip;
    static constexpr int  kTitleDip      = SoundBoardInk::kTitleDip;
    static constexpr int  kRegisterDip   = SoundBoardInk::kRegisterDip;
    static constexpr int  kPhonemeDip    = 128;
    static constexpr int  kStripDip      = 30;
    static constexpr int  kStripHeadDip  = 18;
    static constexpr int  kHistoryDip    = 30;
    static constexpr int  kValueCount    = 8;
    static constexpr int  kValueFirstDip = 250;     // the first column's least width, for the mode's name
    static constexpr int  kValueCellDip  = 160;
    static constexpr int  kVoiceLabelDip = 78;
    static constexpr int  kWordBreakDip  = 6;

    RECT    GetValueCell          (const RECT & card, int index, int columns) const;
    void    PaintStrip            (const Painting & p, std::span<const DiagnosticsSoundVoice::Phoneme> recent, bool isSounding, const RECT & rect) const;
    float   GetWidestPhonemeLabel (const Painting & p, const DxuiFontHandle & font) const;
    void    PaintWordBreak        (const Painting & p, float x, float top, float bottom) const;
    size_t  PaintHistory          (const Painting & p, std::span<const DiagnosticsSoundVoice::Phoneme> recent, bool isSounding, const RECT & rect) const;
    void    PaintFormants         (const Painting & p, const DiagnosticsSoundVoice & voice, const RECT & rect) const;

    const SoundBoardInk          & m_ink;
    const DxuiDpiScaler          & m_scaler;
    const DiagnosticsSoundBoard  & m_board;
    const Palette                & m_palette;
    size_t                         m_phonemesBack      = 0;      // how many of the newest phonemes the strip is scrolled back past
    mutable float                  m_widestLabelPx     = 0.0f;   // the widest phoneme symbol, at
    mutable float                  m_widestLabelSizePx = 0.0f;   // this font size
};
