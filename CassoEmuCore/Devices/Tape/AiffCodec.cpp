#include "Pch.h"

#include "Devices/Tape/AiffCodec.h"
#include "Devices/Tape/TapeChannelMixer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::IsAiff
//
////////////////////////////////////////////////////////////////////////////////

bool AiffCodec::IsAiff (std::span<const Byte> bytes)
{
    bool  isForm = bytes.size() >= kFormHeaderSize && memcmp (bytes.data(), "FORM", kTagLength) == 0;



    return isForm && (memcmp (bytes.data() + 8, "AIFF", kTagLength) == 0 ||
                      memcmp (bytes.data() + 8, "AIFC", kTagLength) == 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::Decode
//
//  Walks the chunk list for COMM and SSND. SSND's own offset field skips any
//  alignment padding ahead of the samples.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AiffCodec::Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error)
{
    HRESULT                hr        = S_OK;
    Format                 format;
    std::span<const Byte>  data;
    size_t                 at        = kFormHeaderSize;
    bool                   isAiff    = false;
    bool                   isAifc    = false;
    bool                   hasFormat = false;
    bool                   hasData   = false;



    audio = TapeAudio();

    isAiff = IsAiff (bytes);
    CBRFEx (isAiff, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The file is not an AIFF file.");

    isAifc = memcmp (bytes.data() + 8, "AIFC", kTagLength) == 0;

    while (at + kChunkHeaderSize <= bytes.size())
    {
        uint32_t  size      = ReadBig32 (bytes, at + 4);
        size_t    body      = at + kChunkHeaderSize;
        size_t    available = min ((size_t) size, bytes.size() - body);
        bool      isComm    = memcmp (bytes.data() + at, "COMM", kTagLength) == 0;
        bool      isSsnd    = memcmp (bytes.data() + at, "SSND", kTagLength) == 0;
        bool      isWhole   = available == size;



        if (isComm)
        {
            CBRFEx (isWhole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The AIFF COMM chunk runs past the end of the file.");

            hr = ReadComm (bytes.subspan (body, size), isAifc, format, error);
            CHR (hr);

            hasFormat = true;
        }
        else if (isSsnd && available >= kSsndHeaderSize)
        {
            size_t  offset = min ((size_t) ReadBig32 (bytes, body), available - kSsndHeaderSize);

            data    = bytes.subspan (body + kSsndHeaderSize + offset, available - kSsndHeaderSize - offset);
            hasData = true;
        }

        at = body + (size_t) size + (size & 1);
    }

    CBRFEx (hasFormat, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The AIFF file has no COMM chunk.");
    CBRFEx (hasData,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The AIFF file has no SSND chunk.");

    ReadSamples (data, format, audio);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::ReadComm
//
//  AIFF-C adds a compression type after the sample rate. Only "NONE"
//  (big-endian) and "sowt" (little-endian) hold plain PCM.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AiffCodec::ReadComm (std::span<const Byte> body, bool isAifc, Format & format, std::string & error)
{
    constexpr size_t  kRateOffset        = 8;
    constexpr size_t  kCompressionOffset = 18;
    HRESULT           hr                 = S_OK;
    size_t            needed             = isAifc ? kCommAifcSize : kCommSize;
    bool              isLongEnough       = body.size() >= needed;
    bool              isNone             = true;
    bool              isSowt             = false;



    CBRFEx (isLongEnough, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The AIFF COMM chunk is too short.");

    format.channels      = ReadBig16 (body, 0);
    format.frames        = ReadBig32 (body, 2);
    format.bitsPerSample = ReadBig16 (body, 6);
    format.sampleRate    = (uint32_t) llround (ReadExtended80 (body, kRateOffset));

    if (isAifc)
    {
        isNone = memcmp (body.data() + kCompressionOffset, "NONE", kTagLength) == 0;
        isSowt = memcmp (body.data() + kCompressionOffset, "sowt", kTagLength) == 0;
    }

    CBRFEx (isNone || isSowt, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The AIFF-C file is compressed; only uncompressed audio is supported.");

    format.isLittle = isSowt;

    hr = CheckFormat (format, error);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::CheckFormat
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AiffCodec::CheckFormat (const Format & format, std::string & error)
{
    HRESULT  hr              = S_OK;
    bool     isKnownDepth    = format.bitsPerSample == kBits8  || format.bitsPerSample == kBits16 ||
                               format.bitsPerSample == kBits24 || format.bitsPerSample == kBits32;
    bool     isKnownChannels = format.channels >= 1 && format.channels <= TapeAudio::kMaxChannels;
    bool     isKnownRate     = format.sampleRate >= TapeAudio::kMinSampleRate && format.sampleRate <= TapeAudio::kMaxSampleRate;
    bool     isLittleOk      = !format.isLittle || format.bitsPerSample == kBits16;



    CBRFEx (isKnownDepth,    HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("{}-bit AIFF samples are not supported.", format.bitsPerSample));
    CBRFEx (isKnownChannels, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("{}-channel AIFF files are not supported.", format.channels));
    CBRFEx (isKnownRate,     HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("A {} Hz sample rate is outside 8,000-96,000 Hz.", format.sampleRate));
    CBRFEx (isLittleOk,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "AIFF-C sowt audio must be 16-bit.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::ReadSamples
//
//  The frame count in COMM is used over the SSND length when it is smaller.
//
////////////////////////////////////////////////////////////////////////////////

void AiffCodec::ReadSamples (std::span<const Byte> data, const Format & format, TapeAudio & audio)
{
    size_t              bytesPerSample = format.bitsPerSample / kBits8;
    size_t              frameSize      = bytesPerSample * format.channels;
    size_t              frames         = min ((size_t) format.frames, data.size() / frameSize);
    size_t              count          = frames * format.channels;
    std::vector<float>  interleaved (count);



    for (size_t i = 0; i < count; i++)
    {
        interleaved[i] = ReadSample (data, i * bytesPerSample, format);
    }

    TapeChannelMixer::MixToMono (interleaved, format.channels, audio.samples);
    audio.sampleRate = format.sampleRate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::ReadSample
//
//  AIFF PCM is signed at every depth, 8 bits included.
//
////////////////////////////////////////////////////////////////////////////////

float AiffCodec::ReadSample (std::span<const Byte> data, size_t at, const Format & format)
{
    constexpr double  kScale8      = 128.0;
    constexpr double  kScale16     = 32768.0;
    constexpr double  kScale24     = 8388608.0;
    constexpr double  kScale32     = 2147483648.0;
    constexpr int     kByteBits    = 8;
    int32_t           packed       = 0;



    switch (format.bitsPerSample)
    {
    case kBits8:
        return (float) ((double) (int8_t) data[at] / kScale8);

    case kBits16:
        if (format.isLittle)
        {
            return (float) ((double) (int16_t) (data[at] | (data[at + 1] << kByteBits)) / kScale16);
        }

        return (float) ((double) (int16_t) ReadBig16 (data, at) / kScale16);

    case kBits24:
        packed = (int32_t) ((uint32_t) data[at] << (kByteBits * 3) | (uint32_t) data[at + 1] << (kByteBits * 2) | (uint32_t) data[at + 2] << kByteBits);
        return (float) ((double) (packed >> kByteBits) / kScale24);

    default:
        return (float) ((double) (int32_t) ReadBig32 (data, at) / kScale32);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::ReadExtended80
//
//  The sample rate is an 80-bit IEEE extended float: a sign bit, a 15-bit
//  exponent biased by 16383, and a 64-bit mantissa with an explicit integer
//  bit.
//
////////////////////////////////////////////////////////////////////////////////

double AiffCodec::ReadExtended80 (std::span<const Byte> bytes, size_t at)
{
    constexpr int       kExponentBias = 16383;
    constexpr int       kMantissaBits = 63;
    constexpr Word      kExponentMask = 0x7FFF;
    constexpr int       kHalfShift    = 32;
    Word                signExponent  = ReadBig16 (bytes, at);
    uint64_t            mantissa      = ((uint64_t) ReadBig32 (bytes, at + 2) << kHalfShift) | ReadBig32 (bytes, at + 6);
    int                 exponent      = (int) (signExponent & kExponentMask) - kExponentBias - kMantissaBits;
    double              value         = ldexp ((double) mantissa, exponent);



    return (signExponent & ~kExponentMask) != 0 ? -value : value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::ReadBig32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t AiffCodec::ReadBig32 (std::span<const Byte> bytes, size_t at)
{
    constexpr int  kByteBits = 8;



    return ((uint32_t) bytes[at] << (kByteBits * 3)) | ((uint32_t) bytes[at + 1] << (kByteBits * 2)) |
           ((uint32_t) bytes[at + 2] << kByteBits)   |  (uint32_t) bytes[at + 3];
}





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodec::ReadBig16
//
////////////////////////////////////////////////////////////////////////////////

Word AiffCodec::ReadBig16 (std::span<const Byte> bytes, size_t at)
{
    constexpr int  kByteBits = 8;



    return (Word) ((bytes[at] << kByteBits) | bytes[at + 1]);
}
