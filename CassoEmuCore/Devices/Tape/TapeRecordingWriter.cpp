#include "Pch.h"

#include "Devices/Tape/TapeRecordingWriter.h"
#include "Devices/Tape/OggVorbisCodec.h"
#include "Devices/Tape/WavCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter::GetRecords
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecordingWriter::GetRecords (const TapeRecordingFile & file, std::vector<std::vector<Byte>> & outRecords)
{
    size_t             length  = file.bytes.size();
    std::vector<Byte>  header  = { (Byte) (length & kLowByte), (Byte) ((length >> kByteShift) & kLowByte) };
    std::vector<Byte>  program = file.bytes;



    outRecords.clear();

    if (file.kind == TapeFileKind::Binary)
    {
        outRecords.push_back (file.bytes);
        return;
    }

    //  Applesoft's third byte is its LOCK flag, which runs the program
    //  after LOAD when bit 7 is set; it is written clear. Applesoft saves
    //  through the byte at PRGEND, one past the program.
    if (file.kind == TapeFileKind::Applesoft)
    {
        header.push_back (0);
        program.push_back (0);
    }

    outRecords.push_back (header);
    outRecords.push_back (program);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter::AddHalf
//
//  One half-cycle at the current level, then the level flips. A sample an
//  edge falls inside takes each level for its share of the sample, so the
//  edges are at their exact times between samples.
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecordingWriter::AddHalf (Synth & synth, double us)
{
    double  end  = synth.clock + us * (double) kSampleRate / kMicrosecondsPerSecond;
    size_t  last = (size_t) std::ceil (end);



    if (synth.samples.size() < last)
    {
        synth.samples.resize (last, 0.0f);
    }

    for (size_t k = (size_t) std::floor (synth.clock); k < last; k++)
    {
        double  from = std::max ((double) k, synth.clock);
        double  to   = std::min ((double) k + 1.0, end);

        synth.samples[k] += (float) (to - from) * synth.level;
    }

    synth.clock = end;
    synth.level = -synth.level;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter::AddSilence
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecordingWriter::AddSilence (Synth & synth, double seconds)
{
    synth.clock += seconds * (double) kSampleRate;
    synth.samples.resize ((size_t) std::ceil (synth.clock), 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter::AddRecord
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecordingWriter::AddRecord (Synth & synth, std::span<const Byte> bytes)
{
    long               halves   = std::lround (kLeaderSeconds * kMicrosecondsPerSecond / kLeaderHalfUs);
    Byte               checksum = kChecksumSeed;
    std::vector<Byte>  written (bytes.begin(), bytes.end());



    for (Byte value : bytes)
    {
        checksum ^= value;
    }

    written.push_back (checksum);

    for (long i = 0; i < halves; i++)
    {
        AddHalf (synth, kLeaderHalfUs);
    }

    AddHalf (synth, kSyncFirstHalfUs);
    AddHalf (synth, kSyncSecondHalfUs);

    for (Byte value : written)
    {
        for (int bit = 0; bit < kBitsPerByte; bit++)
        {
            double  half = ((value << bit) & kHighBit) != 0 ? kOneHalfUs : kZeroHalfUs;

            AddHalf (synth, half);
            AddHalf (synth, half);
        }
    }

    AddHalf (synth, kEndHalfUs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter::Describe
//
////////////////////////////////////////////////////////////////////////////////

std::string TapeRecordingWriter::Describe (const TapeRecordingFile & file, double startSeconds)
{
    int          minutes = (int) (startSeconds / kSecondsPerMinute);
    double       seconds = startSeconds - minutes * kSecondsPerMinute;
    size_t       length  = file.bytes.size();
    std::string  start   = std::format ("{:02}:{:04.1f}", minutes, seconds);
    std::string  line;



    switch (file.kind)
    {
        case TapeFileKind::Applesoft:
            line = std::format ("{}  {}  Applesoft  {} bytes  ${:04X}  LOAD", start, file.name, length, kApplesoftStart);
            break;

        case TapeFileKind::IntegerBasic:
            line = std::format ("{}  {}  Integer BASIC  {} bytes  below HIMEM  LOAD", start, file.name, length);
            break;

        default:
            line = std::format ("{}  {}  Binary  {} bytes  ${:04X}  CALL -151, then {:04X}.{:04X}R",
                                start, file.name, length, file.loadAddress,
                                file.loadAddress, (Word) (file.loadAddress + std::max ((size_t) 1, length) - 1));
            break;
    }

    return line + "\r\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter::Render
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecordingWriter::Render (const std::vector<TapeRecordingFile> & files, TapeAudio & outAudio, std::string & outSidecar)
{
    Synth  synth;



    outSidecar.clear();

    AddSilence (synth, kLeadInSeconds);

    for (size_t i = 0; i < files.size(); i++)
    {
        std::vector<std::vector<Byte>>  records;

        if (i > 0)
        {
            AddSilence (synth, kGapSeconds);
        }

        outSidecar += Describe (files[i], synth.clock / (double) kSampleRate);

        GetRecords (files[i], records);

        for (const std::vector<Byte> & record : records)
        {
            AddRecord (synth, record);
        }
    }

    AddSilence (synth, kLeadInSeconds);

    outAudio               = TapeAudio();
    outAudio.samples       = std::move (synth.samples);
    outAudio.sampleRate    = kSampleRate;
    outAudio.bitsPerSample = WavCodec::kWrittenBitsPerSample;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter::Encode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingWriter::Encode (const TapeAudio & audio, TapeRecordingFormat format, ITapeAudioEncoder & mp3Encoder, std::vector<Byte> & outBytes, std::string & outError)
{
    HRESULT  hr = S_OK;



    outBytes.clear();
    outError.clear();

    switch (format)
    {
        case TapeRecordingFormat::Wav:
            WavCodec::Encode (audio, outBytes);
            break;

        case TapeRecordingFormat::Mp3:
            hr = mp3Encoder.EncodeMp3 (audio, outBytes, outError);
            break;

        default:
            hr = OggVorbisCodec::Encode (audio, outBytes, outError);
            break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriter::GetExtension
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * TapeRecordingWriter::GetExtension (TapeRecordingFormat format)
{
    return (format == TapeRecordingFormat::Wav) ? L".wav" : (format == TapeRecordingFormat::Mp3) ? L".mp3" : L".ogg";
}
