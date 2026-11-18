#pragma once

#include "Pch.h"

#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeRecordScanner.h"
#include "Machines/Apple2/Common/IVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume
//
//  A tape recording, WAV, AIFF, MP3, FLAC or Ogg Vorbis, read as a folder of
//  the files on it. The buffer is the recording file's bytes. It is decoded
//  once, when the volume is made, through the same signal decoder and record
//  scanner the emulator's tape deck uses.
//
//  Records become files the way BASIC's SAVE writes them. A 3-byte record
//  followed by one of its length plus one is an Applesoft program, without
//  the byte after it. A 2-byte record followed by one of its length is an
//  Integer BASIC program. Any other record is a binary, whose load address
//  the tape doesn't store. Files have the names FILE1, FILE2 and so on, and
//  the ProDOS types BAS, INT and BIN, so they're copied out by those types'
//  rules. A file whose checksum fails is listed with that reason and can't
//  be read. The volume is read-only.
//
//  Addressing for the integrity pass is one unit per file.
//
////////////////////////////////////////////////////////////////////////////////

class TapeRecordingVolume : public IVolume
{
public:
    static constexpr Byte  kTypeBinary    = 0x06;
    static constexpr Byte  kTypeInteger   = 0xFA;
    static constexpr Byte  kTypeApplesoft = 0xFC;
    static constexpr Word  kApplesoftLoad = 0x0801;

    //  The buffer is accessed by reference and isn't modified. MP3 and FLAC
    //  are decoded by Media Foundation.
    explicit TapeRecordingVolume (const vector<Byte> & bytes);
    TapeRecordingVolume (const vector<Byte> & bytes, ITapeAudioDecoder & decoder);

    //  Whether the bytes are a recording of a kind the tape deck reads.
    static bool  IsFound (const vector<Byte> & bytes);

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  Delete    (const FilePath  & path,
                        vector<Byte>    & outBuffer,
                        DeleteOutcome   & outOutcome) const override;

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  Rename    (const FilePath     & from,
                        const std::string  & to,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  SetAttributes (const FilePath        & path,
                            const FileAttributes  & attributes,
                            vector<Byte>          & outBuffer) const override;

private:
    //  One file found on the tape.
    struct TapeFile
    {
        std::string        name;
        Byte               type    = kTypeBinary;
        std::vector<Byte>  bytes;
        bool               isValid = false;
    };

    static constexpr HRESULT  kReadOnly          = HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
    static constexpr size_t   kApplesoftHeader   = 3;
    static constexpr size_t   kIntegerHeader     = 2;
    static constexpr int      kByteShift         = 8;

    static void  GroupRecords (const std::vector<TapeRecord> & records, std::vector<TapeFile> & outFiles);
    static bool  TryFindFile  (const std::vector<TapeFile> & files, const std::string & name, size_t & outIndex);

    void  Decode (ITapeAudioDecoder & decoder);

    const vector<Byte> &   m_bytes;
    std::vector<TapeFile>  m_files;
    std::string            m_error;
};
