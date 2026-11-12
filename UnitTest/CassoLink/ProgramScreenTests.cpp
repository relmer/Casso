#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../EmuTests/TextScreenScraper.h"
#include "Core/TextEncoding.h"
#include "Devices/RamDevice.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramScreenTests
//
//  What the shim and the program draw on a //c's 40-column screen from the
//  host's packets: the shim's wait, the connection, the label, an error, the
//  history and a drive's bar. The session's text for each is covered by the
//  CassoLinkSessionTests unit tests; these check its place on the screen.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ProgramScreenTests)
{
public:

    static constexpr uint64_t  kSettleCycles = 30 * 17'030;   // for the program to draw after a packet
    static constexpr Byte      kFastControl  = 0x10;          // 115,200 baud
    static constexpr Byte      kCatalogTrack = 17;

    //  The rows, by their numbers on the text page.
    static constexpr int  kRowStatus       = 0;
    static constexpr int  kRowPort         = 1;
    static constexpr int  kRowWaiting      = 2;
    static constexpr int  kRowLabel        = 4;
    static constexpr int  kRowDrive1       = 7;
    static constexpr int  kRowBar2         = 11;
    static constexpr int  kRowError        = 13;
    static constexpr int  kRowHistoryTitle = 14;
    static constexpr int  kRowHistory1     = 15;
    static constexpr int  kRowHistory2     = 16;

    static std::vector<Byte>  Dos33()
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        return std::vector<Byte> (bytes.begin(), bytes.end());
    }

    //  A row of the text page, with its trailing spaces cut, after the
    //  machine has run long enough to draw it.
    static std::string  Row (ProgramRig & rig, int row)
    {
        std::string  text;



        rig.machine.RunCycles (kSettleCycles);

        text = TextScreenScraper::Scrape40 (rig.machine.GetMemoryBus(), 0x0400)[(size_t) row];
        return text.substr (0, text.find_last_not_of (' ') + 1);
    }

    static void  AssertRow (ProgramRig & rig, int row, const std::string & text, const wchar_t * what)
    {
        std::string  drawn = Row (rig, row);



        Assert::AreEqual (text, drawn, (std::wstring (what) + L": " + TextEncoding::NarrowToWide (drawn)).c_str());
    }



    //  No host yet: the shim waits, at the speed it was built for.
    TEST_METHOD (TheShim_ShowsItsWait_AndItsSpeed)
    {
        ProgramRig  rig ({}, {}, 9600);



        AssertRow (rig, kRowWaiting, "WAITING FOR THE PC ON:", L"the wait");
        AssertRow (rig, kRowLabel,   "SERIAL PORT 2, 9600",    L"the port and speed");
    }



    //  Once a request is over, the host's Idle clears the label, and the
    //  program draws the connection, the link's speed and an idle drive.
    //  The Idle is the reply to the program's next Ready. An Idle sent
    //  before the Ready arrives while the program draws and sends, and each
    //  byte is overwritten by the next in the //c's serial chip, which has a
    //  one-byte receive register.
    TEST_METHOD (AfterARequest_TheLabelIsClear_AndTheConnectionShown)
    {
        static constexpr DWORD  kReadyTimeoutMs = 2000;   // the program sends a Ready each second

        ProgramRig         rig (Dos33());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  data;
        CassoLinkStatus    status;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));
        AssertSucceeded (rig.host->SendLabel ("READING THE CATALOG OF D1"));
        AssertRow (rig, kRowLabel, "READING THE CATALOG OF D1", L"the label");

        AssertSucceeded (rig.host->ReadSectors (1, kCatalogTrack, sector, data, status));
        AssertSucceeded (rig.host->ServeOne (kReadyTimeoutMs));

        AssertRow (rig, kRowStatus, "CASSOLINK: CONNECTED",  L"the status");
        AssertRow (rig, kRowPort,   "SERIAL PORT 2, 115200", L"the link's speed");
        AssertRow (rig, kRowLabel,  "",                      L"no label after the request");
        AssertRow (rig, kRowError,  "",                      L"no error");
        AssertRow (rig, kRowDrive1, "D1: IDLE",              L"drive 1");
    }



    //  A write to a protected disk: the error row has the disk's drive, not a
    //  track, and stays on the screen.
    TEST_METHOD (AWriteToAProtectedDisk_ShowsItsDrive_OnTheErrorRow)
    {
        ProgramRig         rig (Dos33());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  data (256, 0x5A);
        CassoLinkStatus    status;



        rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0)->SetWriteProtected (true);

        AssertSucceeded (rig.host->WriteSectors (1, 9, sector, data, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorWriteProtected, status.error);

        AssertRow (rig, kRowError, "ERROR: WRITE-PROTECTED DISK IN D1", L"the error row");
        AssertRow (rig, kRowError, "ERROR: WRITE-PROTECTED DISK IN D1", L"the error row, a moment later");
    }



    //  Each line is the PC's time, then the text; the newest is on top, under
    //  the HISTORY row.
    TEST_METHOD (TheHistory_HasEachLine_NewestOnTop)
    {
        ProgramRig  rig ({});



        AssertSucceeded (rig.host->SetSpeed (kFastControl));
        AssertSucceeded (rig.host->SendLog (12, 30, "PC CONNECTED"));
        AssertSucceeded (rig.host->SendLog (12, 34, "OK  COPIED FROMPC TO D1"));

        AssertRow (rig, kRowHistoryTitle, "HISTORY:",                      L"the heading");
        AssertRow (rig, kRowHistory1,     "12:34 OK  COPIED FROMPC TO D1", L"the newest line");
        AssertRow (rig, kRowHistory2,     "12:30 PC CONNECTED",            L"the line before it");
    }



    //  Set columns to 80 on a //c: the odd columns of the status row are in
    //  main memory, with the connection at the row's right edge.
    TEST_METHOD (EightyColumns_PutTheOddColumnsInMainMemory)
    {
        struct Cell { Word at; char c; const wchar_t * what; };

        static constexpr Cell  kCells[] = { { 0x0400, 'A', L"column 1 of CASSOLINK" },
                                            { 0x0401, 'S', L"column 3" },
                                            { 0x0406, 'N', L"column 13, in CONNECTED" },
                                            { 0x0426, '2', L"column 77, in the connection at the right edge" },
                                            { 0x0427, '0', L"column 79, the speed's last digit" } };

        ProgramRig  rig ({});



        AssertSucceeded (rig.host->SetSpeed (kFastControl));
        AssertSucceeded (rig.host->SetColumns (true));
        rig.machine.RunCycles (kSettleCycles);

        for (const Cell & cell : kCells)
        {
            Assert::AreEqual<int> (cell.c | 0x80, (int) rig.machine.GetRefs().mainRamDev->GetData()[cell.at], cell.what);
        }
    }



    //  The bar has a cell for each track, so reads with no share of a
    //  whole-disk transfer draw no cells, and reads with one draw some.
    TEST_METHOD (TheDrivesBar_IsFilledOnlyByAWholeDiskTransfer)
    {
        ProgramRig         rig ({}, Dos33());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  data;
        CassoLinkStatus    status;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        AssertSucceeded (rig.host->ReadSectors (2, kCatalogTrack, sector, data, status));
        AssertRow (rig, kRowBar2, "", L"a listing's read");

        rig.host->SetProgress (0, 100);
        AssertSucceeded (rig.host->ReadSectors (2, kCatalogTrack, sector, data, status));
        Assert::AreNotEqual (std::string(), Row (rig, kRowBar2), L"a whole-disk transfer's read");
    }
};
