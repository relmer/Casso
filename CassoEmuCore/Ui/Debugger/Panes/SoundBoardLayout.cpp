#include "Pch.h"

#include "Ui/Debugger/Panes/SoundBoardLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::GetNoteText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardLayout::GetNoteText (double hz)
{
    static constexpr const wchar_t * kNames[] = { L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };
    constexpr double  kA4Hz        = 440.0;
    constexpr int     kA4Midi      = 69;
    constexpr int     kMidiMax     = 127;
    constexpr int     kSemitones   = 12;
    constexpr int     kCents       = 100;
    constexpr int     kOctaveBase  = 1;
    double            midi         = 0.0;
    int               nearest      = 0;
    int               cents        = 0;
    std::wstring      text;



    if (hz <= 0.0)
    {
        return text;
    }

    midi    = kA4Midi + kSemitones * std::log2 (hz / kA4Hz);
    nearest = (int) std::lround (midi);
    cents   = (int) std::lround ((midi - nearest) * kCents);

    if (nearest < 0 || nearest > kMidiMax)
    {
        return text;
    }

    text = std::format (L"{}{}", kNames[nearest % kSemitones], nearest / kSemitones - kOctaveBase);

    if (cents != 0)
    {
        text += std::format (L" {:+}", cents);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::GetFrequencyText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardLayout::GetFrequencyText (double hz)
{
    constexpr double  kHzPerKhz = 1000.0;



    if (hz >= kHzPerKhz)
    {
        return std::format (L"{:.2f} kHz", hz / kHzPerKhz);
    }

    return std::format (L"{:.1f} Hz", hz);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::GetEnvelopePoints
//
//  The first ramp rises with ATT set and falls without. Without CONT the level
//  drops to 0 for the rest. With bit 0 set the level is constant after the first
//  ramp: where it ended, or at the other end with ALT. Otherwise ramps repeat,
//  each reversing the last with ALT.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SoundBoardLayout::Point> SoundBoardLayout::GetEnvelopePoints (Byte shapeRegister)
{
    constexpr Byte      kContinue  = 0x08;
    constexpr Byte      kAttack    = 0x04;
    constexpr Byte      kAlternate = 0x02;
    constexpr Byte      kHold      = 0x01;
    bool                isRising   = (shapeRegister & kAttack) != 0;
    bool                isAlt      = (shapeRegister & kAlternate) != 0;
    float               step       = 1.0f / kEnvelopeRamps;
    float               level      = isRising ? 1.0f : 0.0f;
    std::vector<Point>  points;
    int                 ramp       = 0;



    points.push_back ({ 0.0f, isRising ? 0.0f : 1.0f });
    points.push_back ({ step, level });

    if ((shapeRegister & kContinue) == 0)
    {
        points.push_back ({ step, 0.0f });
        points.push_back ({ 1.0f, 0.0f });
        return points;
    }

    if ((shapeRegister & kHold) != 0)
    {
        level = isAlt ? 1.0f - level : level;

        points.push_back ({ step, level });
        points.push_back ({ 1.0f, level });
        return points;
    }

    for (ramp = 1; ramp < kEnvelopeRamps; ramp++)
    {
        float  x = step * (float) (ramp + 1);

        if (isAlt)
        {
            isRising = !isRising;
        }
        else
        {
            points.push_back ({ step * (float) ramp, isRising ? 0.0f : 1.0f });
        }

        points.push_back ({ x, isRising ? 1.0f : 0.0f });
    }

    return points;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::GetScopeWindow
//
////////////////////////////////////////////////////////////////////////////////

size_t SoundBoardLayout::GetScopeWindow (double hz, uint32_t sampleRate)
{
    constexpr double  kCycles = 4.0;



    if (hz <= 0.0 || sampleRate == 0)
    {
        return kScopeWindowMax;
    }

    return std::clamp ((size_t) (kCycles * sampleRate / hz), kScopeWindowMin, kScopeWindowMax);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::FindTrigger
//
////////////////////////////////////////////////////////////////////////////////

size_t SoundBoardLayout::FindTrigger (std::span<const float> samples, size_t window)
{
    size_t  last = (samples.size() > window) ? samples.size() - window : 0;
    float   low  = 0.0f;
    float   high = 0.0f;
    float   mid  = 0.0f;
    size_t  i    = 0;



    if (samples.empty())
    {
        return 0;
    }

    low  = *std::min_element (samples.begin(), samples.end());
    high = *std::max_element (samples.begin(), samples.end());
    mid  = (low + high) * 0.5f;

    for (i = 1; i <= last && high > low; i++)
    {
        if (samples[i - 1] < mid && samples[i] >= mid)
        {
            return i;
        }
    }

    return last;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::GetScopeColumns
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SoundBoardLayout::ScopeColumn> SoundBoardLayout::GetScopeColumns (
    std::span<const float>  samples,
    size_t                  first,
    size_t                  window,
    int                     columns,
    float                   low,
    float                   high)
{
    float                     range = std::max (high - low, 0.0001f);
    std::vector<ScopeColumn>  out;
    int                       c     = 0;



    if (columns <= 0 || window == 0 || first >= samples.size())
    {
        return out;
    }

    window = std::min (window, samples.size() - first);
    out.reserve ((size_t) columns);

    for (c = 0; c < columns; c++)
    {
        size_t  from = first + window * (size_t) c / (size_t) columns;
        size_t  to   = std::max (from + 1, first + window * (size_t) (c + 1) / (size_t) columns);
        float   lo   = samples[from];
        float   hi   = samples[from];
        size_t  i    = 0;

        for (i = from; i < to && i < samples.size(); i++)
        {
            lo = std::min (lo, samples[i]);
            hi = std::max (hi, samples[i]);
        }

        out.push_back ({ std::clamp ((lo - low) / range, 0.0f, 1.0f), std::clamp ((hi - low) / range, 0.0f, 1.0f) });
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::GetFormantX
//
////////////////////////////////////////////////////////////////////////////////

float SoundBoardLayout::GetFormantX (double hz)
{
    if (hz <= 0.0)
    {
        return 0.0f;
    }

    return std::clamp ((float) (std::log (hz / kFormantMinHz) / std::log (kFormantMaxHz / kFormantMinHz)), 0.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::GetFormantCurve
//
////////////////////////////////////////////////////////////////////////////////

std::vector<float> SoundBoardLayout::GetFormantCurve (const std::array<double, 3> & formantsHz, int columns)
{
    static constexpr float  kHeights[] = { 1.0f, 0.78f, 0.58f };
    constexpr float         kWidth     = 0.05f;
    std::vector<float>      out;
    int                     c          = 0;
    size_t                  f          = 0;



    out.assign ((size_t) std::max (columns, 0), 0.0f);

    for (c = 0; c < columns; c++)
    {
        float  x = (columns > 1) ? (float) c / (float) (columns - 1) : 0.0f;

        for (f = 0; f < formantsHz.size(); f++)
        {
            float  away = (x - GetFormantX (formantsHz[f])) / kWidth;

            if (formantsHz[f] > 0.0)
            {
                out[(size_t) c] = std::max (out[(size_t) c], kHeights[f] * std::exp (-0.5f * away * away));
            }
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::PlaceStrip
//
//  Widths scale so that a phoneme of kFullSeconds fills kFullWidthShare of the
//  strip, which spreads a run of phonemes without one long pause taking it all.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SoundBoardLayout::StripPlace> SoundBoardLayout::PlaceStrip (
    const std::vector<float>  & seconds,
    const std::vector<float>  & leastWidths,
    float                       width,
    float                       gap)
{
    constexpr float          kFullSeconds    = 0.25f;
    constexpr float          kFullWidthShare = 0.3f;



    return PlaceAtScale (seconds, leastWidths, width, gap, width * kFullWidthShare / kFullSeconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::FillStrip
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SoundBoardLayout::StripPlace> SoundBoardLayout::FillStrip (
    const std::vector<float>  & seconds,
    const std::vector<float>  & leastWidths,
    float                       width,
    float                       gap)
{
    float  total = 0.0f;
    float  room  = width - gap * (float) (seconds.empty() ? 0 : seconds.size() - 1);



    for (float each : seconds)
    {
        total += each;
    }

    return PlaceAtScale (seconds, leastWidths, width, gap, (total > 0.0f) ? std::max (0.0f, room) / total : 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::PlaceAtScale
//
//  Newest at the right, each block its seconds at the scale or its least
//  width; the oldest that do not fit are left out.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SoundBoardLayout::StripPlace> SoundBoardLayout::PlaceAtScale (
    const std::vector<float>  & seconds,
    const std::vector<float>  & leastWidths,
    float                       width,
    float                       gap,
    float                       scale)
{
    float                    right           = width;
    std::vector<StripPlace>  placed;
    size_t                   i               = seconds.size();



    while (i > 0)
    {
        float  wide = std::max (leastWidths[i - 1], seconds[i - 1] * scale);

        i--;

        if (right - wide < 0.0f)
        {
            break;
        }

        placed.push_back ({ i, right - wide, wide });
        right -= wide + gap;
    }

    std::reverse (placed.begin(), placed.end());
    return placed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::FitHistory
//
////////////////////////////////////////////////////////////////////////////////

size_t SoundBoardLayout::FitHistory (const std::vector<float> & widths, float width, float gap)
{
    float   used  = 0.0f;
    size_t  count = 0;



    for (size_t i = widths.size(); i > 0; i--)
    {
        float  next = used + ((count > 0) ? gap : 0.0f) + widths[i - 1];

        if (next > width)
        {
            break;
        }

        used = next;
        count++;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::GetColumnCount
//
////////////////////////////////////////////////////////////////////////////////

int SoundBoardLayout::GetColumnCount (int width, int leastCardWidth, int gap, int cards)
{
    return std::clamp ((width + gap) / std::max (leastCardWidth + gap, 1), 1, std::clamp (cards, 1, kMaxColumns));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayout::PlaceCards
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RECT> SoundBoardLayout::PlaceCards (
    const std::vector<int>  & heights,
    int                       columns,
    int                       width,
    int                       gap,
    int                     & outHeight)
{
    int                wide = (width - gap * (columns - 1)) / columns;
    std::vector<int>   tops ((size_t) columns, 0);
    std::vector<RECT>  places;



    for (int height : heights)
    {
        size_t  column = (size_t) (std::min_element (tops.begin(), tops.end()) - tops.begin());
        int     left   = (int) column * (wide + gap);

        places.push_back ({ left, tops[column], left + wide, tops[column] + height });
        tops[column] += height + gap;
    }

    outHeight = std::max (0, *std::max_element (tops.begin(), tops.end()) - gap);
    return places;
}
