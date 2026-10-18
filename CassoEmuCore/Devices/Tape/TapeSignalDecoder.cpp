#include "Pch.h"

#include "Devices/Tape/TapeSignalDecoder.h"

#include "Devices/Tape/TapeRecordScanner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::Decode
//
//  Filtering only what needs it. A clean recording is decoded as the Apple's
//  own input would: DC removed, then a comparator switching close to zero.
//  That is the reading that survives the uneven half-cycles real transfers
//  have. But it is also the reading hiss flips at random, so the result is
//  checked first: when too many of its crossings come closer together than
//  any Apple tone can, the recording is noisy, and it is decoded again
//  smoothed, with the comparator switching further from zero.
//
//  Then every record that fails its checksum is given a second chance --
//  see RescueRecords.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::Decode (const TapeAudio & audio, TapeSignal & signal)
{
    std::vector<double>  filtered;



    signal               = TapeSignal();
    signal.lengthSamples = audio.samples.size();
    signal.sampleRate    = audio.sampleRate;

    if (audio.samples.empty() || audio.sampleRate == 0)
    {
        return;
    }

    Filter  (audio, false, filtered);
    Compare (filtered, audio.sampleRate, kCleanThreshold, kThresholdFloor, signal);

    if (ChatterShare (signal) > kNoisyChatterShare)
    {
        Filter  (audio, true, filtered);
        Compare (filtered, audio.sampleRate, kNoisyThreshold, kThresholdFloor, signal);
    }

    RescueRecords (audio, signal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::RescueRecords
//
//  A second chance for a record that fails its checksum. One setting cannot
//  read every damaged stretch of every real recording: a floor high enough
//  to keep hiss out of the gaps loses the tiny swings near zero one tape's
//  damaged stretch is made of, and a floor low enough for those lets hiss
//  in everywhere. So the whole tape is decoded the one way, and then each
//  standard record whose checksum fails is decoded again on its own, from
//  its leader to the next record's, with each of a few other settings in
//  turn. The first that gives it a good checksum replaces that stretch.
//
//  A record no setting rescues is left as it was decoded, and so is
//  anything that is not a standard record at all.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::RescueRecords (const TapeAudio & audio, TapeSignal & signal)
{
    std::vector<TapeRecord>  records;



    TapeRecordScanner::Scan (signal.transitions, signal.sampleRate, records);

    for (size_t k = 0; k < records.size(); k++)
    {
        const TapeRecord &   record = records[k];
        double               from   = record.leaderStart;
        double               to     = (k + 1 < records.size()) ? records[k + 1].leaderStart : (double) audio.samples.size();
        std::vector<double>  rescued;



        // Any record that fails, however short: damage can end one early, so
        // a leader and a sync are evidence enough that a record starts here.
        if (record.isValid)
        {
            continue;
        }

        for (const Settings & settings : kRescueSettings)
        {
            if (TryRescue (audio, from, to, record.dataStart, settings, rescued))
            {
                std::vector<double> &  edges = signal.transitions;
                auto                   first = std::lower_bound (edges.begin(), edges.end(), from);
                auto                   last  = std::lower_bound (edges.begin(), edges.end(), to);

                first = edges.erase (first, last);
                edges.insert (first, rescued.begin(), rescued.end());
                break;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::TryRescue
//
//  Decodes the samples from `from` to `to` with `settings`, starting a
//  little early so the filters and the envelope have settled by the
//  leader. Succeeds when the record starting at `dataStart` comes out with
//  a good checksum, and hands back the stretch's transitions, in the whole
//  recording's sample positions.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeSignalDecoder::TryRescue (const TapeAudio & audio, double from, double to, double dataStart,
                                   const Settings & settings, std::vector<double> & transitions)
{
    TapeAudio                slice;
    TapeSignal               decoded;
    std::vector<double>      filtered;
    std::vector<TapeRecord>  records;
    size_t                   start     = (size_t) max (0.0, from - kRescueLeadSeconds * audio.sampleRate);
    size_t                   end       = min ((size_t) ceil (to), audio.samples.size());
    double                   tolerance = 0.05 * audio.sampleRate;
    bool                     isRescued = false;



    transitions.clear();

    if (end <= start + 1)
    {
        return false;
    }

    slice.sampleRate = audio.sampleRate;
    slice.samples.assign (audio.samples.begin() + (ptrdiff_t) start, audio.samples.begin() + (ptrdiff_t) end);

    decoded.sampleRate = audio.sampleRate;
    Filter  (slice, settings.lowPass, filtered);
    Compare (filtered, audio.sampleRate, settings.ratio, settings.floor, decoded);

    for (double & t : decoded.transitions)
    {
        t += (double) start;
    }

    TapeRecordScanner::Scan (decoded.transitions, audio.sampleRate, records);

    for (const TapeRecord & record : records)
    {
        if (record.isValid && fabs (record.dataStart - dataStart) < tolerance)
        {
            isRescued = true;
        }
    }

    if (isRescued)
    {
        for (double t : decoded.transitions)
        {
            if (t >= from && t < to)
            {
                transitions.push_back (t);
            }
        }
    }

    return isRescued;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::Compare
//
//  A comparator with hysteresis that follows the signal's own level: the level
//  goes high when the filtered signal rises past +h and low when it falls past
//  -h, where h is `ratio` of a running peak envelope, never below
//  `floor`. Each crossing is placed between its two samples by linear
//  interpolation. The first crossing fixes the starting level as its
//  opposite, so the first event is always a transition.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::Compare (const std::vector<double> & filtered, uint32_t sampleRate, double ratio,
                                 double floor, TapeSignal & signal)
{
    double  releaseCoefficient = exp (-1.0 / (kReleaseSeconds * sampleRate));
    double  envelope           = 0.0;
    double  previous           = 0.0;
    bool    hasLevel           = false;
    bool    level              = false;



    signal.transitions.clear();
    signal.initialLevel = false;

    for (size_t i = 0; i < filtered.size(); i++)
    {
        double  sample    = filtered[i];
        double  threshold = 0.0;
        double  target    = 0.0;
        double  fraction  = 0.0;
        bool    rises     = false;
        bool    falls     = false;



        envelope  = max (fabs (sample), envelope * releaseCoefficient);
        threshold = max (envelope * ratio, floor);
        rises     = sample >  threshold && (!hasLevel || !level);
        falls     = sample < -threshold && (!hasLevel ||  level);

        if (rises || falls)
        {
            target = rises ? threshold : -threshold;

            if (!hasLevel)
            {
                signal.initialLevel = falls;
                hasLevel            = true;
            }

            fraction = i == 0 ? 0.0 : clamp ((target - previous) / (sample - previous), 0.0, 1.0);
            level    = rises;
            signal.transitions.push_back (i == 0 ? 0.0 : (double) (i - 1) + fraction);
        }

        previous = sample;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::ChatterShare
//
//  The share of crossings that follow the one before sooner than any Apple
//  tone can: noise, not signal.
//
////////////////////////////////////////////////////////////////////////////////

double TapeSignalDecoder::ChatterShare (const TapeSignal & signal)
{
    const std::vector<double>  & transitions = signal.transitions;
    double                       limit       = kChatterSeconds * signal.sampleRate;
    size_t                       chatter     = 0;



    if (transitions.size() < 2)
    {
        return 0.0;
    }

    for (size_t i = 1; i < transitions.size(); i++)
    {
        if (transitions[i] - transitions[i - 1] < limit)
        {
            chatter++;
        }
    }

    return (double) chatter / (double) (transitions.size() - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoder::Filter
//
//  A one-pole high-pass removes DC offset and rumble far below the tones. A
//  one-pole low-pass, when asked for, trims hiss if the sample rate leaves
//  room for it; it delays every edge equally, so the spacing the guest
//  measures is unchanged. The high-pass starts settled on the first sample,
//  so a constant offset produces no opening step.
//
////////////////////////////////////////////////////////////////////////////////

void TapeSignalDecoder::Filter (const TapeAudio & audio, bool lowPass, std::vector<double> & filtered)
{
    double  dt          = 1.0 / audio.sampleRate;
    double  highPassRc  = 1.0 / (2.0 * std::numbers::pi * kHighPassHz);
    double  lowPassRc   = 1.0 / (2.0 * std::numbers::pi * kLowPassHz);
    double  highAlpha   = highPassRc / (highPassRc + dt);
    double  lowAlpha    = dt / (lowPassRc + dt);
    bool    useLowPass  = lowPass && audio.sampleRate >= kMinLowPassRate;
    double  priorInput  = audio.samples.front();
    double  highOut     = 0.0;
    double  lowOut      = 0.0;



    filtered.resize (audio.samples.size());

    for (size_t i = 0; i < audio.samples.size(); i++)
    {
        double  input = audio.samples[i];



        highOut     = highAlpha * (highOut + input - priorInput);
        priorInput  = input;
        lowOut      = useLowPass ? lowOut + lowAlpha * (highOut - lowOut) : highOut;
        filtered[i] = lowOut;
    }
}
