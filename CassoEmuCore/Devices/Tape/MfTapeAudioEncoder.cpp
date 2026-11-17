#include "Pch.h"

#include "Devices/Tape/MfTapeAudioEncoder.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioEncoder::EncodeMp3
//
//  A sink writer over an in-memory stream, with the MP3 container type
//  stated, since there is no file name for Media Foundation to go by.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioEncoder::EncodeMp3 (const TapeAudio & audio, std::vector<Byte> & bytes, std::string & error)
{
    HRESULT                hr        = S_OK;
    bool                   isStarted = false;
    ComPtr<IStream>        stream;
    ComPtr<IMFByteStream>  byteStream;
    ComPtr<IMFAttributes>  attributes;
    ComPtr<IMFSinkWriter>  writer;
    ComPtr<IMFMediaType>   mp3Type;
    ComPtr<IMFMediaType>   pcmType;
    DWORD                  index     = 0;



    bytes.clear();

    hr = MFStartup (MF_VERSION, MFSTARTUP_LITE);
    CHRF (hr, error = "Media Foundation is not available.");
    isStarted = true;

    hr = FindOutputType (audio.sampleRate, mp3Type);
    CHRF (hr, error = "Windows has no MP3 encoder for mono audio at this sample rate.");

    hr = CreatePcmType (audio.sampleRate, pcmType);
    CHR (hr);

    hr = CreateStreamOnHGlobal (nullptr, TRUE, &stream);
    CHR (hr);

    hr = MFCreateMFByteStreamOnStream (stream.Get(), &byteStream);
    CHR (hr);

    hr = MFCreateAttributes (&attributes, 1);
    CHR (hr);

    hr = attributes->SetGUID (MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MP3);
    CHR (hr);

    hr = MFCreateSinkWriterFromURL (nullptr, byteStream.Get(), attributes.Get(), &writer);
    CHRF (hr, error = "The MP3 writer could not be created.");

    hr = writer->AddStream (mp3Type.Get(), &index);
    CHR (hr);

    hr = writer->SetInputMediaType (index, pcmType.Get(), nullptr);
    CHRF (hr, error = "The MP3 encoder doesn't take this audio.");

    hr = writer->BeginWriting();
    CHR (hr);

    hr = WriteSamples (writer.Get(), index, audio);
    CHRF (hr, error = "The recording could not be encoded.");

    hr = writer->Finalize();
    CHRF (hr, error = "The recording could not be encoded.");

    writer.Reset();

    hr = ReadStream (stream.Get(), bytes);
    CHR (hr);

Error:
    writer.Reset();
    byteStream.Reset();

    if (isStarted)
    {
        MFShutdown();
    }

    if (FAILED (hr))
    {
        bytes.clear();

        if (error.empty())
        {
            error = "The recording could not be encoded as MP3.";
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioEncoder::FindOutputType
//
//  The encoder's mono type at the sample rate with the highest bit rate.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioEncoder::FindOutputType (uint32_t sampleRate, ComPtr<IMFMediaType> & outType)
{
    HRESULT                hr       = S_OK;
    ComPtr<IMFCollection>  types;
    DWORD                  count    = 0;
    UINT32                 bestRate = 0;
    bool                   found    = false;



    hr = MFTranscodeGetAudioOutputAvailableTypes (MFAudioFormat_MP3, MFT_ENUM_FLAG_ALL, nullptr, &types);
    CHR (hr);

    hr = types->GetElementCount (&count);
    CHR (hr);

    for (DWORD i = 0; i < count; i++)
    {
        ComPtr<IUnknown>      element;
        ComPtr<IMFMediaType>  type;
        UINT32                channels = 0;
        UINT32                rate     = 0;
        UINT32                bytesPer = 0;

        hr = types->GetElement (i, &element);
        CHR (hr);

        hr = element.As (&type);
        CHR (hr);

        //  A type without one of these has 0 here and isn't chosen.
        hr = type->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &channels);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = type->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = type->GetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bytesPer);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (channels == 1 && rate == sampleRate && bytesPer > bestRate)
        {
            outType  = type;
            bestRate = bytesPer;
            found    = true;
        }
    }

    hr = found ? S_OK : MF_E_TRANSFORM_TYPE_NOT_SET;
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioEncoder::CreatePcmType
//
//  16-bit mono PCM, what the encoder takes in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioEncoder::CreatePcmType (uint32_t sampleRate, ComPtr<IMFMediaType> & outType)
{
    HRESULT  hr = S_OK;



    hr = MFCreateMediaType (&outType);
    CHR (hr);

    hr = outType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    CHR (hr);

    hr = outType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
    CHR (hr);

    hr = outType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 1);
    CHR (hr);

    hr = outType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    CHR (hr);

    hr = outType->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, kPcmBits);
    CHR (hr);

    hr = outType->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, kBytesPerFrame);
    CHR (hr);

    hr = outType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, sampleRate * kBytesPerFrame);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioEncoder::WriteSamples
//
//  A tenth of a second at a time, each sample stamped with its time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioEncoder::WriteSamples (IMFSinkWriter * writer, DWORD stream, const TapeAudio & audio)
{
    HRESULT  hr = S_OK;



    for (size_t at = 0; at < audio.samples.size(); at += kFramesPerWrite)
    {
        size_t                  count  = std::min (kFramesPerWrite, audio.samples.size() - at);
        DWORD                   length = (DWORD) (count * kBytesPerFrame);
        ComPtr<IMFMediaBuffer>  buffer;
        ComPtr<IMFSample>       sample;
        BYTE                  * data   = nullptr;
        int16_t               * pcm    = nullptr;

        hr = MFCreateMemoryBuffer (length, &buffer);
        CHR (hr);

        hr = buffer->Lock (&data, nullptr, nullptr);
        CHR (hr);

        pcm = reinterpret_cast<int16_t *> (data);

        for (size_t i = 0; i < count; i++)
        {
            pcm[i] = (int16_t) std::lround (std::clamp (audio.samples[at + i], -1.0f, 1.0f) * kFullScale);
        }

        hr = buffer->Unlock();
        CHR (hr);

        hr = buffer->SetCurrentLength (length);
        CHR (hr);

        hr = MFCreateSample (&sample);
        CHR (hr);

        hr = sample->AddBuffer (buffer.Get());
        CHR (hr);

        hr = sample->SetSampleTime ((LONGLONG) at * kTicksPerSecond / (LONGLONG) audio.sampleRate);
        CHR (hr);

        hr = sample->SetSampleDuration ((LONGLONG) count * kTicksPerSecond / (LONGLONG) audio.sampleRate);
        CHR (hr);

        hr = writer->WriteSample (stream, sample.Get());
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MfTapeAudioEncoder::ReadStream
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MfTapeAudioEncoder::ReadStream (IStream * stream, std::vector<Byte> & bytes)
{
    HRESULT         hr     = S_OK;
    STATSTG         stat   = {};
    LARGE_INTEGER   origin = {};
    ULONG           read   = 0;



    hr = stream->Stat (&stat, STATFLAG_NONAME);
    CHR (hr);

    bytes.resize ((size_t) stat.cbSize.QuadPart);

    hr = stream->Seek (origin, STREAM_SEEK_SET, nullptr);
    CHR (hr);

    hr = stream->Read (bytes.data(), (ULONG) bytes.size(), &read);
    CHR (hr);

    bytes.resize (read);

Error:
    return hr;
}
