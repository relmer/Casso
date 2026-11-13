#pragma once

#include "Pch.h"

#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ITapeAudioEncoder
//
//  Encodes mono samples to MP3 in memory. WAV and Ogg Vorbis don't come
//  through here; Casso writes them itself.
//
////////////////////////////////////////////////////////////////////////////////

class ITapeAudioEncoder
{
public:
    virtual ~ITapeAudioEncoder () = default;

    virtual HRESULT  EncodeMp3 (const TapeAudio & audio, std::vector<Byte> & bytes, std::string & error) = 0;
};
