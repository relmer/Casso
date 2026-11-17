#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/EmulatedCassetteAudio.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/Audio/AdtProAudioByteStream.h"
#include "CassoLink/CassoLinkHost.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProAudioScenarioTests
//
//  ADTPro 2.1.0's audio client, booted from ADTPro's disk in an emulated //e
//  and started with its menu's Audio choice, served by CassoLinkHost over an
//  AdtProAudioByteStream wired to the machine's cassette ports. The client
//  sends a disk to the PC and receives one from it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkAdtProAudioScenarioTests)
{
public:
    static constexpr uint64_t  kClockHz      = 1'020'484;
    static constexpr int       kDiskSlot     = 6;
    static constexpr int       kBootSeconds  = 25;
    static constexpr int       kStartSeconds = 6;
    static constexpr double    kPromptSecond = 8.0;
    static constexpr double    kDiskSeconds  = 1200.0;
    static constexpr DWORD     kServeMs      = 1000;

    struct Rig
    {
        TestMachine                               machine;
        std::shared_ptr<EmulatedCassetteAudio>    audio;
        std::shared_ptr<AdtProAudioByteStream>    stream;
        InMemoryCassoLinkFolder                   folder;
        std::unique_ptr<CassoLinkHost>            host;
        std::vector<Byte>                         adtPro;
        std::vector<Byte>                         dos33;

        //  ADTPro's disk in drive 1 and, in drive 2, the DOS 3.3 disk or a
        //  second copy of ADTPro's.
        explicit Rig (bool dos33InDrive2) :
            machine ("Apple2e")
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;



            AssertSucceeded (fixtures.OpenFixture ("CassoLink/ADTPRO-2.1.0.DSK", bytes));
            adtPro.assign (bytes.begin(), bytes.end());
            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            dos33.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (kDiskSlot, 0, "", DiskFormat::Dsk, adtPro));
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (kDiskSlot, 1, "", DiskFormat::Dsk, dos33InDrive2 ? dos33 : adtPro));

            machine.PowerCycle();
            machine.RunCycles (kBootSeconds * kClockHz);
            Assert::IsTrue (Screen().find ("(A)UDIO") != std::string::npos, TextEncoding::NarrowToWide (Screen()).c_str());

            Type ("A");
            machine.RunCycles (kStartSeconds * kClockHz);
            Assert::IsTrue (Screen().find ("(S)END") != std::string::npos, TextEncoding::NarrowToWide (Screen()).c_str());

            audio  = std::make_shared<EmulatedCassetteAudio> (machine);
            stream = std::make_shared<AdtProAudioByteStream> (audio);
            host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });
        }

        void  Type (const char * text)
        {
            for (size_t i = 0; text[i] != '\0'; i++)
            {
                machine.GetRefs().iieKeyboard->PressKey ((Byte) text[i]);
                machine.RunCycles (200'000ULL);
            }
        }

        std::string  Screen()
        {
            std::string  text;



            for (const std::string & row : TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400))
            {
                text += row + "\n";
            }

            return text;
        }

        //  The client served until `done`, for at most `seconds` of the
        //  machine's time.
        bool  ServeUntil (const std::function<bool()> & done, double seconds)
        {
            uint64_t  start = *machine.GetCpu()->GetBusCyclePtr();



            while (!done())
            {
                if ((double) (*machine.GetCpu()->GetBusCyclePtr() - start) > seconds * kClockHz)
                {
                    return false;
                }

                AssertSucceeded (host->ServeOne (kServeMs));
            }

            return true;
        }

        //  The client's answer to its first request, then its volume list,
        //  where slot 6, drive 2 is two rows below the RAM disk it starts on.
        void  PickDrive2()
        {
            ServeUntil ([this] () { return Screen().find ("VOLUMES CURRENTLY ON-LINE") != std::string::npos; }, kPromptSecond);
            Assert::IsTrue (Screen().find ("VOLUMES CURRENTLY ON-LINE") != std::string::npos, TextEncoding::NarrowToWide (Screen()).c_str());

            //  The list is drawn as the drives are scanned, and keys are
            //  read once it is whole.
            machine.RunCycles (kStartSeconds * kClockHz);
            Type ("\n\n\r");
        }

        std::vector<Byte>  ReadDrive2()
        {
            std::vector<Byte>  image;



            AssertSucceeded (NibblizationLayer::Denibblize (*machine.GetDiskStore().GetImage (kDiskSlot, 1), DiskFormat::Dsk, image));
            return image;
        }
    };


    //  Send: the client reads drive 2 and sends it as OUT, which is written
    //  in DOS order as OUT.dsk.
    TEST_METHOD (TheAudioClient_SendsADiskToThePc)
    {
        Rig   rig (true);
        bool  done = false;



        rig.Type ("S");
        rig.Type ("OUT\r");
        rig.PickDrive2();

        done = rig.ServeUntil ([&rig] () { return rig.folder.files.contains ("OUT.dsk"); }, kDiskSeconds);
        Assert::IsTrue (done, TextEncoding::NarrowToWide (rig.Screen()).c_str());
        Assert::IsTrue (rig.folder.files["OUT.dsk"] == rig.dos33, L"the image is drive 2's disk");
        Assert::IsTrue (rig.Screen().find ("COMPLETE") != std::string::npos, TextEncoding::NarrowToWide (rig.Screen()).c_str());
    }


    //  Receive: the client requests IN.DSK and writes it to drive 2, which
    //  had ADTPro's disk.
    TEST_METHOD (TheAudioClient_ReceivesADiskFromThePc)
    {
        Rig   rig (false);
        bool  done = false;



        rig.folder.files["IN.DSK"] = rig.dos33;

        rig.Type ("R");
        rig.Type ("IN.DSK\r");
        rig.PickDrive2();

        done = rig.ServeUntil ([&rig] () { return rig.Screen().find ("COMPLETE") != std::string::npos; }, kDiskSeconds);
        Assert::IsTrue (done, TextEncoding::NarrowToWide (rig.Screen()).c_str());
        Assert::IsTrue (rig.ReadDrive2() == rig.dos33, L"drive 2 has the PC's image");
    }
};
