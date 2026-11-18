#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkProDosDisk.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/Host/CassoLinkVirtualDrives.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDriveScenarioTests
//
//  The CassoLink ProDOS disk, made from the ProDOS Users Disk, booted on an
//  emulated //c with port 2 on the link and a host serving two images as
//  its virtual drives on a thread of the test: the installer reports the drives'
//  slot, BASIC.SYSTEM catalogs both, a program saved to each is in that
//  image on the PC, and a catalog with the host stopped is an I/O error.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkVirtualDriveScenarioTests)
{
public:

    static constexpr DWORD         kLinkBaud  = 115200;
    static constexpr int           kPort      = 2;
    static constexpr const char *  kDrive1    = "C:\\link\\One.dsk";
    static constexpr const char *  kDrive2    = "C:\\link\\Two.dsk";
    static constexpr uint64_t      kNoReplyCap = 200'000'000ULL;   // each of ProDOS's calls, four waits of about three seconds



    //  A blank 140K ProDOS volume, in DOS 3.3 order as a .dsk has it.
    static std::vector<Byte>  MakeVolume (const std::string & name)
    {
        std::vector<Byte>  volume (NibblizationLayer::kImageByteSize, 0);



        AssertSucceeded (ProDosSkeleton::Write (volume, name));

        return volume;
    }


    //  The host on a thread, serving the drives until the rig ends.
    struct Rig
    {
        FakeDiskFileIo                        io;
        CassoLinkVirtualDrives                drives { io };
        InMemoryCassoLinkFolder               folder;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::unique_ptr<CassoLinkHost>        host;
        std::jthread                          thread;

        Rig()
        {
            for (const char * path : { kDrive1, kDrive2 })
            {
                std::vector<Byte>  volume = MakeVolume (path == kDrive1 ? "ONE" : "TWO");

                io.files[path]  = volume;
                io.stamps[path] = FileStamp { volume.size(), 1 };
            }

            drives.SetImage (1, kDrive1);
            drives.SetImage (2, kDrive2);

            InProcessByteStream::CreatePair (machineEnd, pcEnd);

            host = std::make_unique<CassoLinkHost> (pcEnd, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetVirtualDrives (&drives);
        }

        void  Serve()
        {
            thread = std::jthread ([this] (std::stop_token stop)
            {
                HRESULT  hr = S_OK;

                while (!stop.stop_requested() && SUCCEEDED (hr))
                {
                    hr = host->ServeOne (20);
                }
            });
        }

        void  Stop()
        {
            thread.request_stop();

            if (thread.joinable())
            {
                thread.join();
            }
        }

        ~Rig()
        {
            Stop();
        }
    };


    static bool  RunUntilScreenShows (MachineHost & host, const std::string & needle, uint64_t cycleCap)
    {
        constexpr uint64_t  kSlice = 1'000'000ULL;

        std::vector<std::string>  rows;
        uint64_t                  spent    = 0;
        bool                      onScreen = false;



        while (!onScreen && spent < cycleCap)
        {
            spent += host.GetRunner().RunCycles (kSlice);

            rows.clear();
            GuestSession::CollectRows (host, rows);
            onScreen = GuestSession::AnyRowContains (rows, needle);
        }

        return onScreen;
    }


    //  The slot in the installer's report, or 0.
    static int  FindSlot (const std::vector<std::string> & rows)
    {
        static constexpr const char *  kReport = "CASSOLINK DRIVES IN SLOT ";

        for (const std::string & row : rows)
        {
            size_t  at = row.find (kReport);

            if (at != std::string::npos && at + strlen (kReport) < row.size())
            {
                return row[at + strlen (kReport)] - '0';
            }
        }

        return 0;
    }


    TEST_METHOD (AProgramSavedToEachVirtualDrive_IsInItsImageOnThePc)
    {
        Rig                       rig;
        TestMachine               machine ("Apple2c");
        std::vector<Byte>         disk;
        std::vector<std::string>  rows;
        std::vector<Byte>         saved;
        Byte                      fileType = 0;
        Word                      auxType  = 0;
        int                       slot     = 0;



        AssertSucceeded (CassoLinkProDosDisk::Make (GuestSession::RequireProDosUsersDisk(), "Users.dsk", kLinkBaud, disk));

        GuestSession::BootToPrompt (machine, disk);
        GuestSession::CollectRows (machine, rows);

        AssertSucceeded (machine.GetConnectors().AttachSerialStream (kPort, rig.machineEnd));
        rig.Serve();

        slot = FindSlot (rows);
        Assert::IsTrue (slot >= 1 && slot <= 7, L"The installer reports the drives' slot");

        rows = GuestSession::TypeAndCollect (machine, std::format ("CAT ,S{},D1", slot));
        Assert::IsTrue (GuestSession::AnyRowContains (rows, "/ONE"), L"Drive 1 is the first image's volume");

        rows = GuestSession::TypeAndCollect (machine, std::format ("CAT ,S{},D2", slot));
        Assert::IsTrue (GuestSession::AnyRowContains (rows, "/TWO"), L"Drive 2 is the second image's volume");

        GuestSession::TypeAndCollect (machine, "10 PRINT 42");
        GuestSession::TypeAndCollect (machine, "SAVE /ONE/FIRST");
        GuestSession::TypeAndCollect (machine, "SAVE /TWO/SECOND");

        //  With the PC gone, a call to the drive ends in ProDOS's I/O error.
        rig.Stop();
        KeystrokeInjector::InjectLine (machine, std::format ("CAT ,S{},D1", slot));
        Assert::IsTrue (RunUntilScreenShows (machine, "I/O ERROR", kNoReplyCap), L"A call with no reply is an I/O error");

        AssertSucceeded (ProDosReader::ExtractFile (rig.io.files[kDrive1], "FIRST", saved, fileType, auxType));
        Assert::AreEqual ((Byte) CassoLinkProDosDisk::kBasicType, fileType, L"The first image has the program saved to drive 1");

        AssertSucceeded (ProDosReader::ExtractFile (rig.io.files[kDrive2], "SECOND", saved, fileType, auxType));
        Assert::AreEqual ((Byte) CassoLinkProDosDisk::kBasicType, fileType, L"The second image has the program saved to drive 2");
    }
};
