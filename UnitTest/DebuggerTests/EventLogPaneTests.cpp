#include "Pch.h"

#include "Ui/Debugger/Panes/DiskLogPane.h"
#include "Ui/Debugger/Panes/InputLogPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace EventLogPaneTests
{
    //  A Disk II row at a position: its cycle is ten times the position.
    static DiskLogRow  MakeDiskRow (uint64_t position, Disk2EventType type, int track = -1, int sector = -1, int qt = 0)
    {
        DiskLogRow  row;



        row.position    = position;
        row.cycle       = position * 10;
        row.event.type  = type;
        row.event.drive = 0;

        switch (type)
        {
            case Disk2EventType::AddrMark:
                row.event.payload.addrMark = { track, sector, 254 };
                break;

            case Disk2EventType::DataRead:
            case Disk2EventType::DataWrite:
            case Disk2EventType::DataChecksumError:
                row.event.payload.dataMark = { track, sector, 254, 256 };
                break;

            case Disk2EventType::HeadStep:
                row.event.payload.step = { qt - 2, qt };
                break;

            default:
                break;
        }

        return row;
    }


    static InputLogRow  MakeInputRow (uint64_t position, InputEventType type, Word address, Byte value, bool isHost = false)
    {
        InputLogRow  row;



        row.position                 = position;
        row.cycle                    = position * 10;
        row.event.category           = isHost ? InputEventCategory::Host : InputEventCategory::Guest;
        row.event.type               = type;
        row.event.payload.io.address = address;
        row.event.payload.io.value   = value;

        return row;
    }


    //  The cell text of every row in the list, in one column.
    template <typename T>
    static std::vector<std::wstring>  GetColumn (const EventLogPane<T> & pane, size_t column)
    {
        std::vector<std::wstring>       texts;
        std::vector<DxuiListView::Cell> cells;
        size_t                          i     = 0;



        for (i = 0; i < pane.GetView().size(); i++)
        {
            pane.ProvideRow ((int) i, cells);
            texts.push_back (cells[column].text);
        }

        return texts;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EventLogPaneTests
    //
    //  The Disk II event log and input log panes over rows sent as deltas:
    //  the order, the row above the oldest and its request for older rows,
    //  older rows going in front with the same rows selected, cuts, the rows
    //  hidden behind live, the filters, sorting, copying and the options
    //  text.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (EventLogPaneTests)
    {
    public:

        TEST_METHOD (RowsComeInPositionOrderUnderTheStartRow)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;



            pane.Configure();

            delta.isReset   = true;
            delta.isAtStart = true;
            delta.newer     = { MakeDiskRow (10, Disk2EventType::MotorCommandOn),
                                MakeDiskRow (20, Disk2EventType::AddrMark, 17, 5),
                                MakeDiskRow (30, Disk2EventType::DataRead, 17, 5) };

            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual ((size_t) 4, pane.GetView().size());
            Assert::AreEqual (std::wstring (EventLogText::GetStartText()), GetColumn (pane, DiskLogPane::kDetail)[0]);
            Assert::AreEqual (std::wstring (L"Motor command on"),          GetColumn (pane, DiskLogPane::kEvent)[1]);
            Assert::AreEqual (std::wstring (L"T17 S5 V254 (256 bytes)"),   GetColumn (pane, DiskLogPane::kDetail)[3]);
            Assert::AreEqual (std::wstring (L"300"),                       GetColumn (pane, DiskLogPane::kCycle)[3]);
            Assert::AreEqual (4, list.GetRowCount());
        }


        //  The Event column fits its widest label, as "Motor command off",
        //  rather than having a set width; Detail takes the room left.
        TEST_METHOD (TheEventColumnFitsItsTextAndDetailStretches)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});



            pane.Configure();

            Assert::AreEqual (0,     list.GetColumnAt (DiskLogPane::kEvent).widthDip);
            Assert::IsFalse  (list.GetColumnAt (DiskLogPane::kEvent).stretch);
            Assert::IsTrue   (list.GetColumnAt (DiskLogPane::kDetail).stretch);
            Assert::AreEqual (110,   list.GetColumnAt (DiskLogPane::kCycle).widthDip);
        }


        //  An error row and a warning row have their severity and the fill of
        //  it on every cell; an info row has neither fill. The severity
        //  filter hides a severity's rows.
        TEST_METHOD (SeverityRowsHaveTheirFillAndFilter)
        {
            constexpr uint32_t  kWarning = 0xFF806000u;
            constexpr uint32_t  kError   = 0xFF800000u;
            DxuiListView        list;
            DiskLogPane         pane (&list, [] (const std::string &) {});
            DiskLogDelta                     delta;
            std::vector<DxuiListView::Cell>  cells;



            pane.Configure();
            pane.SetSeverityFills (kWarning, kError);

            delta.isReset   = true;
            delta.isAtStart = true;
            delta.newer     = { MakeDiskRow (10, Disk2EventType::DataRead,          17, 5),
                                MakeDiskRow (20, Disk2EventType::DataChecksumError, 17, 6),
                                MakeDiskRow (30, Disk2EventType::ReadStall) };

            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (std::wstring (L"Info"),          GetColumn (pane, DiskLogPane::kSeverity)[1]);
            Assert::AreEqual (std::wstring (L"Error"),         GetColumn (pane, DiskLogPane::kSeverity)[2]);
            Assert::AreEqual (std::wstring (L"Warning"),       GetColumn (pane, DiskLogPane::kSeverity)[3]);
            Assert::AreEqual (std::wstring (L"Data checksum"), GetColumn (pane, DiskLogPane::kEvent)[2]);
            Assert::AreEqual (std::wstring (L"Sector read"),   GetColumn (pane, DiskLogPane::kEvent)[1]);

            pane.ProvideRow (1, cells);
            Assert::AreEqual (0u, cells[DiskLogPane::kEvent].background, L"no fill on an info row");

            pane.ProvideRow (2, cells);
            Assert::AreEqual (kError, cells[DiskLogPane::kCycle].background);
            Assert::AreEqual (kError, cells[DiskLogPane::kDetail].background);

            pane.ProvideRow (3, cells);
            Assert::AreEqual (kWarning, cells[DiskLogPane::kEvent].background);

            pane.SetSeverityShown (DiskLogPane::kInfo, false);
            pane.Refresh();
            Assert::AreEqual ((size_t) 3, pane.GetView().size(), L"the start row, the error and the warning");
        }


        //  A sector's row has its first bytes in the detail, the whole sector
        //  as a hex dump in the detail's tip, and the bytes for Copy.
        TEST_METHOD (ASectorRowHasItsBytes)
        {
            DxuiListView                      list;
            DiskLogPane                       pane (&list, [] (const std::string &) {});
            DiskLogDelta                      delta;
            DiskLogRow                        row  = MakeDiskRow (10, Disk2EventType::DataRead, 0, 0);
            std::vector<Byte>                 bytes (256);
            std::vector<DxuiListView::Cell>   cells;
            std::wstring                      dump;



            for (size_t i = 0; i < bytes.size(); i++)
            {
                bytes[i] = (Byte) i;
            }

            bytes[0x10] = 0xC1;
            row.data    = std::make_shared<const std::vector<Byte>> (bytes);

            pane.Configure();

            delta.isReset   = true;
            delta.isAtStart = true;
            delta.newer     = { row };

            pane.Apply   (delta);
            pane.Refresh();
            pane.ProvideRow (1, cells);

            dump = cells[DiskLogPane::kDetail].tip;

            Assert::AreEqual (std::wstring (L"T0 S0 V254 (256 bytes) 00 01 02 03 04 05 06 07 ..."), cells[DiskLogPane::kDetail].text);
            Assert::IsTrue   (dump.starts_with (L"00: 00 01 02 03"), dump.c_str());
            Assert::IsTrue   (dump.find (L"\n10: C1 11 ") != std::wstring::npos, L"the second line starts at $10");
            Assert::IsTrue   (dump.find (L" A") != std::wstring::npos, L"$C1 is A with its high bit dropped");
            Assert::AreEqual ((size_t) 16, (size_t) std::ranges::count (dump, L'\n') + 1, L"sixteen lines of sixteen");
            Assert::IsNotNull (pane.GetRowData (1).get());
            Assert::IsNull    (pane.GetRowData (0).get(), L"the start row has none");
        }


        //  Columns saved with another set of columns are left out rather
        //  than put on the wrong columns.
        TEST_METHOD (ColumnsSavedForOtherColumnsAreLeftOut)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});



            pane.Configure();
            pane.SetOptionsText ("severity=101;columns=00000 0,0,0,0,0");

            Assert::IsTrue  (pane.IsColumnShown (DiskLogPane::kCycle),  L"five columns saved, six now");
            Assert::IsFalse (pane.IsSeverityShown (DiskLogPane::kWarning));
            Assert::IsTrue  (pane.IsSeverityShown (DiskLogPane::kError));
        }


        //  With the row above the oldest in view, the pane requests the rows
        //  before it once, and again only after older rows came in.
        TEST_METHOD (TheStartRowRequestsEarlierRowsOncePerOldestRow)
        {
            DxuiListView               list;
            std::vector<std::string>   requests;
            DiskLogPane                pane (&list, [&requests] (const std::string & request) { requests.push_back (request); });
            DiskLogDelta               delta;



            pane.Configure();
            pane.SetShown (true);
            list.SetVisible (true);
            list.SetRect    (RECT { 0, 0, 600, 400 });

            delta.isReset  = true;
            delta.heldFrom = 100;
            delta.newer    = { MakeDiskRow (100, Disk2EventType::MotorCommandOn) };

            pane.Apply   (delta);
            pane.Refresh();
            pane.FollowScroll();
            pane.FollowScroll();

            Assert::AreEqual ((size_t) 2, requests.size(), L"open, then one request");
            Assert::AreEqual (std::string ("open"),    requests[0]);
            Assert::AreEqual (std::string ("earlier"), requests[1]);
            Assert::AreEqual (std::wstring (EventLogText::GetEarlierText()), GetColumn (pane, DiskLogPane::kDetail)[0]);

            delta          = DiskLogDelta();
            delta.heldFrom = 40;
            delta.older    = { MakeDiskRow (50, Disk2EventType::HeadStep, -1, -1, 20) };

            pane.Apply   (delta);
            pane.Refresh();
            pane.FollowScroll();

            Assert::AreEqual ((size_t) 3, requests.size(), L"again from the new oldest row");
        }


        TEST_METHOD (OlderRowsGoInFrontAndTheSelectionStays)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;



            pane.Configure();

            delta.isReset  = true;
            delta.heldFrom = 100;
            delta.newer    = { MakeDiskRow (100, Disk2EventType::MotorCommandOn), MakeDiskRow (110, Disk2EventType::MotorEngaged) };

            pane.Apply   (delta);
            pane.Refresh();

            list.SetSelectedRows ({ 2 }, 2);

            delta          = DiskLogDelta();
            delta.heldFrom = 10;
            delta.older    = { MakeDiskRow (10, Disk2EventType::DriveSelect), MakeDiskRow (20, Disk2EventType::DriveSelect) };

            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual ((size_t) 5, pane.GetView().size());
            Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
            Assert::AreEqual (std::wstring (L"Motor engaged"), GetColumn (pane, DiskLogPane::kEvent)[(size_t) list.GetSelectedRows()[0]]);
        }


        TEST_METHOD (ACutDropsTheRowsFromItsPosition)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeDiskRow (10, Disk2EventType::MotorCommandOn), MakeDiskRow (20, Disk2EventType::MotorEngaged), MakeDiskRow (30, Disk2EventType::MotorCommandOff) };
            pane.Apply (delta);

            delta         = DiskLogDelta();
            delta.cutFrom = 20;
            delta.newer   = { MakeDiskRow (25, Disk2EventType::HeadBump) };
            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual ((size_t) 2, pane.GetRows().GetRows().size());
            Assert::AreEqual ((uint64_t) 25, pane.GetRows().GetRows()[1].position);
        }


        TEST_METHOD (RowsFromTheMachinesPositionAreHiddenBehindLive)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeDiskRow (10, Disk2EventType::MotorCommandOn), MakeDiskRow (20, Disk2EventType::MotorEngaged), MakeDiskRow (30, Disk2EventType::MotorCommandOff) };
            pane.Apply (delta);

            pane.SetShownBefore (20);
            pane.Refresh();
            Assert::AreEqual ((size_t) 2, pane.GetView().size(), L"the start row and the row at 10");

            pane.SetShownBefore (UINT64_MAX);
            pane.Refresh();
            Assert::AreEqual ((size_t) 4, pane.GetView().size());
        }


        //  A step matches the track list by quarter-track; a mark by track;
        //  a row with no known track passes.
        TEST_METHOD (TheTrackListMatchesMarksByTrackAndStepsByQuarterTrack)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta               delta;
            std::vector<std::wstring>  details;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeDiskRow (10, Disk2EventType::AddrMark, 17, 1),
                              MakeDiskRow (20, Disk2EventType::AddrMark, 18, 1),
                              MakeDiskRow (30, Disk2EventType::HeadStep, -1, -1, 69),
                              MakeDiskRow (40, Disk2EventType::HeadStep, -1, -1, 74),
                              MakeDiskRow (50, Disk2EventType::DataRead, -1, -1) };
            pane.Apply (delta);

            pane.SetTrackText (L"17");
            pane.Refresh();
            details = GetColumn (pane, DiskLogPane::kDetail);

            Assert::AreEqual ((size_t) 4, details.size(), L"the start row, track 17's mark, the step to 17.25 and the unknown read");
            Assert::AreEqual (std::wstring (L"T17 S1 V254"),            details[1]);
            Assert::AreEqual (std::wstring (L"quarter-track 67 -> 69"), details[2]);

            pane.SetTrackText (L"18.5");
            pane.Refresh();
            Assert::AreEqual ((size_t) 3, pane.GetView().size(), L"the step to 18.5 and the unknown read");
        }


        TEST_METHOD (TheKindAndDriveFiltersHideRows)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;
            DiskLogRow     second = MakeDiskRow (20, Disk2EventType::MotorEngaged);



            pane.Configure();

            second.event.drive = 1;

            delta.isReset = true;
            delta.newer   = { MakeDiskRow (10, Disk2EventType::MotorCommandOn), second, MakeDiskRow (30, Disk2EventType::ReadStall) };
            pane.Apply (delta);

            pane.SetKindShown (DiskLogPane::kMotor, false);
            pane.Refresh();
            Assert::AreEqual ((size_t) 2, pane.GetView().size(), L"the read stall has no kind to hide it");

            pane.SetKindShown (DiskLogPane::kMotor, true);
            pane.SetDrive     (2);
            pane.Refresh();
            Assert::AreEqual ((size_t) 2, pane.GetView().size(), L"drive 2's row");
        }


        TEST_METHOD (TheInvalidTextListsTheTokensNotTaken)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});



            pane.SetTrackText  (L"1, abc, 99");
            pane.SetSectorText (L"16");

            Assert::AreEqual (std::wstring (L"Invalid track: abc, 99    Invalid sector: 16"), pane.GetInvalidText());
        }


        //  A header press sorts by the column, the next the other way, and a
        //  third goes back to position order with the start row.
        TEST_METHOD (AHeaderSortsThenReversesThenRestoresPositionOrder)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeDiskRow (10, Disk2EventType::MotorEngaged), MakeDiskRow (20, Disk2EventType::AddrMark, 3, 4), MakeDiskRow (30, Disk2EventType::DriveSelect) };
            pane.Apply (delta);

            pane.SortBy  (DiskLogPane::kEvent);
            pane.Refresh();
            Assert::IsTrue (GetColumn (pane, DiskLogPane::kEvent) == std::vector<std::wstring> { L"Address mark", L"Drive select", L"Motor engaged" });

            pane.SortBy  (DiskLogPane::kCycle);
            pane.SortBy  (DiskLogPane::kCycle);
            pane.Refresh();
            Assert::IsTrue (GetColumn (pane, DiskLogPane::kCycle) == std::vector<std::wstring> { L"300", L"200", L"100" }, L"by number, descending");

            pane.SortBy  (DiskLogPane::kCycle);
            pane.Refresh();
            Assert::AreEqual ((size_t) 4, pane.GetView().size(), L"the start row is back");
        }


        TEST_METHOD (CopyGivesTheShownColumnsOfEachRow)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeDiskRow (100, Disk2EventType::HeadBump) };
            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (std::wstring (L"1,000\t1\tInfo\tHead bump\tat quarter-track 0\r\n"), pane.GetText (false));

            pane.SetColumnShown (DiskLogPane::kDrive, false);
            Assert::AreEqual (std::wstring (L"1,000\tInfo\tHead bump\tat quarter-track 0\r\n"), pane.GetText (false));
        }


        TEST_METHOD (TheOptionsTextRoundTrips)
        {
            DxuiListView   list;
            DxuiListView   other;
            DiskLogPane    pane  (&list,  [] (const std::string &) {});
            DiskLogPane    again (&other, [] (const std::string &) {});



            pane.Configure();
            again.Configure();

            pane.SetKindShown   (DiskLogPane::kHeadStep, false);
            pane.SetDrive       (1);
            pane.SetTrackText   (L"17-20");
            pane.SetSectorText  (L"0,5");
            pane.SetColumnShown (DiskLogPane::kWall, true);

            again.SetOptionsText (pane.GetOptionsText());

            Assert::AreEqual (pane.GetOptionsText(), again.GetOptionsText());
            Assert::IsFalse  (again.IsKindShown (DiskLogPane::kHeadStep));
            Assert::AreEqual (std::wstring (L"17-20"), again.GetTrackText());
            Assert::IsTrue   (again.IsColumnShown (DiskLogPane::kWall));
        }


        //  A pair viewed as a joystick has axes X and Y and is listed under
        //  the joystick filter; viewed as paddles, its axes are paddles.
        TEST_METHOD (APairsViewNamesItsAxesAndPicksItsFilter)
        {
            DxuiListView   list;
            InputLogPane   pane (&list, [] (const std::string &) {});
            InputLogDelta  delta;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeInputRow (10, InputEventType::HostPaddle, 1, 200, true) };
            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (std::wstring (L"HOST JOY0 Y = 200"), GetColumn (pane, InputLogPane::kMeaning)[1]);

            pane.SetPairJoystick (0, false);
            pane.Refresh();
            Assert::AreEqual (std::wstring (L"HOST PDL1 = 200"), GetColumn (pane, InputLogPane::kMeaning)[1]);

            pane.SetLaneShown (InputLogPane::kPaddle, false);
            pane.Refresh();
            Assert::AreEqual ((size_t) 1, pane.GetView().size(), L"hidden with the paddles");
        }


        TEST_METHOD (TheMouseButtonOnACIsPressedWhenBitSevenIsClear)
        {
            DxuiListView   list;
            InputLogPane   pane (&list, [] (const std::string &) {});
            std::wstring   address;
            std::wstring   value;
            std::wstring   meaning;
            InputEvent     event = MakeInputRow (10, InputEventType::ButtonRead, 0xC063, 0x00).event;



            pane.FormatEvent (event, address, value, meaning);
            Assert::AreEqual (std::wstring (L"Button read $C063 -> $00  pressed=0  Shift / button 2"), meaning);

            pane.SetMouseButtonAtC063 (true);
            pane.FormatEvent (event, address, value, meaning);
            Assert::AreEqual (std::wstring (L"Button read $C063 -> $00  pressed=1  mouse button"), meaning);
        }


        TEST_METHOD (ThePaddleStrobeShowsUnderEitherGamePortFilter)
        {
            DxuiListView   list;
            InputLogPane   pane (&list, [] (const std::string &) {});
            InputLogDelta  delta;



            pane.Configure();
            pane.SetPairJoystick (0, false);

            delta.isReset = true;
            delta.newer   = { MakeInputRow (10, InputEventType::PaddleTrigger, 0xC070, 0), MakeInputRow (20, InputEventType::PaddleRead, 0xC064, 0x80) };
            pane.Apply (delta);

            pane.SetLaneShown (InputLogPane::kJoystick, false);
            pane.Refresh();
            Assert::AreEqual ((size_t) 3, pane.GetView().size(), L"the strobe and the paddle's read");

            pane.SetLaneShown (InputLogPane::kPaddle, false);
            pane.Refresh();
            Assert::AreEqual ((size_t) 1, pane.GetView().size());
        }


        //  A strobe is listed only when the next paddle row after it is a
        //  read: of a loop that strobes again before reading, or strobes and
        //  stops, only the strobe the read follows is listed.
        TEST_METHOD (OnlyAStrobeAReadFollowsShows)
        {
            DxuiListView               list;
            InputLogPane               pane (&list, [] (const std::string &) {});
            InputLogDelta              delta;
            std::vector<std::wstring>  cycles;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeInputRow (10, InputEventType::PaddleTrigger, 0xC070, 0),
                              MakeInputRow (20, InputEventType::PaddleTrigger, 0xC070, 0),
                              MakeInputRow (30, InputEventType::PaddleRead,    0xC064, 0x80),
                              MakeInputRow (40, InputEventType::PaddleTrigger, 0xC070, 0) };
            pane.Apply   (delta);
            pane.Refresh();

            cycles = GetColumn (pane, InputLogPane::kCycle);

            Assert::IsTrue (cycles == std::vector<std::wstring> { L"", L"200", L"300" }, L"the second strobe and the read");
        }
    };
}
