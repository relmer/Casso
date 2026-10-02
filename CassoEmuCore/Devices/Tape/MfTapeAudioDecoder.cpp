#include "Pch.h"

#include "Devices/Tape/MfTapeAudioDecoder.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioDecoder::Decode
//
//  Media Foundation folds the channels to one and converts to float; the
//  sample rate stays the recording's own, so no resampling moves the edges.
//  A rate outside what tapes are read at is resampled to 44.1 kHz instead.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioDecoder::Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error)
{
    HRESULT                  hr         = S_OK;
    ComPtr<IMFSourceReader>  reader;
    uint32_t                 sampleRate = 0;
    bool                     isStarted  = false;
    bool                     isInRange  = false;



    audio = TapeAudio();

    hr = MFStartup (MF_VERSION, MFSTARTUP_LITE);
    CHRF (hr, error = "Media Foundation is not available.");
    isStarted = true;

    hr = CreateReader (bytes, reader);
    CHRF (hr, error = "The MP3 could not be opened.");

    hr = GetNativeRate (reader.Get(), sampleRate);
    CHRF (hr, error = "The MP3 has no audio stream.");

    isInRange  = sampleRate >= TapeAudio::kMinSampleRate && sampleRate <= TapeAudio::kMaxSampleRate;
    sampleRate = isInRange ? sampleRate : kFallbackSampleRate;

    hr = SelectMonoFloat (reader.Get(), sampleRate);
    CHRF (hr, error = "The MP3 could not be decoded.");

    hr = ReadAllSamples (reader.Get(), audio.samples);
    CHRF (hr, error = "The MP3 could not be decoded.");

    audio.sampleRate = sampleRate;

Error:
    reader.Reset();

    if (isStarted)
    {
        MFShutdown();
    }

    if (FAILED (hr))
    {
        audio = TapeAudio();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioDecoder::CreateReader
//
//  A source reader over an in-memory copy of the bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioDecoder::CreateReader (std::span<const Byte> bytes, ComPtr<IMFSourceReader> & reader)
{
    HRESULT                hr      = S_OK;
    ComPtr<IStream>        stream;
    ComPtr<IMFByteStream>  byteStream;
    ULONG                  written = 0;
    LARGE_INTEGER          origin  = {};



    hr = CreateStreamOnHGlobal (nullptr, TRUE, &stream);
    CHR (hr);

    hr = stream->Write (bytes.data(), (ULONG) bytes.size(), &written);
    CHR (hr);

    hr = stream->Seek (origin, STREAM_SEEK_SET, nullptr);
    CHR (hr);

    hr = MFCreateMFByteStreamOnStream (stream.Get(), &byteStream);
    CHR (hr);

    hr = MFCreateSourceReaderFromByteStream (byteStream.Get(), nullptr, &reader);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioDecoder::GetNativeRate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioDecoder::GetNativeRate (IMFSourceReader * reader, uint32_t & sampleRate)
{
    HRESULT               hr   = S_OK;
    ComPtr<IMFMediaType>  native;
    UINT32                rate = 0;



    hr = reader->GetNativeMediaType (MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &native);
    CHR (hr);

    hr = native->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    CHR (hr);

    sampleRate = rate;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioDecoder::SelectMonoFloat
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioDecoder::SelectMonoFloat (IMFSourceReader * reader, uint32_t sampleRate)
{
    constexpr UINT32      kBytesPerFrame = kFloatBits / 8;
    HRESULT               hr             = S_OK;
    ComPtr<IMFMediaType>  pcmType;



    hr = MFCreateMediaType (&pcmType);
    CHR (hr);

    hr = pcmType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    CHR (hr);

    hr = pcmType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_Float);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 1);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, kFloatBits);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, kBytesPerFrame);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, sampleRate * kBytesPerFrame);
    CHR (hr);

    hr = reader->SetCurrentMediaType (MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmType.Get());
    CHR (hr);

    hr = reader->SetStreamSelection (MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioDecoder::ReadAllSamples
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioDecoder::ReadAllSamples (IMFSourceReader * reader, std::vector<float> & samples)
{
    HRESULT                 hr        = S_OK;
    ComPtr<IMFSample>       sample;
    ComPtr<IMFMediaBuffer>  buffer;
    DWORD                   flags     = 0;
    DWORD                   length    = 0;
    BYTE                  * data      = nullptr;
    const float           * floats    = nullptr;
    bool                    isAtEnd   = false;



    samples.clear();

    while (!isAtEnd)
    {
        sample.Reset();
        buffer.Reset();
        flags = 0;

        hr = reader->ReadSample (MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        CHR (hr);

        isAtEnd = (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0;

        if (isAtEnd || sample.Get() == nullptr)
        {
            continue;
        }

        hr = sample->ConvertToContiguousBuffer (&buffer);
        CHR (hr);

        hr = buffer->Lock (&data, nullptr, &length);
        CHR (hr);

        floats = reinterpret_cast<const float *> (data);
        samples.insert (samples.end(), floats, floats + length / sizeof (float));

        hr = buffer->Unlock();
        CHR (hr);
    }

Error:
    return hr;
}
