#include "Pch.h"

#include "Devices/Tape/AiffCodec.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/WavCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeImageLoader::Load
//
//  On failure the image is left empty and error holds the reason.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeImageLoader::Load (
    std::span<const Byte>   bytes,
    const std::string     & path,
    bool                    isReadOnly,
    ITapeAudioDecoder     & compressedDecoder,
    TapeImage             & image,
    std::string           & error)
{
    HRESULT     hr     = S_OK;
    TapeAudio   audio;
    TapeImage   loaded;
    bool        isWav  = WavCodec::IsWav (bytes);
    bool        isAiff = AiffCodec::IsAiff (bytes);
    bool        isFlac = IsFlac (bytes);
    bool        isMp3  = !isFlac && IsMp3 (bytes);



    image = TapeImage();

    CBRFEx (isWav || isAiff || isMp3 || isFlac, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The file is not a WAV, AIFF, MP3, or FLAC recording.");

    if (isWav)
    {
        loaded.format = TapeFormat::Wav;
        hr = WavCodec::Decode (bytes, audio, error);
    }
    else if (isAiff)
    {
        loaded.format = TapeFormat::Aiff;
        hr = AiffCodec::Decode (bytes, audio, error);
    }
    else
    {
        // MP3 and FLAC both go to the platform decoder, which distinguishes
        // them by their headers.
        loaded.format = isFlac ? TapeFormat::Flac : TapeFormat::Mp3;
        hr = compressedDecoder.Decode (bytes, audio, error);
    }

    CHR (hr);

    TapeSignalDecoder::Decode (audio, loaded.signal);

    loaded.path       = path;
    loaded.isWritable = isWav && !isReadOnly;
    image             = std::move (loaded);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeImageLoader::IsMp3
//
//  An ID3 tag, or an MPEG audio frame sync (eleven set bits) at the start.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeImageLoader::IsMp3 (std::span<const Byte> bytes)
{
    constexpr size_t  kId3Length   = 3;
    constexpr Byte    kSyncByte    = 0xFF;
    constexpr Byte    kSyncMask    = 0xE0;
    bool              hasId3       = bytes.size() >= kId3Length && memcmp (bytes.data(), "ID3", kId3Length) == 0;
    bool              hasFrameSync = bytes.size() >= 2 && bytes[0] == kSyncByte && (bytes[1] & kSyncMask) == kSyncMask;



    return hasId3 || hasFrameSync;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeImageLoader::IsFlac
//
//  The "fLaC" marker every FLAC stream opens with.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeImageLoader::IsFlac (std::span<const Byte> bytes)
{
    constexpr size_t  kMarkerLength = 4;



    return bytes.size() >= kMarkerLength && memcmp (bytes.data(), "fLaC", kMarkerLength) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeImageLoader::IsTapeFileExtension
//
////////////////////////////////////////////////////////////////////////////////

bool TapeImageLoader::IsTapeFileExtension (const std::wstring & path)
{
    static constexpr const wchar_t * s_kExtensions[] = { L".wav", L".aif", L".aiff", L".aifc", L".mp3", L".flac" };
    std::wstring                     extension       = std::filesystem::path (path).extension().wstring();



    for (wchar_t & ch : extension)
    {
        ch = (wchar_t) towlower (ch);
    }

    for (const wchar_t * pszCandidate : s_kExtensions)
    {
        if (extension == pszCandidate)
        {
            return true;
        }
    }

    return false;
}




