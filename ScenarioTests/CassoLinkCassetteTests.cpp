#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/EmulatedCassetteAudio.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/Audio/CassetteByteStream.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCassetteScenarioTests
//
//  The link over the cassette ports of an emulated //e and ][+, in Casso's
//  audio encoding: the cassette build of the program, placed where the
//  monitor's tape read puts it, and the host on the test's thread over a
//  CassetteByteStream wired to the machine's cassette port.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkCassetteScenarioTests)
{
public:

    static constexpr uint64_t  kClockHz   = 1'020'484;
    static constexpr int       kDiskSlot  = 6;
    static constexpr int       kSectors   = 16;

    struct Rig
    {
        TestMachine                             machine;
        std::shared_ptr<EmulatedCassetteAudio>  audio;
        std::shared_ptr<CassetteByteStream>     stream;
        InMemoryCassoLinkFolder                 folder;
        std::unique_ptr<CassoLinkHost>          host;
        std::vector<Byte>                       disk;

        explicit Rig (const char * model, EmulatedCassetteAudio::Faults faults = EmulatedCassetteAudio::Faults(), uint32_t rate = 44100) :
            machine (model)
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;
            std::vector<Byte>     program;
            std::string           errors;
            Word                  at = LinkProgram::kProgramAddress;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            disk.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (kDiskSlot, 0, "", DiskFormat::Dsk, disk));

            machine.PowerCycle();
            machine.RunCycles (2 * kClockHz);

            AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::Cassette));

            for (Byte b : program)
            {
                machine.GetCpu()->WriteByte (at++, b);
            }

            machine.GetCpu()->SetPC (LinkProgram::kProgramAddress);

            audio  = std::make_shared<EmulatedCassetteAudio> (machine, rate, faults);
            stream = std::make_shared<CassetteByteStream> (audio);
            host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetProgramChip (LinkChip::Cassette);
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
