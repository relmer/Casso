#pragma once

#include "Pch.h"

#include "Devices/Tape/ITapeAudioEncoder.h"
#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeFileKind
//
//  How a file is saved to tape: a BASIC program as its SAVE writes it, a
//  length record and then the program, and anything else as one record
//  the Monitor's WRITE saves from memory.
//
////////////////////////////////////////////////////////////////////////////////

enum class TapeFileKind
{
    Applesoft,
    IntegerBasic,
    Binary,
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingFormat
//
////////////////////////////////////////////////////////////////////////////////

enum class TapeRecordingFormat
{
    Wav,
    Mp3,
    Ogg,
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingFile
//
//  One file for a recording: a BASIC program's bytes as they are in memory,
//  without DOS's length, or a binary's bytes and the address they load at.
//
////////////////////////////////////////////////////////////////////////////////

struct TapeRecordingFile
{
    std::string        name;
    TapeFileKind       kind        = TapeFileKind::Binary;
    std::vector<Byte>  bytes;
    Word               loadAddress = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter
//
//  Renders files as the Apple II's cassette output writes them, at 44.1 kHz
//  in mono, and encodes the recording as WAV, MP3 or Ogg Vorbis.
//
//  Each record is the Monitor's: about ten seconds of 770 Hz leader, a sync
//  cycle of a 200 us and a 250 us half, then each byte most significant bit
//  first, a 1 bit one cycle of 1 kHz and a 0 bit one of 2 kHz, and an XOR
//  checksum seeded with $FF. The Monitor's WRITE saves from a start address
//  to an end address inclusive, and BASIC's SAVE uses it twice. Applesoft
//  writes the program's length and a third byte, then the program and the
//  byte after it. Integer BASIC writes the length, then the program. A
//  half-cycle longer than any bit's cycle ends each record. The last bit then
//  has a closing edge, and the record ends before an MP3 encoder's ringing in
//  the silence after it, which would otherwise read as more 0 bits. The ROM's
//  READ stops after the bytes in its address range, so the half-cycle isn't
//  read.
//  Files are two seconds apart.
//
//  The sidecar has a line per file: its start time, name, kind, length,
//  load address and the command that loads it.
//
////////////////////////////////////////////////////////////////////////////////

class TapeRecordingWriter
{
public:
    static constexpr uint32_t  kSampleRate       = 44100;
    static constexpr double    kLeadInSeconds    = 0.5;
    static constexpr double    kLeaderSeconds    = 10.0;
    static constexpr double    kGapSeconds       = 2.0;
    static constexpr float     kLevel            = 0.8f;
    static constexpr double    kLeaderHalfUs     = 650.0;
    static constexpr double    kSyncFirstHalfUs  = 200.0;
    static constexpr double    kSyncSecondHalfUs = 250.0;
    static constexpr double    kZeroHalfUs       = 250.0;
    static constexpr double    kOneHalfUs        = 500.0;
    static constexpr double    kEndHalfUs        = 1500.0;
    static constexpr Byte      kChecksumSeed     = 0xFF;
    static constexpr Word      kApplesoftStart   = 0x0801;

    //  The records a file is saved as, without their checksums.
    static void     GetRecords (const TapeRecordingFile & file, std::vector<std::vector<Byte>> & outRecords);

    static void     Render     (const std::vector<TapeRecordingFile> & files, TapeAudio & outAudio, std::string & outSidecar);

    static HRESULT  Encode     (const TapeAudio        & audio,
                                TapeRecordingFormat      format,
                                ITapeAudioEncoder      & mp3Encoder,
                                std::vector<Byte>      & outBytes,
                                std::string            & outError);

    //  ".wav", ".mp3" or ".ogg".
    static const wchar_t *  GetExtension (TapeRecordingFormat format);

private:
    struct Synth
    {
        std::vector<float>  samples;
        double              clock = 0.0;
        float               level = kLevel;
    };

    static constexpr double  kMicrosecondsPerSecond = 1.0e6;
    static constexpr int     kBitsPerByte           = 8;
    static constexpr Byte    kHighBit               = 0x80;
    static constexpr int     kSecondsPerMinute      = 60;
    static constexpr int     kByteShift             = 8;
    static constexpr size_t  kLowByte               = 0xFF;

    static void         AddHalf     (Synth & synth, double us);
    static void         AddSilence  (Synth & synth, double seconds);
    static void         AddRecord   (Synth & synth, std::span<const Byte> bytes);
    static std::string  Describe    (const TapeRecordingFile & file, double startSeconds);
};
