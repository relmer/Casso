#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  TapeChannelMixer
//
//  Folds interleaved one- or two-channel samples down to the single channel a
//  cassette input receives.
//
////////////////////////////////////////////////////////////////////////////////

class TapeChannelMixer
{
public:
    static void MixToMono (std::span<const float> interleaved, size_t channels, std::vector<float> & mono);

private:
    static constexpr double  kCancelRatio = 0.5;   // a mix this much quieter than a channel means the channels cancel
    static constexpr float   kAverage     = 0.5f;

    static double  GetRms    (std::span<const float> interleaved, size_t channels, size_t channel);
    static double  GetMixRms (std::span<const float> interleaved);
};
