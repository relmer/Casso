#include "Pch.h"

#include "Audio/AudioFileDecoder.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  AudioFileDecoder::DecodeToMono
//
//  On any failure `outSamples` is empty, which is silent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AudioFileDecoder::DecodeToMono (const wchar_t * path, uint32_t targetSampleRate, vector<float> & outSamples)
{
    HRESULT                  hr         = S_OK;
    ComPtr<IMFSourceReader>  reader;
    ComPtr<IMFMediaType>     pcmType;
    ComPtr<IMFSample>        sample;
    ComPtr<IMFMediaBuffer>   buffer;
    DWORD                    flags      = 0;
    DWORD                    bufLen     = 0;
    BYTE                   * bufPtr     = nullptr;
    const float            * srcFloat   = nullptr;
    DWORD                    floatCount = 0;



    outSamples.clear();

    CBRAEx (path != nullptr && targetSampleRate != 0, E_INVALIDARG);

    hr = MFCreateSourceReaderFromURL (path, nullptr, &reader);
    CHR (hr);

    hr = MFCreateMediaType (&pcmType);
    CHR (hr);

    hr = pcmType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    CHR (hr);

    hr = pcmType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_Float);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 1);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, targetSampleRate);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, targetSampleRate * 4);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    CHR (hr);

    hr = reader->SetCurrentMediaType (MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmType.Get());
    CHR (hr);

    hr = reader->SetStreamSelection (MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    CHR (hr);

    for (;;)
    {
        sample.Reset();
        buffer.Reset();
        flags = 0;

        hr = reader->ReadSample (MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        CHR (hr);

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            break;
        }

        if (sample.Get() == nullptr)
        {
            continue;
        }

        hr = sample->ConvertToContiguousBuffer (&buffer);
        CHR (hr);

        hr = buffer->Lock (&bufPtr, nullptr, &bufLen);
        CHR (hr);

        srcFloat   = reinterpret_cast<const float *> (bufPtr);
        floatCount = bufLen / sizeof (float);

        outSamples.insert (outSamples.end(), srcFloat, srcFloat + floatCount);

        hr = buffer->Unlock();
        CHR (hr);
    }

Error:
    if (FAILED (hr))
    {
        outSamples.clear();
    }

    return hr;
}





