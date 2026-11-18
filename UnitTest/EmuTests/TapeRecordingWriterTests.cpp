#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "Devices/Tape/MfTapeAudioDecoder.h"
#include "Devices/Tape/MfTapeAudioEncoder.h"
#include "Devices/Tape/OggVorbisCodec.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/TapeRecordScanner.h"
#include "Devices/Tape/TapeRecordingWriter.h"
#include "Devices/Tape/TapeSignalDecoder.h"
#include "Devices/Tape/WavCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingWriterTests
//
//  Recordings TapeRecordingWriter renders, read back through the decoder and
//  record scanner the emulator's tape deck uses: every record is found with
//  its bytes and checksum, from WAV, MP3 and Ogg Vorbis. Loading them in an
//  emulated Apple through its ROM is in ScenarioTests/TapeRecordingTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeRecordingWriterTests)
{
public:

    static std::vector<Byte> MakeBytes (size_t count, Byte seed)
    {
        std::vector<Byte>  bytes (count);



        for (size_t i = 0; i < count; i++)
        {
            bytes[i] = (Byte) (i * 37 + seed);
        }

        return bytes;
    }


    static std::vector<TapeRecordingFile> MakeFiles()
    {
        TapeRecordingFile  applesoft;
        TapeRecordingFile  integer;
        TapeRecordingFile  binary;



        applesoft.name  = "HELLO";
        applesoft.kind  = TapeFileKind::Applesoft;
        applesoft.bytes = MakeBytes (40, 3);

        integer.name    = "GAME";
        integer.kind    = TapeFileKind::IntegerBasic;
        integer.bytes   = MakeBytes (25, 9);

        binary.name        = "PICTURE";
        binary.kind        = TapeFileKind::Binary;
        binary.bytes       = MakeBytes (300, 1);
        binary.loadAddress = 0x0800;

        return { applesoft, integer, binary };
    }


    //  The records every file is saved as, in order.
    static std::vector<std::vector<Byte>> GetAllRecords (const std::vector<TapeRecordingFile> & files)
    {
        std::vector<std::vector<Byte>>  all;



        for (const TapeRecordingFile & file : files)
        {
            std::vector<std::vector<Byte>>  records;

            TapeRecordingWriter::GetRecords (file, records);
            all.insert (all.end(), records.begin(), records.end());
        }

        return all;
    }


    static void CheckScansBack (const TapeAudio & audio, const std::vector<std::vector<Byte>> & expected, const wchar_t * pszFormat)
    {
        TapeSignal               signal;
        std::vector<TapeRecord>  records;



        TapeSignalDecoder::Decode (audio, signal);
        TapeRecordScanner::Scan   (signal.transitions, signal.sampleRate, records);

        Assert::AreEqual (expected.size(), records.size(), pszFormat);

        for (size_t i = 0; i < records.size(); i++)
        {
            Assert::IsTrue (records[i].isValid, pszFormat);
            Assert::IsTrue (records[i].bytes == expected[i], pszFormat);
        }
    }


    static void CheckFormat (TapeRecordingFormat format, const std::wstring & pszFormat)
    {
        std::vector<TapeRecordingFile>  files    = MakeFiles();
        TapeAudio                       audio;
        std::string                     sidecar;
        std::vector<Byte>               bytes;
        std::string                     error;
        MfTapeAudioEncoder              encoder;
        MfTapeAudioDecoder              decoder;
        TapeImage                       image;
        std::vector<TapeRecord>         records;
        std::vector<std::vector<Byte>>  expected = GetAllRecords (files);
        std::wstring                    found;



        TapeRecordingWriter::Render (files, audio, sidecar);

        AssertSucceeded (TapeRecordingWriter::Encode (audio, format, encoder, bytes, error));
        AssertSucceeded (TapeImageLoader::Load (bytes, "recording", true, decoder, image, error));

        TapeRecordScanner::Scan (image.signal.transitions, image.signal.sampleRate, records);

        for (size_t i = 0; i < records.size(); i++)
        {
            found += std::format (L" [{} bytes, {}]", records[i].bytes.size(), records[i].isValid ? L"valid" : L"bad checksum");
        }

        Assert::AreEqual (expected.size(), records.size(), (pszFormat + found).c_str());

        for (size_t i = 0; i < records.size(); i++)
        {
            Assert::IsTrue (records[i].isValid,              (pszFormat + found).c_str());
            Assert::IsTrue (records[i].bytes == expected[i], (pszFormat + found).c_str());
        }
    }


    TEST_METHOD (GetRecords_AreWhatBasicsSaveAndTheMonitorsWriteSave)
    {
        std::vector<TapeRecordingFile>  files     = MakeFiles();
        std::vector<std::vector<Byte>>  applesoft;
        std::vector<std::vector<Byte>>  integer;
        std::vector<std::vector<Byte>>  binary;
        std::vector<Byte>               padded    = files[0].bytes;



        TapeRecordingWriter::GetRecords (files[0], applesoft);
        TapeRecordingWriter::GetRecords (files[1], integer);
        TapeRecordingWriter::GetRecords (files[2], binary);

        padded.push_back (0);

        Assert::IsTrue (applesoft == std::vector<std::vector<Byte>> { { 40, 0, 0 }, padded });
        Assert::IsTrue (integer   == std::vector<std::vector<Byte>> { { 25, 0 }, files[1].bytes });
        Assert::IsTrue (binary    == std::vector<std::vector<Byte>> { files[2].bytes });
    }


    TEST_METHOD (Render_EveryRecordScansBackWithItsBytes)
    {
        std::vector<TapeRecordingFile>  files = MakeFiles();
        TapeAudio                       audio;
        std::string                     sidecar;



        TapeRecordingWriter::Render (files, audio, sidecar);

        Assert::AreEqual (TapeRecordingWriter::kSampleRate, audio.sampleRate);
        CheckScansBack (audio, GetAllRecords (files), L"rendered");
    }


    TEST_METHOD (Render_TheSidecarHasALinePerFile)
    {
        std::vector<TapeRecordingFile>  files = MakeFiles();
        TapeAudio                       audio;
        std::string                     sidecar;



        TapeRecordingWriter::Render (files, audio, sidecar);

        Assert::IsTrue (sidecar.starts_with ("00:00.5  HELLO  Applesoft  40 bytes  $0801  LOAD\r\n"), TextEncoding::NarrowToWide (sidecar).c_str());
        Assert::IsTrue (sidecar.find ("  GAME  Integer BASIC  25 bytes  below HIMEM  LOAD\r\n") != std::string::npos);
        Assert::IsTrue (sidecar.find ("  PICTURE  Binary  300 bytes  $0800  CALL -151, then 0800.092BR\r\n") != std::string::npos);

        //  Two Applesoft records, two Integer BASIC records and a gap before
        //  the binary, which starts after 45 seconds.
        Assert::IsTrue (sidecar.find ("\r\n00:4") != std::string::npos, TextEncoding::NarrowToWide (sidecar).c_str());
    }


    TEST_METHOD (Wav_ReadsBack)
    {
        CheckFormat (TapeRecordingFormat::Wav, L"WAV");
    }


    TEST_METHOD (Ogg_ReadsBack)
    {
        CheckFormat (TapeRecordingFormat::Ogg, L"Ogg Vorbis");
    }


    TEST_METHOD (Mp3_ReadsBack)
    {
        CheckFormat (TapeRecordingFormat::Mp3, L"MP3");
    }


    TEST_METHOD (Ogg_IsRecognizedAndAnythingElseIsNot)
    {
        std::vector<Byte>  wav;
        std::vector<Byte>  ogg;
        std::string        error;
        TapeAudio          audio;
        TapeAudio          decoded;



        audio.sampleRate = 44100;
        audio.samples.assign (44100, 0.25f);

        AssertSucceeded (OggVorbisCodec::Encode (audio, ogg, error));
        WavCodec::Encode (audio, wav);

        Assert::IsTrue  (OggVorbisCodec::IsOgg (ogg));
        Assert::IsFalse (OggVorbisCodec::IsOgg (wav));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), OggVorbisCodec::Decode (wav, decoded, error));
        Assert::IsTrue   (TapeImageLoader::IsTapeFileExtension (L"C:\\tapes\\game.OGG"));
    }
};
