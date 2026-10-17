#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSectorTests
//
//  Casso's own client on an emulated //c: the shim is placed at $0300 and
//  run, the host loads the program into it over port 2, and the program
//  reads, writes and formats the disks in its two drives -- a DOS 3.3 image
//  in drive 1 and a ProDOS image in drive 2 -- at the host's request. What
//  comes back, and what the images contain afterward, is compared with the
//  images byte for byte.
//
//  A sector here is the physical sector, the number in its address field;
//  the images are in DOS 3.3 and ProDOS order, so the comparisons map each
//  physical sector to its place in the file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkSectorTests)
{
public:

    static constexpr uint64_t  kFrame       = 17030;
    static constexpr uint64_t  kBudget      = 400'000'000;
    static constexpr Byte      kFastControl = 0x10;     // 115,200 baud

    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        InMemoryCassoLinkFolder               folder;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::unique_ptr<CassoLinkHost>        host;
        std::vector<Byte>                     dos;
        std::vector<Byte>                     prodos;
        std::vector<std::string>              screens;

        Rig()
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;
            std::vector<Byte>     shim;
            std::string           errors;
            Word                  at = LinkProgram::kShimAddress;

            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            dos.assign (bytes.begin(), bytes.end());
            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/prodos.po", bytes));
            prodos.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            machine.PowerCycle();

            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, dos));
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Po, prodos));
            machine.GetRefs().diskController->SetExternalDisk (0, machine.GetDiskStore().GetImage (6, 0));
            machine.GetRefs().diskController->SetExternalDisk (1, machine.GetDiskStore().GetImage (6, 1));

            InProcessByteStream::CreatePair (machineEnd, hostEnd);
            Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

            host = std::make_unique<CassoLinkHost> (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetOperationTimeout (60000);

            Assert::AreEqual (S_OK, LinkProgram::AssembleShim (shim, errors));

            for (Byte b : shim)
            {
                machine.GetCpu()->WriteByte (at++, b);
            }

            machine.GetCpu()->SetPC (LinkProgram::kShimAddress);
        }

        std::string  Screen()
        {
            std::string  text;

            for (const std::string & row : TextScreenScraper::Scrape (machine)) { text += row + "\n"; }
            return text;
        }

        //  Steps the machine while the host runs `operation`, and adds the
        //  screen at each frame to `screens`.
        HRESULT  Run (std::function<HRESULT()> operation, std::function<void()> eachFrame = nullptr)
        {
            auto      result = std::async (std::launch::async, operation);
            uint64_t  spent  = 0;

            while (result.wait_for (std::chrono::seconds (0)) != std::future_status::ready && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
                screens.push_back (Screen());

                if (eachFrame)
                {
                    eachFrame();
                }
            }

            Assert::IsTrue (spent < kBudget, L"the operation finishes");
            return result.get();
        }

        //  The program loaded, and the line at 115,200 baud.
        void  Connect()
        {
            Assert::AreEqual (S_OK, Run ([this] () { return host->SetSpeed (kFastControl); }));
            Assert::AreEqual<int> (LinkProgram::kProgramVersion, host->GetClient().programVersion);
            Assert::AreEqual<int> (0x03, host->GetClient().driveMap);
        }

        std::vector<Byte>  Read (Byte drive, Byte track, std::span<const Byte> sectors, Byte expectedError = CassoLinkHost::kErrorNone)
        {
            std::vector<Byte>  data;
            CassoLinkStatus    status;

            Assert::AreEqual (S_OK, Run ([&] () { return host->ReadSectors (drive, track, sectors, data, status); }));
            Assert::AreEqual<int> (expectedError, status.error);
            return data;
        }

        std::vector<Byte>  Image (int drive)
        {
            std::vector<Byte>  bytes;

            AssertSucceeded (machine.GetDiskStore().GetImage (6, drive)->Serialize (bytes));
            return bytes;
        }
    };

    static std::vector<Byte>  AllSectors()
    {
        std::vector<Byte>  sectors (16);

        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        return sectors;
    }

    //  Where a physical sector of a track sits in a DOS-ordered or a
    //  ProDOS-ordered file.
    static size_t  DosOffset (int track, int physical)
    {
        return (size_t) (track * 16 + NibblizationLayer::GetDosFileIndexForPhysicalSector (physical)) * 256;
    }

    static size_t  ProDosOffset (int track, int physical)
    {
        int  logical = NibblizationLayer::GetDosFileIndexForPhysicalSector (physical);

        return (size_t) (track * 16 + NibblizationLayer::GetPoFileIndexForDosLogicalSector (logical)) * 256;
    }

    static bool  SameSector (const std::vector<Byte> & data, size_t index, const std::vector<Byte> & image, size_t offset)
    {
        return std::equal (data.begin() + (ptrdiff_t) (index * 256), data.begin() + (ptrdiff_t) ((index + 1) * 256), image.begin() + (ptrdiff_t) offset);
    }

    static std::vector<Byte>  Pattern (size_t size, Byte seed)
    {
        std::vector<Byte>  bytes (size);

        for (size_t i = 0; i < size; i++)
        {
            bytes[i] = (Byte) (seed + i * 13 + (i >> 8));
        }

        return bytes;
    }




    TEST_METHOD (ReadSectors_MatchBothImages)
    {
        Rig                rig;
        std::vector<Byte>  sectors = AllSectors();
        std::vector<Byte>  data;

        rig.Connect();

        for (Byte track : { 0, 17 })
        {
            data = rig.Read (1, track, sectors);

            for (int p = 0; p < 16; p++)
            {
                Assert::IsTrue (SameSector (data, (size_t) p, rig.dos, DosOffset (track, p)), std::format (L"DOS 3.3 track {} sector {}", track, p).c_str());
            }
        }

        data = rig.Read (2, 2, sectors);

        for (int p = 0; p < 16; p++)
        {
            Assert::IsTrue (SameSector (data, (size_t) p, rig.prodos, ProDosOffset (2, p)), std::format (L"ProDOS track 2 sector {}", p).c_str());
        }
    }




    TEST_METHOD (WriteSectors_AreInBothImages)
    {
        Rig                rig;
        const Byte         sectors[] = { 3, 11, 4 };
        std::vector<Byte>  data      = Pattern (3 * 256, 0x5A);
        std::vector<Byte>  image;
        CassoLinkStatus    status;

        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->WriteSectors (1, 20, sectors, data, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::AreEqual<int> (3, status.done);

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->WriteSectors (2, 5, sectors, data, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);

        image = rig.Image (0);

        for (size_t i = 0; i < 3; i++)
        {
            Assert::IsTrue (SameSector (data, i, image, DosOffset (20, sectors[i])), L"the DOS 3.3 image has the sector");
        }

        image = rig.Image (1);

        for (size_t i = 0; i < 3; i++)
        {
            Assert::IsTrue (SameSector (data, i, image, ProDosOffset (5, sectors[i])), L"the ProDOS image has the sector");
        }

        Assert::IsTrue (SameSector (rig.Read (1, 20, std::vector<Byte> { 5 }), 0, rig.dos, DosOffset (20, 5)), L"and the sectors around them are as they were");
    }




    TEST_METHOD (FormatTrack_WritesSixteenEmptySectors)
    {
        Rig                rig;
        std::vector<Byte>  before = rig.dos;
        std::vector<Byte>  image;
        std::vector<Byte>  data;
        CassoLinkStatus    status;

        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->FormatTrack (1, 30, 254, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);

        data = rig.Read (1, 30, AllSectors());
        Assert::IsTrue (std::all_of (data.begin(), data.end(), [] (Byte b) { return b == 0; }), L"every sector reads back empty");

        image = rig.Image (0);
        Assert::IsTrue (std::all_of (image.begin() + 30 * 4096, image.begin() + 31 * 4096, [] (Byte b) { return b == 0; }), L"the image's track is empty");
        Assert::IsTrue (std::equal (image.begin() + 29 * 4096, image.begin() + 30 * 4096, before.begin() + 29 * 4096), L"track 29 is as it was");
        Assert::IsTrue (std::equal (image.begin() + 31 * 4096, image.begin() + 32 * 4096, before.begin() + 31 * 4096), L"track 31 is as it was");
    }




    TEST_METHOD (WriteSectors_AWriteProtectedImageEndsWithItsError)
    {
        Rig                rig;
        const Byte         sectors[] = { 0 };
        std::vector<Byte>  data      = Pattern (256, 1);
        CassoLinkStatus    status;

        rig.machine.GetDiskStore().GetImage (6, 0)->SetUserWriteProtected (true);
        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->WriteSectors (1, 9, sectors, data, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorWriteProtected, status.error);
        Assert::IsTrue (rig.Image (0) == rig.dos, L"and the image is untouched");
    }




    TEST_METHOD (ReadSectors_ProgressIsOnTheStatusScreen)
    {
        Rig          rig;
        std::string  reading;

        rig.Connect();
        rig.screens.clear();
        rig.Read (1, 17, AllSectors());

        for (const std::string & screen : rig.screens)
        {
            if (screen.find ("READING") != std::string::npos && screen.find ("(50%)") != std::string::npos)
            {
                reading = screen;
                break;
            }
        }

        Assert::IsFalse (reading.empty(), L"the screen reported the read halfway through");
        Assert::IsTrue (reading.find ("CONNECTION: PORT 2, 115200") != std::string::npos, L"the connection");
        Assert::IsTrue (reading.find ("STATUS: READING D1, T17, S") != std::string::npos, L"the drive and track on the status row");
    }




    TEST_METHOD (Cancel_StopsARead_AndTheNextReadWorks)
    {
        Rig                rig;
        std::vector<Byte>  data;
        CassoLinkStatus    status;
        bool               canceled = false;
        HRESULT            hr       = S_OK;

        rig.Connect();

        hr = rig.Run ([&] () { return rig.host->ReadSectors (1, 17, AllSectors(), data, status); },
                      [&] ()
                      {
                          if (!canceled && rig.screens.back().find ("(18%)") != std::string::npos)
                          {
                              canceled = true;
                              Assert::AreEqual (S_OK, rig.host->Cancel());
                          }
                      });

        Assert::IsTrue (canceled);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), hr);

        data = rig.Read (1, 0, std::vector<Byte> { 0 });
        Assert::IsTrue (SameSector (data, 0, rig.dos, DosOffset (0, 0)), L"the next read");
    }




    TEST_METHOD (ReadSectors_CompleteAfterTheMachinePausesTenSeconds)
    {
        Rig                rig;
        std::vector<Byte>  data;
        CassoLinkStatus    status;
        bool               paused = false;

        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->ReadSectors (1, 0, AllSectors(), data, status); },
                                         [&] ()
                                         {
                                             if (!paused && rig.screens.back().find ("(25%)") != std::string::npos)
                                             {
                                                 paused = true;
                                                 std::this_thread::sleep_for (std::chrono::seconds (10));
                                             }
                                         }));

        Assert::IsTrue (paused, L"the machine paused mid-read");
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);

        for (int p = 0; p < 16; p++)
        {
            Assert::IsTrue (SameSector (data, (size_t) p, rig.dos, DosOffset (0, p)), std::format (L"sector {}", p).c_str());
        }
    }
};
