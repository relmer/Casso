#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "AppleLink/AppleLinkCaptures.h"
#include "AppleLink/InMemoryAppleLinkFolder.h"
#include "AppleLink/AppleLinkHost.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkAdtProTests
//
//  ADTPro's own client, unmodified, against Casso's host. The client disk
//  boots in an emulated //c whose port 2 is joined to AppleLinkHost over the
//  serial bridge; the client is driven through its own menus, as a user
//  would; and the disks it sends and receives must be byte for byte the ones
//  ADTPro's own server made in the same exchanges (the captures in
//  UnitTest/Fixtures/AppleLink/).
//
//  The machine runs as fast as it can. The client's timeouts are counted in
//  its own cycles and the host replies within a millisecond, far inside them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleLinkAdtProTests)
{
public:

    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        InMemoryAppleLinkFolder               folder;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::unique_ptr<AppleLinkHost>        host;
        std::jthread                          serving;

        Rig()
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  client;

            AssertSucceeded (fixtures.OpenFixture ("AppleLink/ADTPRO-2.1.0.DSK", client));

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            machine.PowerCycle();

            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, std::vector<Byte> (client.begin(), client.end())));
            machine.GetRefs().diskController->SetExternalDisk (0, machine.GetDiskStore().GetImage (6, 0));

            InProcessByteStream::CreatePair (machineEnd, hostEnd);
            Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

            host    = std::make_unique<AppleLinkHost> (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });
            serving = std::jthread ([this] (std::stop_token stop)
            {
                HRESULT  hr = S_OK;

                while (!stop.stop_requested() && SUCCEEDED (hr))
                {
                    hr = host->ServeOne (20);
                }
            });
        }

        ~Rig()
        {
            serving.request_stop();
            serving.join();
        }

        std::string  Screen()
        {
            std::string  text;

            for (const std::string & row : TextScreenScraper::Scrape (machine)) { text += row + "\n"; }
            return text;
        }

        bool  RunUntil (const std::string & text, uint64_t budget)
        {
            uint64_t  spent = 0;

            while (Screen().find (text) == std::string::npos && spent < budget)
            {
                machine.RunCycles (17030);
                spent += 17030;
            }

            return spent < budget;
        }

        void  Type (const std::string & keys)
        {
            for (char key : keys)
            {
                machine.GetRefs().keyboard->PressKey ((Byte) key);
                machine.RunCycles (150000);
            }
        }

        //  Boot, choose the serial connection, and start the command.
        void  Start (char command)
        {
            Assert::IsTrue (RunUntil ("CONNECTION TYPE", 40'000'000), L"ADTPro's client boots");
            Type ("S");
            Assert::IsTrue (RunUntil ("(S)END", 10'000'000), L"and takes the serial connection");
            Type (std::string (1, command));
        }

        //  The volume list, then slot 6 drive 1.
        void  PickDriveOne()
        {
            Assert::IsTrue (RunUntil ("ADTPRO.2.1.0", 40'000'000), L"drive 1 is in the volume list");
            Type ("\x0A\x0D");
        }
    };


    TEST_METHOD (ASentDiskIsTheImageAdtProsServerWrote)
    {
        Rig                   rig;
        FixtureProvider       fixtures;
        std::vector<uint8_t>  source;

        AssertSucceeded (fixtures.OpenFixture ("AppleLink/ADTPRO-2.1.0.DSK", source));

        rig.Start ('S');
        rig.Type ("CASSOCAP.DSK\r");
        rig.PickDriveOne();

        Assert::IsTrue (rig.RunUntil ("COMPLETE", 400'000'000), L"the send completes");
        rig.machine.RunCycles (2'000'000);

        Assert::IsTrue (rig.folder.files["CASSOCAP.DSK"] == std::vector<Byte> (source.begin(), source.end()),
                        L"the image is the disk, as ADTPro's own server wrote it");
    }


    TEST_METHOD (AReceivedDiskIsTheImageTheServerHeld)
    {
        Rig                rig;
        std::vector<Byte>  written;

        AppleLinkHost::ToFileOrder ("VDRIVE-2.1.0.DSK", AppleLinkCaptures::RebuildImage ("adtpro-receive-disk.txt", '<', 280), rig.folder.files["VDRIVE-2.1.0.DSK"]);

        rig.Start ('R');
        rig.Type ("VDRIVE-2.1.0.DSK\r");
        rig.PickDriveOne();

        Assert::IsTrue (rig.RunUntil ("COMPLETE", 600'000'000), L"the receive completes");

        AssertSucceeded (rig.machine.GetDiskStore().GetImage (6, 0)->Serialize (written));
        Assert::IsTrue (written == rig.folder.files["VDRIVE-2.1.0.DSK"], L"drive 1 now has the image, sector for sector");
    }
};
