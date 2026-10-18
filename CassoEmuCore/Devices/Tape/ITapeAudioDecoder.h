#pragma once

#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ITapeAudioDecoder
//
//  Decodes a compressed recording (MP3 or FLAC) held in memory to mono
//  samples. WAV and AIFF do not come through here; they are parsed directly.
//
////////////////////////////////////////////////////////////////////////////////

class ITapeAudioDecoder
{
public:
    virtual ~ITapeAudioDecoder () = default;

    virtual HRESULT  Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NullTapeAudioDecoder
//
//  For hosts with no compressed-audio support: every decode fails, so an MP3
//  or FLAC tape is reported as unreadable rather than inserted silent.
//
////////////////////////////////////////////////////////////////////////////////

class NullTapeAudioDecoder : public ITapeAudioDecoder
{
public:
    HRESULT  Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error) override
    {
        UNREFERENCED_PARAMETER (bytes);

        audio = TapeAudio();
        error = "Compressed recordings cannot be decoded here.";
        return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
    }
};
