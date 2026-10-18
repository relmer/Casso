#pragma once

#include "Devices/Tape/ITapeAudioDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioDecoder
//
//  Decodes compressed recordings (MP3 and FLAC) with Windows Media
//  Foundation, from bytes already in memory, to mono float samples at the
//  recording's own sample rate.

class MfTapeAudioDecoder : public ITapeAudioDecoder
{
public:
    HRESULT  Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error) override;

private:
    static constexpr uint32_t  kFallbackSampleRate = 44100;
    static constexpr uint32_t  kFloatBits          = 32;

    static HRESULT  CreateReader     (std::span<const Byte> bytes, ComPtr<IMFSourceReader> & reader);
    static HRESULT  GetNativeRate    (IMFSourceReader * reader, uint32_t & sampleRate);
    static HRESULT  SelectMonoFloat  (IMFSourceReader * reader, uint32_t sampleRate);
    static HRESULT  ReadAllSamples   (IMFSourceReader * reader, std::vector<float> & samples);
};
