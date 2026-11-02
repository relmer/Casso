#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout
//
//  The arithmetic of the Mockingboard view, apart from any drawing: a
//  frequency's note, the picture of an AY envelope, a stable start for a scope
//  and its columns, the formant curve, the phoneme strip, and the cards'
//  places in columns.
//
////////////////////////////////////////////////////////////////////////////////

class SoundBoardLayout
{
public:
    struct Point
    {
        float  x = 0.0f;
        float  y = 0.0f;
    };

    struct ScopeColumn
    {
        float  low  = 0.0f;
        float  high = 0.0f;
    };

    struct StripPlace
    {
        size_t  index = 0;
        float   x     = 0.0f;
        float   width = 0.0f;
    };

    static constexpr int     kEnvelopeRamps  = 4;
    static constexpr double  kFormantMinHz   = 150.0;
    static constexpr double  kFormantMaxHz   = 6000.0;
    static constexpr size_t  kScopeWindowMin = 96;
    static constexpr size_t  kScopeWindowMax = 512;

    //  The nearest note and its cents, as "A4" or "C#5 -12", for an equal
    //  temperament at A4 = 440 Hz; an empty string outside the MIDI range.
    static std::wstring  GetNoteText (double hz);

    //  "440.0 Hz", or "1.25 kHz" from 1 kHz up.
    static std::wstring  GetFrequencyText (double hz);

    //  An envelope over kEnvelopeRamps ramps, from R13's four bits: x from 0 to
    //  1, and y from 0 at silence to 1 at full.
    static std::vector<Point>  GetEnvelopePoints (Byte shapeRegister);

    //  The samples in about four cycles, within the scope's limits; the most
    //  for a frequency of 0, as for noise or speech.
    static size_t  GetScopeWindow (double hz, uint32_t sampleRate);

    //  The first rise through the samples' midpoint with a full window after
    //  it, or else the last full window.
    static size_t  FindTrigger (std::span<const float> samples, size_t window);

    //  Each column's lowest and highest sample from `first` over `window`
    //  samples, from 0 at `low` to 1 at `high`.
    static std::vector<ScopeColumn>  GetScopeColumns (std::span<const float> samples, size_t first, size_t window, int columns, float low, float high);

    //  A frequency's place from 0 to 1 on the formant picture's logarithmic axis.
    static float  GetFormantX (double hz);

    //  The height from 0 to 1 at each column of the formant picture: a peak at
    //  each formant, the first the tallest. A formant of 0 has no peak.
    static std::vector<float>  GetFormantCurve (const std::array<double, 3> & formantsHz, int columns);

    //  Places phonemes along a strip, newest at the right, each wide in
    //  proportion to its seconds and at least its least width. The oldest that
    //  do not fit are left out. `leastWidths` has one entry per entry of `seconds`.
    static std::vector<StripPlace>  PlaceStrip (const std::vector<float> & seconds, const std::vector<float> & leastWidths, float width, float gap);

    //  The same with all the phonemes sharing the width in proportion to
    //  their seconds, for a strip that has the phonemes of the history row.
    static std::vector<StripPlace>  FillStrip  (const std::vector<float> & seconds, const std::vector<float> & leastWidths, float width, float gap);

    //  How many of the newest labels, each as wide as `widths` has it with
    //  `gap` between them, fit on a row of `width`, counted from the end.
    static size_t  FitHistory (const std::vector<float> & widths, float width, float gap);

    //  How many columns of cards fit in a width, from 1 to kMaxColumns and no
    //  more than there are cards, so a wide pane has no empty column.
    static int  GetColumnCount (int width, int leastCardWidth, int gap, int cards);

    //  Each card's place in `columns` columns, the next in the shortest column,
    //  in coordinates from (0, 0); the total height is returned in `outHeight`.
    static std::vector<RECT>  PlaceCards (const std::vector<int> & heights, int columns, int width, int gap, int & outHeight);

    static constexpr int  kMaxColumns = 3;

private:
    static std::vector<StripPlace>  PlaceAtScale (const std::vector<float> & seconds, const std::vector<float> & leastWidths, float width, float gap, float scale);
};
