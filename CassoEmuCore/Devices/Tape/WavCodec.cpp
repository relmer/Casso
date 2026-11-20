#include "Pch.h"

#include "Devices/Tape/TapeChannelMixer.h"
#include "Devices/Tape/WavCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::IsWav
//
////////////////////////////////////////////////////////////////////////////////

bool WavCodec::IsWav (std::span<const Byte> bytes)
{
    return bytes.size() >= kRiffHeaderSize           &&
           memcmp (bytes.data(),     "RIFF", kTagLength) == 0 &&
           memcmp (bytes.data() + 8, "WAVE", kTagLength) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::Decode
//
//  Walks the chunk list for "fmt " and "data", skipping everything else. The
//  fmt chunk must be whole. A data chunk whose size exceeds what the file
//  holds is read to the end of the file, since recorders that were stopped
//  without finishing the header leave exactly that.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WavCodec::Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error)
{
    HRESULT                hr        = S_OK;
    Format                 format;
    std::span<const Byte>  data;
    size_t                 at        = kRiffHeaderSize;
    bool                   isWav     = false;
    bool                   hasFormat = false;
    bool                   hasData   = false;



    audio = TapeAudio();

    isWav = IsWav (bytes);
    CBRFEx (isWav, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The file is not a RIFF/WAVE file.");

    while (at + kChunkHeaderSize <= bytes.size())
    {
        uint32_t  size      = ReadLittle32 (bytes, at + 4);
        size_t    body      = at + kChunkHeaderSize;
        size_t    available = min ((size_t) size, bytes.size() - body);
        bool      isFmt     = memcmp (bytes.data() + at, "fmt ", kTagLength) == 0;
        bool      isData    = memcmp (bytes.data() + at, "data", kTagLength) == 0;
        bool      isWhole   = available == size;



        if (isFmt)
        {
            CBRFEx (isWhole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The WAVE format chunk runs past the end of the file.");

            hr = ReadFormat (bytes.subspan (body, size), format, error);
            CHR (hr);

            hasFormat = true;
        }
        else if (isData)
        {
            data    = bytes.subspan (body, available);
            hasData = true;
        }

        at = body + (size_t) size + (size & 1);
    }

    CBRFEx (hasFormat, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The WAVE file has no format chunk.");
    CBRFEx (hasData,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The WAVE file has no data chunk.");

    ReadSamples (data, format, audio);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::Encode
//
//  A canonical 44-byte header and 16-bit mono PCM at the audio's own rate.
//  Samples are clamped to full scale.
//
////////////////////////////////////////////////////////////////////////////////

void WavCodec::Encode (const TapeAudio & audio, std::vector<Byte> & bytes)
{
    constexpr double    kMaxPositive   = 32767.0;
    bool                isEightBit     = audio.bitsPerSample == kBits8;
    uint32_t            bits           = isEightBit ? kBits8 : kWrittenBitsPerSample;
    uint32_t            bytesPerSample = bits / kBits8;
    uint32_t            dataSize       = (uint32_t) (audio.samples.size() * bytesPerSample);
    size_t              header         = 0;
    const float       * in             = audio.samples.data();
    const float       * inEnd          = in + audio.samples.size();
    Byte              * out            = nullptr;



    bytes.clear();
    bytes.reserve (kCanonicalHeader + dataSize);

    WriteTag      (bytes, "RIFF");
    WriteLittle32 (bytes, kCanonicalHeader - kChunkHeaderSize + dataSize);
    WriteTag      (bytes, "WAVE");
    WriteTag      (bytes, "fmt ");
    WriteLittle32 (bytes, kFmtPcmSize);
    WriteLittle16 (bytes, kTagPcm);
    WriteLittle16 (bytes, 1);
    WriteLittle32 (bytes, audio.sampleRate);
    WriteLittle32 (bytes, audio.sampleRate * bytesPerSample);
    WriteLittle16 (bytes, (Word) bytesPerSample);
    WriteLittle16 (bytes, (Word) bits);
    WriteTag      (bytes, "data");
    WriteLittle32 (bytes, dataSize);

    //  8-bit WAV samples are unsigned, centered on 128. The samples are
    //  written through pointers: a checked subscript or push_back per sample
    //  costs most of a Debug build's time here.
    header = bytes.size();
    bytes.resize (header + dataSize);
    out = bytes.data() + header;

    for (; in < inEnd; in++)
    {
        if (isEightBit)
        {
            *out++ = (Byte) clamp (round ((double) *in * kFullScale8) + kCenter8, 0.0, kMaxUnsigned8);
        }
        else
        {
            Word  word = (Word) (int16_t) clamp (round ((double) *in * kFullScale16), -kFullScale16, kMaxPositive);

            *out++ = (Byte) (word & 0xFF);
            *out++ = (Byte) (word >> 8);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::ReadFormat
//
//  WAVE_FORMAT_EXTENSIBLE stores the real format tag in the first two bytes
//  of its sub-format GUID.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WavCodec::ReadFormat (std::span<const Byte> body, Format & format, std::string & error)
{
    HRESULT  hr              = S_OK;
    bool     isLongEnough    = body.size() >= kFmtMinSize;
    bool     isExtensible    = false;
    bool     hasSubFormat    = false;



    CBRFEx (isLongEnough, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The WAVE format chunk is too short.");

    format.tag           = ReadLittle16 (body, 0);
    format.channels      = ReadLittle16 (body, 2);
    format.sampleRate    = ReadLittle32 (body, 4);
    format.blockAlign    = ReadLittle16 (body, 12);
    format.bitsPerSample = ReadLittle16 (body, 14);

    isExtensible = format.tag == kTagExtensible;

    if (isExtensible)
    {
        hasSubFormat = body.size() >= kFmtExtensibleSize;
        CBRFEx (hasSubFormat, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The extensible WAVE format chunk is too short.");

        format.tag = ReadLittle16 (body, kSubFormatOffset);
    }

    hr = CheckFormat (format, error);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::CheckFormat
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WavCodec::CheckFormat (const Format & format, std::string & error)
{
    HRESULT         hr              = S_OK;
    bool            isPcm           = format.tag == kTagPcm;
    bool            isFloat         = format.tag == kTagFloat;
    bool            isKnownTag      = isPcm || isFloat;
    bool            isPcmDepth      = format.bitsPerSample == kBits8  || format.bitsPerSample == kBits16 ||
                                      format.bitsPerSample == kBits24 || format.bitsPerSample == kBits32;
    bool            isFloatDepth    = format.bitsPerSample == kBits32 || format.bitsPerSample == kBits64;
    bool            isKnownDepth    = isPcm ? isPcmDepth : isFloatDepth;
    bool            isKnownChannels = format.channels >= 1 && format.channels <= TapeAudio::kMaxChannels;
    bool            isKnownRate     = format.sampleRate >= TapeAudio::kMinSampleRate && format.sampleRate <= TapeAudio::kMaxSampleRate;
    bool            isBlockAligned  = format.blockAlign == format.channels * (format.bitsPerSample / kBits8);



    CBRFEx (isKnownTag,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("WAVE format {} is neither PCM nor IEEE float.", format.tag));
    CBRFEx (isKnownDepth,    HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("{}-bit WAVE samples are not supported.", format.bitsPerSample));
    CBRFEx (isKnownChannels, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("{}-channel WAVE files are not supported.", format.channels));
    CBRFEx (isKnownRate,     HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("A {} Hz sample rate is outside 8,000-96,000 Hz.", format.sampleRate));
    CBRFEx (isBlockAligned,  HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The WAVE block alignment does not match its channels and depth.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::ReadSamples
//
////////////////////////////////////////////////////////////////////////////////

void WavCodec::ReadSamples (std::span<const Byte> data, const Format & format, TapeAudio & audio)
{
    size_t              bytesPerSample = format.bitsPerSample / kBits8;
    size_t              frames         = data.size() / format.blockAlign;
    size_t              count          = frames * format.channels;
    std::vector<float>  interleaved (count);
    const Byte        * in             = data.data();
    float             * out            = interleaved.data();
    float             * outEnd         = out + count;



    //  A pointer walk: a checked subscript per byte costs most of a Debug
    //  build's time here.
    for (; out < outEnd; out++, in += bytesPerSample)
    {
        *out = ReadSample (in, format);
    }

    TapeChannelMixer::MixToMono (interleaved, format.channels, audio.samples);
    audio.sampleRate    = format.sampleRate;
    audio.bitsPerSample = (format.bitsPerSample == kBits8) ? kBits8 : kBits16;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::ReadSample
//
//  Integer PCM is unsigned at 8 bits and signed above, all scaled to -1..1.
//
////////////////////////////////////////////////////////////////////////////////

float WavCodec::ReadSample (const Byte * at, const Format & format)
{
    constexpr double  kScale8       = 128.0;
    constexpr double  kScale24      = 8388608.0;
    constexpr double  kScale32      = 2147483648.0;
    constexpr int     kSignShift24  = 8;
    float             floatSample   = 0.0f;
    double            doubleSample  = 0.0;
    int32_t           packed        = 0;



    if (format.tag == kTagFloat)
    {
        if (format.bitsPerSample == kBits32)
        {
            memcpy (&floatSample, at, sizeof (floatSample));
            return floatSample;
        }

        memcpy (&doubleSample, at, sizeof (doubleSample));
        return (float) doubleSample;
    }

    switch (format.bitsPerSample)
    {
    case kBits8:
        return (float) (((double) at[0] - kScale8) / kScale8);

    case kBits16:
        return (float) ((double) (int16_t) (at[0] | at[1] << 8) / kFullScale16);

    case kBits24:
        packed = (int32_t) ((uint32_t) at[0] << kSignShift24 | (uint32_t) at[1] << (kSignShift24 * 2) | (uint32_t) at[2] << (kSignShift24 * 3));
        return (float) ((double) (packed >> kSignShift24) / kScale24);

    default:
        return (float) ((double) (int32_t) ((uint32_t) at[0] | (uint32_t) at[1] << 8 | (uint32_t) at[2] << 16 | (uint32_t) at[3] << 24) / kScale32);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::ReadLittle32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WavCodec::ReadLittle32 (std::span<const Byte> bytes, size_t at)
{
    constexpr int  kByteBits = 8;



    return (uint32_t) bytes[at]                         | ((uint32_t) bytes[at + 1] << kByteBits) |
           ((uint32_t) bytes[at + 2] << (kByteBits * 2)) | ((uint32_t) bytes[at + 3] << (kByteBits * 3));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::ReadLittle16
//
////////////////////////////////////////////////////////////////////////////////

Word WavCodec::ReadLittle16 (std::span<const Byte> bytes, size_t at)
{
    constexpr int  kByteBits = 8;



    return (Word) (bytes[at] | (bytes[at + 1] << kByteBits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::WriteLittle32
//
////////////////////////////////////////////////////////////////////////////////

void WavCodec::WriteLittle32 (std::vector<Byte> & bytes, uint32_t value)
{
    constexpr int  kByteBits = 8;



    for (size_t i = 0; i < sizeof (value); i++)
    {
        bytes.push_back ((Byte) (value >> (kByteBits * (int) i)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::WriteLittle16
//
////////////////////////////////////////////////////////////////////////////////

void WavCodec::WriteLittle16 (std::vector<Byte> & bytes, Word value)
{
    constexpr int  kByteBits = 8;



    bytes.push_back ((Byte) value);
    bytes.push_back ((Byte) (value >> kByteBits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodec::WriteTag
//
////////////////////////////////////////////////////////////////////////////////

void WavCodec::WriteTag (std::vector<Byte> & bytes, const char * pszTag)
{
    bytes.insert (bytes.end(), pszTag, pszTag + kTagLength);
}
