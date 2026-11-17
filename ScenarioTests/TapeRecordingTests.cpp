#include "Pch.h"

#include "EhmTestHelper.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "ApplesoftTokenizer.h"
#include "IntegerBasicTokenizer.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Tape/MfTapeAudioDecoder.h"
#include "Devices/Tape/MfTapeAudioEncoder.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/TapeRecordingVolume.h"
#include "Devices/Tape/TapeRecordingWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingTests
//
//  Recordings Casso Explorer's Convert to tape audio writes, played into an
//  emulated Apple's cassette input and loaded by its ROM: an Applesoft
//  program with LOAD and RUN, and a binary with the Monitor's READ, on a ][+
//  from WAV, MP3 and Ogg Vorbis, and an Integer BASIC program on a ][. Each
//  recording is also read back as a folder, and each file is identical to
//  the original.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeRecordingTests)
{
public:

    static constexpr uint64_t  kColdBootCycles = 5'000'000ULL;
    static constexpr uint64_t  kAfterCommand   = 2'000'000ULL;
    static constexpr uint64_t  kTailCycles     = 2'000'000ULL;
    static constexpr Word      kBinaryStart    = 0x2000;
    static constexpr size_t    kBinaryLength   = 256;
    static constexpr Byte      kCtrlB          = 0x02;



    static std::string GetScreen (TestMachine & machine)
    {
        std::string  screen;



        for (const std::string & line : TextScreenScraper::Scrape (machine))
        {
            screen += line + "\n";
        }

        return screen;
    }


    static void Boot (TestMachine & machine)
    {
        machine.PowerCycle();
        machine.RunCycles (kColdBootCycles);
    }


    //  The recording of one file, encoded in the format.
    static std::vector<Byte> MakeRecording (const TapeRecordingFile & file, TapeRecordingFormat format)
    {
        TapeAudio           audio;
        std::string         sidecar;
        std::vector<Byte>   bytes;
        std::string         error;
        MfTapeAudioEncoder  encoder;



        TapeRecordingWriter::Render ({ file }, audio, sidecar);
        AssertSucceeded (TapeRecordingWriter::Encode (audio, format, encoder, bytes, error));

        return bytes;
    }


    //  The recording in the deck, played to its end.
    static void Play (TestMachine & machine, const std::vector<Byte> & recording)
    {
        MfTapeAudioDecoder  decoder;
        TapeImage           image;
        std::string         error;
        double              seconds = 0.0;



        AssertSucceeded (TapeImageLoader::Load (recording, "recording", true, decoder, image, error));

        seconds = (double) image.signal.lengthSamples / image.signal.sampleRate;
        machine.GetTapeDeck().Insert (std::move (image));

        machine.GetTapeDeck().Play (*machine.GetCpu()->GetBusCyclePtr());
        machine.RunCycles ((uint64_t) (seconds * machine.GetConfig().clockSpeed) + kTailCycles);
        machine.GetTapeDeck().Update (*machine.GetCpu()->GetBusCyclePtr());
    }


    //  The recording's one file, read back as a folder.
    static void CheckReadsBack (const std::vector<Byte> & recording, const TapeRecordingFile & file, const wchar_t * pszContext)
    {
        TapeRecordingVolume  volume (recording);
        FilePayload          payload;



        AssertSucceeded (volume.Read (FilePath::Parse ("FILE1"), payload));
        Assert::IsTrue  (payload.bytes == file.bytes, pszContext);
    }


    static void LoadAndRunApplesoft (TapeRecordingFormat format, const wchar_t * pszContext)
    {
        TestMachine            machine ("Apple2Plus", TestMachine::Slots::Empty);
        TapeRecordingFile      file;
        ApplesoftListingError  listingError;
        std::vector<Byte>      recording;
        std::string            screen;



        file.name = "HELLO";
        file.kind = TapeFileKind::Applesoft;
        AssertSucceeded (ApplesoftTokenizer::Tokenize ("10 PRINT \"CASSO TAPE \";6*7\n20 END\n", file.bytes, listingError));

        recording = MakeRecording (file, format);

        Boot (machine);
        KeystrokeInjector::InjectLine (machine, "LOAD", kAfterCommand);
        Play (machine, recording);
        KeystrokeInjector::InjectLine (machine, "RUN", kAfterCommand);

        screen = GetScreen (machine);
        Assert::IsTrue (screen.find ("ERR") == std::string::npos,         (pszContext + TextEncoding::NarrowToWide ("\n" + screen)).c_str());
        Assert::IsTrue (screen.find ("CASSO TAPE 42") != std::string::npos, (pszContext + TextEncoding::NarrowToWide ("\n" + screen)).c_str());

        CheckReadsBack (recording, file, pszContext);
    }


    static void LoadBinary (TapeRecordingFormat format, const wchar_t * pszContext)
    {
        TestMachine        machine ("Apple2Plus", TestMachine::Slots::Empty);
        TapeRecordingFile  file;
        std::vector<Byte>  recording;
        int                mismatches = 0;



        file.name        = "PATTERN";
        file.kind        = TapeFileKind::Binary;
        file.loadAddress = kBinaryStart;

        for (size_t i = 0; i < kBinaryLength; i++)
        {
            file.bytes.push_back ((Byte) (i * 37 + 11));
        }

        recording = MakeRecording (file, format);

        Boot (machine);
        KeystrokeInjector::InjectLine (machine, "CALL -151", kAfterCommand);
        KeystrokeInjector::InjectLine (machine, "2000.20FFR", kAfterCommand);
        Play (machine, recording);

        for (size_t i = 0; i < kBinaryLength; i++)
        {
            mismatches += (machine.GetMemoryBus().ReadByte ((Word) (kBinaryStart + i)) == file.bytes[i]) ? 0 : 1;
        }

        Assert::AreEqual (0, mismatches, (pszContext + TextEncoding::NarrowToWide ("\n" + GetScreen (machine))).c_str());

        CheckReadsBack (recording, file, pszContext);
    }


    TEST_METHOD (Applesoft_LoadsAndRunsFromWav) { LoadAndRunApplesoft (TapeRecordingFormat::Wav, L"WAV"); }
    TEST_METHOD (Applesoft_LoadsAndRunsFromMp3) { LoadAndRunApplesoft (TapeRecordingFormat::Mp3, L"MP3"); }
    TEST_METHOD (Applesoft_LoadsAndRunsFromOgg) { LoadAndRunApplesoft (TapeRecordingFormat::Ogg, L"Ogg Vorbis"); }

    TEST_METHOD (Binary_LoadsFromWav) { LoadBinary (TapeRecordingFormat::Wav, L"WAV"); }
    TEST_METHOD (Binary_LoadsFromMp3) { LoadBinary (TapeRecordingFormat::Mp3, L"MP3"); }
    TEST_METHOD (Binary_LoadsFromOgg) { LoadBinary (TapeRecordingFormat::Ogg, L"Ogg Vorbis"); }


    //  Integer BASIC is in the ][, not the ][+.
    TEST_METHOD (IntegerBasic_LoadsAndRunsOnTheII)
    {
        TestMachine            machine ("Apple2", TestMachine::Slots::Empty);
        TapeRecordingFile      file;
        ApplesoftListingError  listingError;
        std::vector<Byte>      recording;
        std::string            screen;



        file.name = "GAME";
        file.kind = TapeFileKind::IntegerBasic;
        AssertSucceeded (IntegerBasicTokenizer::Tokenize ("10 PRINT \"INTEGER TAPE \";6*7\n20 END\n", file.bytes, listingError));

        recording = MakeRecording (file, TapeRecordingFormat::Wav);

        Boot (machine);
        KeystrokeInjector::InjectLine (machine, "CALL -151", kAfterCommand);
        KeystrokeInjector::InjectKey  (machine, kCtrlB);
        KeystrokeInjector::InjectLine (machine, "", kAfterCommand);
        KeystrokeInjector::InjectLine (machine, "LOAD", kAfterCommand);
        Play (machine, recording);
        KeystrokeInjector::InjectLine (machine, "RUN", kAfterCommand);

        screen = GetScreen (machine);
        Assert::IsTrue (screen.find ("ERR") == std::string::npos,            TextEncoding::NarrowToWide (screen).c_str());
        Assert::IsTrue (screen.find ("INTEGER TAPE 42") != std::string::npos, TextEncoding::NarrowToWide (screen).c_str());

        CheckReadsBack (recording, file, L"Integer BASIC");
    }
};
