#pragma once

#include "Pch.h"

#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OggVorbisCodec
//
//  Reads Ogg Vorbis recordings into mono samples and writes mono Ogg Vorbis,
//  with the libogg and libvorbis sources in External. Works on byte buffers
//  only; the caller does the file I/O.
//
////////////////////////////////////////////////////////////////////////////////

class OggVorbisCodec
{
public:
    //  The quality a recording is written at, on libvorbis's scale of -0.1
    //  to 1.0. At 0.6 the tape tones' edges decode as cleanly as from WAV.
    static constexpr float  kQuality = 0.6f;

    //  The "OggS" capture pattern every Ogg page begins with.
    static bool     IsOgg  (std::span<const Byte> bytes);

    static HRESULT  Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error);
    static HRESULT  Encode (const TapeAudio & audio, std::vector<Byte> & bytes, std::string & error);

private:
    struct Reader
    {
        std::span<const Byte>  bytes;
        size_t                 at = 0;
    };

    static constexpr size_t  kCapturePatternLength = 4;
    static constexpr int     kStreamSerial         = 0x43617373;
    static constexpr int     kEncodeFrames         = 1024;
    static constexpr int     kDecodeFrames         = 4096;

    static size_t  ReadCallback  (void * buffer, size_t size, size_t count, void * source);
    static int     SeekCallback  (void * source, int64_t offset, int whence);
    static long    TellCallback  (void * source);
};
