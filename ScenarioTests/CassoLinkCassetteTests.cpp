#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/EmulatedCassetteAudio.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/Audio/CassetteByteStream.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/WavCodec.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCassetteScenarioTests
//
//  The link over the cassette ports of an emulated //e and ][+, in Casso's
//  audio encoding: the cassette build of the program, read from tape by the
//  monitor or placed where the monitor's read puts it, and the host on the
//  test's thread over a CassetteByteStream wired to the machine's cassette
//  port.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkCassetteScenarioTests)
{
public:

    static constexpr uint64_t  kClockHz   = 1'020'484;
    static constexpr int       kDiskSlot  = 6;
    static constexpr int       kSectors   = 16;

    enum class Start
    {
        Placed,
        FromTape,
    };

    struct Rig
    {
        TestMachine                             machine;
        std::shared_ptr<EmulatedCassetteAudio>  audio;
        std::shared_ptr<CassetteByteStream>     stream;
        InMemoryCassoLinkFolder                 folder;
        std::unique_ptr<CassoLinkHost>          host;
        std::vector<Byte>                       disk;

        explicit Rig (const char * model, EmulatedCassetteAudio::Faults faults = EmulatedCassetteAudio::Faults(), uint32_t rate = 44100, Start start = Start::Placed) :
            machine (model)
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            disk.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

            //  At the BASIC prompt, then the disk in drive 1.
            machine.PowerCycle();
            machine.RunCycles (2 * kClockHz);
            machine.GetCpu()->SetPC (0xE000);
            machine.RunCycles (2 * kClockHz);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (kDiskSlot, 0, "", DiskFormat::Dsk, disk));

            if (start == Start::FromTape)
            {
                LoadFromTape();
            }
            else
            {
                Place();
            }

            audio  = std::make_shared<EmulatedCassetteAudio> (machine, rate, faults);
            stream = std::make_shared<CassetteByteStream> (audio);
            host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetProgramChip (LinkChip::Cassette);
        }

        //  The program where the monitor's tape read puts it, and run.
        void  Place()
        {
            std::vector<Byte>  program;
            std::string        errors;
            Word               at = LinkProgram::kProgramAddress;



            AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::Cassette));

            for (Byte b : program)
            {
                machine.GetCpu()->WriteByte (at++, b);
            }

            machine.GetCpu()->SetPC (LinkProgram::kProgramAddress);
        }

        //  The monitor entered, the read command typed, and the tape's audio
        //  fed into the cassette input.
        void  LoadFromTape()
        {
            TapeAudio             audio;
            std::vector<Byte>     wav;
            std::string           command;
            NullTapeAudioDecoder  mp3;
            TapeImage             image;
            std::string           error;
            double                seconds = 0.0;



            AssertSucceeded (LinkAudioBootstrap::BuildCassetteLinkAudio (audio, command));
            WavCodec::Encode (audio, wav);
            Assert::AreEqual (S_OK, TapeImageLoader::Load (wav, "cassette.wav", false, mp3, image, error), TextEncoding::NarrowToWide (error).c_str());

            seconds = (double) image.signal.lengthSamples / image.signal.sampleRate;

            Type ("CALL -151\r");
            Type (command.c_str());

            machine.GetTapeDeck().Insert (std::move (image));
            machine.GetTapeDeck().Play (*machine.GetCpu()->GetBusCyclePtr());
            machine.RunCycles ((uint64_t) ((seconds + 1.0) * kClockHz));
            machine.GetTapeDeck().Update (*machine.GetCpu()->GetBusCyclePtr());
        }

        void  Type (const char * text)
        {
            for (size_t i = 0; text[i] != '\0'; i++)
            {
                machine.GetRefs().iieKeyboard->PressKey ((Byte) text[i]);
                machine.RunCycles (100'000ULL);
            }
        }

        std::vector<Byte>  ReadTrack (Byte track)
        {
            std::vector<Byte>  sectors (kSectors);
            std::vector<Byte>  data;
            CassoLinkStatus    status;



            for (size_t i = 0; i < sectors.size(); i++)
            {
                sectors[i] = (Byte) i;
            }

            AssertSucceeded (host->ReadSectors (1, track, sectors, data, status));
            Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, std::format (L"track {}", track).c_str());
            return data;
        }

        //  A track's 16 physical sectors from the image, in order.
        std::vector<Byte>  ImageTrack (Byte track)
        {
            std::vector<Byte>  data;



            for (int p = 0; p < kSectors; p++)
            {
                size_t  offset = (size_t) (track * kSectors + NibblizationLayer::GetDosFileIndexForPhysicalSector (p)) * 256;

                data.insert (data.end(), disk.begin() + (ptrdiff_t) offset, disk.begin() + (ptrdiff_t) offset + 256);
            }

            return data;
        }

        std::string  Screen()
        {
            std::string  text;



            for (const std::string & row : TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400))
            {
                text += row + " | ";
            }

            return text;
        }
    };


    TEST_METHOD (ATrack_IsReadOverTheCassettePorts_OfAnApple2e)
    {
        Rig  rig ("Apple2e");



        Assert::IsTrue (rig.ReadTrack (17) == rig.ImageTrack (17), TextEncoding::NarrowToWide (rig.Screen()).c_str());
    }


    //  The monitor reads the program from tape at its prompt, and the link
    //  then runs over the same cassette ports.
    TEST_METHOD (TheProgram_LoadsFromTape_AndReadsATrack)
    {
        Rig          rig    ("Apple2e", EmulatedCassetteAudio::Faults(), 44100, Start::FromTape);
        std::string  screen = rig.Screen();



        std::transform (screen.begin(), screen.end(), screen.begin(), [] (char c) { return (char) toupper ((unsigned char) c); });
        Assert::IsTrue (screen.find ("CASSETTE PORTS") != std::string::npos, TextEncoding::NarrowToWide (screen).c_str());
        Assert::IsTrue (rig.ReadTrack (17) == rig.ImageTrack (17));
    }


    //  A program damaged on its way from the tape fails its CRC, and the host
    //  fails the operation rather than load the program over the cassette
    //  ports, which the cassette build doesn't take.
    TEST_METHOD (ADamagedProgram_FailsItsCrc_AndIsntLoadedAgain)
    {
        Rig                       rig ("Apple2e");
        std::vector<Byte>         program;
        std::string               errors;
        std::vector<Byte>         sectors  = { 0 };
        std::vector<Byte>         data;
        std::vector<std::string>  activity;
        CassoLinkStatus           status;
        Word                      last     = 0;
        bool                      reported = false;



        AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::Cassette));
        last = (Word) (LinkProgram::kProgramAddress + program.size() - 1);

        rig.host->SetActivitySink ([&activity] (const std::string & text) { activity.push_back (text); });
        rig.machine.GetCpu()->WriteByte (last, (Byte) (program.back() ^ 0x55));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), rig.host->ReadSectors (1, 17, sectors, data, status));

        reported = std::any_of (activity.begin(), activity.end(), [] (const std::string & text) { return text.find ("failed its CRC check") != std::string::npos; });
        Assert::IsTrue (reported, L"the failed check is in the activity");
    }


    //  The whole disk read, and a track written and read back, on a ][+.
    TEST_METHOD (AWholeDisk_IsRead_AndATrackWritten_OnAnApple2Plus)
    {
        static constexpr Byte  kTracks = 35;
        static constexpr Byte  kTrack  = 20;

        Rig                rig ("Apple2Plus");
        std::vector<Byte>  sectors (kSectors);
        std::vector<Byte>  pattern ((size_t) kSectors * 256);
        CassoLinkStatus    status;



        for (Byte track = 0; track < kTracks; track++)
        {
            Assert::IsTrue (rig.ReadTrack (track) == rig.ImageTrack (track), std::format (L"track {}", track).c_str());
        }

        for (size_t i = 0; i < pattern.size(); i++)
        {
            pattern[i] = (Byte) (i * 7 + i / 256);
        }

        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        AssertSucceeded (rig.host->WriteSectors (1, kTrack, sectors, pattern, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::IsTrue (rig.ReadTrack (kTrack) == pattern, L"the track written over the cassette ports reads back");
    }


    //  The audio both ways inverted, the PC's input at 48 kHz, weak, through
    //  a low-pass, with hiss and an offset.
    TEST_METHOD (ATrack_IsRead_OverAPoorAudioPath)
    {
        EmulatedCassetteAudio::Faults  faults;



        faults.invertToApple   = true;
        faults.invertFromApple = true;
        faults.level           = 0.03f;
        faults.hiss            = 0.004f;
        faults.lowPassHz       = 5000.0;
        faults.offset          = 0.01f;

        Rig  rig ("Apple2e", faults, 48000);

        Assert::IsTrue (rig.ReadTrack (3) == rig.ImageTrack (3));
    }
};
