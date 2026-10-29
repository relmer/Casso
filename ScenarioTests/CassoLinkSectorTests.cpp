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
        uint64_t                              spent = 0;   // the last Run's cycles

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
            auto  result = std::async (std::launch::async, operation);

            spent = 0;

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




    //  Zero page and the reset vector as the machine had them before the
    //  shim ran, for CassoLink's exit to put back.
    static void  MarkMachineState (Rig & rig)
    {
        for (Word at = 0xE0; at <= 0xFF; at++)
        {
            rig.machine.GetCpu()->WriteByte (at, (Byte) (0x40 + at - 0xE0));
        }

        for (Word at = 0x06; at <= 0x1F; at++)
        {
            rig.machine.GetCpu()->WriteByte (at, (Byte) (0x80 + at));
        }

        rig.machine.GetCpu()->WriteByte (0x03F2, 0x69);
        rig.machine.GetCpu()->WriteByte (0x03F3, 0xFF);
        rig.machine.GetCpu()->WriteByte (0x03F4, 0xFF ^ 0xA5);
    }

    //  CassoLink ended: the marks back, BASIC's program empty, and with no
    //  DOS in memory, the monitor's prompt.
    static void  AssertMachinePutBack (Rig & rig)
    {
        std::string  screen;

        for (int frame = 0; frame < 60; frame++)
        {
            rig.machine.RunCycles (kFrame);
        }

        //  FLASHBIT, at $F3, is set off rather than put back.
        for (Word at = 0xE0; at <= 0xFF; at++)
        {
            Assert::AreEqual<int> (at == 0xF3 ? 0 : 0x40 + at - 0xE0, rig.machine.GetCpu()->ReadByte (at), std::format (L"zero page ${:02X}", at).c_str());
        }

        for (Word at = 0x06; at <= 0x1F; at++)
        {
            Assert::AreEqual<int> (0x80 + at, rig.machine.GetCpu()->ReadByte (at), std::format (L"zero page ${:02X}", at).c_str());
        }

        Assert::AreEqual<int> (0x69, rig.machine.GetCpu()->ReadByte (0x03F2), L"the reset vector");
        Assert::AreEqual<int> (0xFF, rig.machine.GetCpu()->ReadByte (0x03F3));
        Assert::AreEqual<int> (0x00, rig.machine.GetCpu()->ReadByte (0x0800), L"BASIC's program is empty");
        Assert::AreEqual<int> (0x00, rig.machine.GetCpu()->ReadByte (0x0801));
        Assert::AreEqual<int> (0x00, rig.machine.GetCpu()->ReadByte (0x0802));

        screen = rig.Screen();
        Assert::IsTrue (screen.find ('*') != std::string::npos, L"the monitor's prompt");
    }




    //  Set speed's status fits its row whatever values zero page has at the
    //  start, so nothing is drawn in the history below it.
    TEST_METHOD (SetSpeed_StatusStaysOnItsRow)
    {
        Rig  rig;

        rig.machine.GetCpu()->WriteByte (0xE3, 200);     // the sector
        rig.machine.GetCpu()->WriteByte (0x07, 200);     // the percentage
        rig.machine.GetCpu()->WriteByte (0x08, 200);

        rig.Connect();

        for (const std::string & screen : rig.screens)
        {
            std::vector<std::string>  rows;
            size_t                    start = 0;

            for (size_t end = screen.find ('\n'); end != std::string::npos; start = end + 1, end = screen.find ('\n', start))
            {
                rows.push_back (screen.substr (start, end - start));
            }

            for (size_t row = 12; row < 23 && row < rows.size(); row++)
            {
                Assert::AreEqual (std::string::npos, rows[row].find_first_not_of (' '), std::format (L"row {} is blank", row).c_str());
            }
        }

        Assert::AreNotEqual (std::string::npos, rig.screens.back().find ("PRESS ESC TO EXIT"), L"the last row has how to leave");
    }




    //  With no reply for several seconds, the PC is gone: the label of its
    //  operation is cleared, and the PC is waited for again.
    TEST_METHOD (APcThatStopsAnswering_ClearsItsLabel_AndIsWaitedForAgain)
    {
        Rig                       rig;
        std::vector<std::string>  rows;

        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->SendLabel ("READING THE CATALOG OF D2"); }));

        for (int frame = 0; frame < 60 * 7; frame++)
        {
            rig.machine.RunCycles (kFrame);
        }

        rows = TextScreenScraper::Scrape (rig.machine);

        Assert::AreEqual (std::string::npos, rows[7].find_first_not_of (' '), L"the label row is blank");
        Assert::AreNotEqual (std::string::npos, rows[3].find ("WAITING"), L"the PC is waited for");
        Assert::AreNotEqual (std::string::npos, rows[6].find ("STATUS: WAITING FOR CONNECTION"), L"and the status row");
    }




    //  Time drive sends a marker each time sector 0 passes, then the Status.
    TEST_METHOD (TimeDrive_SendsAMarkerEachRevolution)
    {
        Rig              rig;
        CassoLinkStatus  status;
        double           revolution = 0.0;

        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->TimeDrive (1, 17, 5, revolution, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::IsTrue (revolution > 0.0, L"a time between the markers");
    }




    //  The PC's Exit ends CassoLink, and the machine's state is put back.
    TEST_METHOD (Exit_PutsTheMachineBack)
    {
        Rig  rig;

        MarkMachineState (rig);
        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->Exit(); }));
        AssertMachinePutBack (rig);
    }




    //  Control-Reset ends CassoLink the same way.
    TEST_METHOD (ControlReset_PutsTheMachineBack)
    {
        Rig  rig;

        MarkMachineState (rig);
        rig.Connect();

        rig.machine.SoftReset();
        AssertMachinePutBack (rig);
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




    //  A whole track's sectors, in the skewed order DOS 3.3's logical
    //  sectors map to, are written in one pass and read back once. The
    //  emulated line takes most of the time: about 16 revolutions, where
    //  writing and reading back each sector in turn took 38.
    TEST_METHOD (WriteSectors_AWholeTrackTakesAFewRevolutions)
    {
        static constexpr uint64_t  kRevolution = 204'600;   // cycles at 300 rpm
        Rig                        rig;
        const Byte                 sectors[]   = { 0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15 };
        std::vector<Byte>          data        = Pattern (16 * 256, 0x33);
        std::vector<Byte>          image;
        CassoLinkStatus            status;

        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->WriteSectors (1, 21, sectors, data, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Logger::WriteMessage (std::format ("{} cycles, {:.1f} revolutions\n", rig.spent, (double) rig.spent / kRevolution).c_str());
        Assert::IsTrue (rig.spent < 25 * kRevolution, std::format (L"{} cycles", rig.spent).c_str());

        image = rig.Image (0);

        for (size_t i = 0; i < 16; i++)
        {
            Assert::IsTrue (SameSector (data, i, image, DosOffset (21, sectors[i])), L"the DOS 3.3 image has the sector");
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




    TEST_METHOD (WriteTrack_WritesEverySector_AndItsStatusEndsOnTheLast)
    {
        Rig                rig;
        std::vector<Byte>  pattern = Pattern (16 * 256, 7);
        std::vector<Byte>  data;
        CassoLinkStatus    status;

        rig.Connect();

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->WriteTrack (1, 30, 254, pattern, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);

        //  The status's sector, at $E3, is the one shown until the next
        //  operation: on a real line, while the next track arrives.
        Assert::AreEqual<int> (15, rig.machine.GetCpu()->ReadByte (0xE3), L"the status ends on sector 15");

        data = rig.Read (1, 30, AllSectors());
        Assert::IsTrue (data == pattern, L"every sector reads back as written");
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
        Assert::IsTrue (reading.find ("CONNECTION: SERIAL PORT 2, 115200") != std::string::npos, L"the connection");
        Assert::IsTrue (reading.find ("STATUS: READING    D1, T17, S") != std::string::npos, L"the drive and track on the status row");
        Assert::AreEqual ((size_t) 32, reading.find ('(') - reading.find ("STATUS:"), L"two-digit track and sector put the percentage in one column");
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
