#include "Pch.h"

#include "Devices/Tape/TapeChannelMixer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeChannelMixer::MixToMono
//
//  Mono passes through. Stereo is averaged, unless the average is far quieter
//  than the louder channel: that happens when one channel is wired inverted,
//  and averaging would cancel the signal, so the louder channel is used alone.
//
////////////////////////////////////////////////////////////////////////////////

void TapeChannelMixer::MixToMono (std::span<const float> interleaved, size_t channels, std::vector<float> & mono)
{
    size_t  frames   = 0;
    double  leftRms  = 0.0;
    double  rightRms = 0.0;
    double  mixRms   = 0.0;
    size_t  pick     = 0;
    bool    useMix   = true;



    mono.clear();

    if (channels <= 1)
    {
        mono.assign (interleaved.begin(), interleaved.end());
        return;
    }

    frames   = interleaved.size() / channels;
    leftRms  = GetRms (interleaved, channels, 0);
    rightRms = GetRms (interleaved, channels, 1);
    mixRms   = GetMixRms (interleaved);
    useMix   = mixRms >= kCancelRatio * max (leftRms, rightRms);
    pick     = leftRms >= rightRms ? 0 : 1;

    mono.resize (frames);

    for (size_t i = 0; i < frames; i++)
    {
        if (useMix)
        {
            mono[i] = (interleaved[i * channels] + interleaved[i * channels + 1]) * kAverage;
        }
        else
        {
            mono[i] = interleaved[i * channels + pick];
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeChannelMixer::GetRms
//
////////////////////////////////////////////////////////////////////////////////

double TapeChannelMixer::GetRms (std::span<const float> interleaved, size_t channels, size_t channel)
{
    size_t  frames = interleaved.size() / channels;
    double  sum    = 0.0;



    for (size_t i = 0; i < frames; i++)
    {
        double  sample = interleaved[i * channels + channel];

        sum += sample * sample;
    }

    return frames == 0 ? 0.0 : sqrt (sum / (double) frames);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeChannelMixer::GetMixRms
//
//  RMS of the two-channel average, over interleaved stereo frames.
//
////////////////////////////////////////////////////////////////////////////////

double TapeChannelMixer::GetMixRms (std::span<const float> interleaved)
{
    constexpr size_t  kStereo = 2;
    size_t            frames  = interleaved.size() / kStereo;
    double            sum     = 0.0;



    for (size_t i = 0; i < frames; i++)
    {
        double  sample = (interleaved[i * kStereo] + interleaved[i * kStereo + 1]) * kAverage;

        sum += sample * sample;
    }

    return frames == 0 ? 0.0 : sqrt (sum / (double) frames);
}
