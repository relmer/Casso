#pragma once

#include "Devices/Tape/TapeSignalDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeFormat
//
////////////////////////////////////////////////////////////////////////////////

enum class TapeFormat
{
    Wav,
    Aiff,
    Mp3,
    Flac,
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeImage
//
//  An inserted recording: where it came from, what it was, and the level
//  transitions decoded from it. The decoded audio itself is not kept.
//  isWritable is true only for a WAV file that is not read-only, since WAV is
//  the only format Casso writes.
//
////////////////////////////////////////////////////////////////////////////////

struct TapeImage
{
    std::string  path;
    TapeFormat   format     = TapeFormat::Wav;
    TapeSignal   signal;
    bool         isWritable = false;
};
