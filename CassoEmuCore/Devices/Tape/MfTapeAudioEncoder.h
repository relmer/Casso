#pragma once

#include "Pch.h"

#include "Devices/Tape/ITapeAudioEncoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioEncoder
//
//  Encodes MP3 with Windows Media Foundation's MP3 encoder, into memory. Of
//  the output types the encoder offers for mono at the recording's rate, the
//  one with the highest bit rate is used.
//
////////////////////////////////////////////////////////////////////////////////

class MfTapeAudioEncoder : public ITapeAudioEncoder
{
public:
    HRESULT  EncodeMp3 (const TapeAudio & audio, std::vector<Byte> & bytes, std::string & error) override;

private:
    static constexpr UINT32    kPcmBits        = 16;
    static constexpr UINT32    kBytesPerFrame  = kPcmBits / 8;
    static constexpr size_t    kFramesPerWrite = 4410;
    static constexpr LONGLONG  kTicksPerSecond = 10'000'000;
    static constexpr float     kFullScale      = 32767.0f;

    static HRESULT  FindOutputType (uint32_t sampleRate, ComPtr<IMFMediaType> & outType);
    static HRESULT  CreatePcmType  (uint32_t sampleRate, ComPtr<IMFMediaType> & outType);
    static HRESULT  WriteSamples   (IMFSinkWriter * writer, DWORD stream, const TapeAudio & audio);
    static HRESULT  ReadStream     (IStream * stream, std::vector<Byte> & bytes);
};
