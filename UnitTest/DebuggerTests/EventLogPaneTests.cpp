#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Ui/Debugger/Panes/DiskLogPane.h"
#include "Ui/Debugger/Panes/InputLogPane.h"
#include "Ui/Debugger/Panes/IoLogPane.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerStatusText.h"

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
            pane.SetOptionsText ("severity=101;columnLayout=00000 0,0,0,0,0");

            Assert::IsTrue  (pane.IsColumnShown (DiskLogPane::kTime),   L"five columns saved, six now");
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


        //  A Shift+click range, with rows arriving at both ends, has the same
        //  anchor and keyboard row, so the next Shift+click extends the same
        //  range; filtering the rows twice in a row reads the selection
        //  through the view it was made in.
        TEST_METHOD (ARangeExtendsFromTheSameAnchorAsRowsArrive)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;



            pane.Configure();

            delta.isReset  = true;
            delta.heldFrom = 100;
            delta.newer    = { MakeDiskRow (100, Disk2EventType::DriveSelect), MakeDiskRow (110, Disk2EventType::MotorCommandOn),
                               MakeDiskRow (120, Disk2EventType::MotorEngaged), MakeDiskRow (130, Disk2EventType::HeadBump) };
            pane.Apply   (delta);
            pane.Refresh();

            list.ClickRow (1, false, false);
            list.ClickRow (3, false, true);

            delta          = DiskLogDelta();
            delta.heldFrom = 10;
            delta.older    = { MakeDiskRow (10, Disk2EventType::DriveSelect) };
            delta.newer    = { MakeDiskRow (140, Disk2EventType::MotorCommandOff) };
            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (2, list.GetAnchorRow(),   L"the first row clicked, one lower");
            Assert::AreEqual (4, list.GetSelectedRow(), L"the Shift+click's row, one lower");

            list.ClickRow (5, false, true);
            Assert::IsTrue (list.GetSelectedRows() == std::vector<int> { 2, 3, 4, 5 }, L"the same range, one row longer");

            pane.SetKindShown (DiskLogPane::kHeadStep, false);
            pane.MarkDirty();
            pane.Refresh();
            pane.MarkDirty();
            pane.Refresh();

            Assert::IsTrue (list.GetSelectedRows() == std::vector<int> { 2, 3, 4, 5 });
            Assert::AreEqual (2, list.GetAnchorRow());
            Assert::AreEqual (5, list.GetSelectedRow());
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
            Assert::AreEqual (std::wstring (L"track 16.75 -> 17.25"), details[2]);

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


        //  Newer rows come into a sorted view in their places, with no full
        //  build, and the selected row stays selected.
        TEST_METHOD (NewerRowsGoInTheirPlacesInASortedView)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;
            size_t         builds = 0;



            pane.Configure();

            delta.isReset   = true;
            delta.isAtStart = true;
            delta.newer     = { MakeDiskRow (100, Disk2EventType::MotorEngaged), MakeDiskRow (110, Disk2EventType::DriveSelect) };

            pane.Apply   (delta);
            pane.SortBy  (DiskLogPane::kEvent);
            pane.Refresh();

            list.SetSelectedRows ({ 1 }, 1);
            builds = pane.GetBuildCount();

            delta           = DiskLogDelta();
            delta.isAtStart = true;
            delta.newer     = { MakeDiskRow (120, Disk2EventType::MotorCommandOn), MakeDiskRow (130, Disk2EventType::DriveSelect) };

            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (builds, pane.GetBuildCount(), L"no full build");
            Assert::IsTrue   (GetColumn (pane, DiskLogPane::kEvent) == std::vector<std::wstring> { L"Drive select", L"Drive select", L"Motor command on", L"Motor engaged" },
                              L"in their places");
            Assert::AreEqual (std::wstring (EventLogText::FormatCycle (1000)), GetColumn (pane, DiskLogPane::kCycle)[(size_t) list.GetSelectedRows().at (0)], L"the same row selected");
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
            pane.SetColumnShown (DiskLogPane::kTime,  false);
            pane.SetColumnShown (DiskLogPane::kCycle, true);

            Assert::AreEqual (std::wstring (L"1,000\t1\tInfo\tHead bump\tat track 0\r\n"), pane.GetText (false));

            pane.SetColumnShown (DiskLogPane::kDrive, false);
            Assert::AreEqual (std::wstring (L"1,000\tInfo\tHead bump\tat track 0\r\n"), pane.GetText (false));
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
            pane.SetColumnShown (DiskLogPane::kCycle, true);

            again.SetOptionsText (pane.GetOptionsText());

            Assert::AreEqual (pane.GetOptionsText(), again.GetOptionsText());
            Assert::IsFalse  (again.IsKindShown (DiskLogPane::kHeadStep));
            Assert::AreEqual (std::wstring (L"17-20"), again.GetTrackText());
            Assert::IsTrue   (again.IsColumnShown (DiskLogPane::kCycle));
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


        TEST_METHOD (ASerialByteShowsThePortAndTheByte)
        {
            DxuiListView   list;
            InputLogPane   pane (&list, [] (const std::string &) {});
            std::wstring   address;
            std::wstring   value;
            std::wstring   meaning;
            InputEvent     card    = MakeInputRow (10, InputEventType::HostSerialByte, 2, 'A').event;
            InputEvent     builtIn = MakeInputRow (10, InputEventType::HostSerialByte, Acia6551::kCommPortBuiltIn | 2, 'A').event;



            pane.FormatEvent (card, address, value, meaning);

            Assert::AreEqual (std::wstring (L"Slot 2"), address);
            Assert::AreEqual (L"HOST serial byte " + value, meaning);

            pane.FormatEvent (builtIn, address, value, meaning);

            Assert::AreEqual (std::wstring (L"Port 2"), address);
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


        //  A strobe at the end of one delta is listed once the read in the
        //  next delta follows it, as a full build lists it.
        TEST_METHOD (AStrobeIsListedWhenTheNextDeltasReadFollowsIt)
        {
            DxuiListView               list;
            InputLogPane               pane (&list, [] (const std::string &) {});
            InputLogDelta              delta;
            std::vector<std::wstring>  cycles;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeInputRow (10, InputEventType::PaddleRead,    0xC064, 0x80),
                              MakeInputRow (20, InputEventType::PaddleTrigger, 0xC070, 0) };
            pane.Apply   (delta);
            pane.Refresh();

            Assert::IsTrue (GetColumn (pane, InputLogPane::kCycle) == std::vector<std::wstring> { L"", L"100" }, L"no read follows the strobe yet");

            delta       = InputLogDelta();
            delta.newer = { MakeInputRow (30, InputEventType::PaddleRead, 0xC064, 0x80) };
            pane.Apply   (delta);
            pane.Refresh();

            cycles = GetColumn (pane, InputLogPane::kCycle);

            Assert::IsTrue (cycles == std::vector<std::wstring> { L"", L"100", L"200", L"300" }, L"the strobe, then the read after it");
            Assert::AreEqual (4, list.GetRowCount());
        }


        //  Newer rows extend the list with the selection on the same rows.
        TEST_METHOD (NewerRowsLeaveTheSelectionOnItsRows)
        {
            DxuiListView   list;
            DiskLogPane    pane (&list, [] (const std::string &) {});
            DiskLogDelta   delta;



            pane.Configure();

            delta.isReset = true;
            delta.newer   = { MakeDiskRow (10, Disk2EventType::MotorCommandOn), MakeDiskRow (20, Disk2EventType::MotorEngaged) };
            pane.Apply   (delta);
            pane.Refresh();

            list.SetSelectedRows ({ 1 }, 1);

            delta       = DiskLogDelta();
            delta.newer = { MakeDiskRow (30, Disk2EventType::DriveSelect), MakeDiskRow (40, Disk2EventType::MotorCommandOff) };
            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual ((size_t) 5, pane.GetView().size());
            Assert::IsTrue   (list.GetSelectedRows() == std::vector<int> { 1 });
            Assert::AreEqual (std::wstring (L"Motor command on"), GetColumn (pane, DiskLogPane::kEvent)[1]);
        }


        //  Past the most rows a pane has, the oldest are dropped, the machine's
        //  thread is sent where the rows start now, and the selected row stays
        //  selected.
        TEST_METHOD (TheOldestRowsAreDroppedPastTheMost)
        {
            DxuiListView               list;
            std::vector<std::string>   requests;
            DiskLogPane                pane (&list, [&requests] (const std::string & request) { requests.push_back (request); });
            DiskLogDelta               delta;
            size_t                     i        = 0;
            size_t                     past     = DiskLogPane::kMaxRows + 1;
            uint64_t                   oldest   = (past - DiskLogPane::kRetainedRows + 1) * 10;



            pane.Configure();
            list.SetRect (RECT { 0, 0, 600, 400 });

            delta.isReset = true;

            for (i = 1; i < past; i++)
            {
                delta.newer.push_back (MakeDiskRow (i * 10, Disk2EventType::DriveSelect));
            }

            pane.Apply   (delta);
            pane.Refresh();

            list.SetTopRow       ((int) past);
            list.SetSelectedRows ({ (int) past - 1 }, (int) past - 1);

            delta       = DiskLogDelta();
            delta.newer = { MakeDiskRow (past * 10, Disk2EventType::MotorCommandOn) };
            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (DiskLogPane::kRetainedRows, pane.GetRows().GetRows().size(), L"down to the rows retained");
            Assert::AreEqual (oldest, pane.GetRows().GetRows().front().position);
            Assert::IsFalse  (pane.GetRows().IsAtStart(), L"the start row offers to rebuild the rows dropped");
            Assert::IsTrue   (std::ranges::find (requests, std::format ("from {}", oldest)) != requests.end(), L"the machine's thread is sent the new oldest row");
            Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
            Assert::AreEqual (std::wstring (EventLogText::FormatCycle ((past - 1) * 100)), GetColumn (pane, DiskLogPane::kCycle)[(size_t) list.GetSelectedRows()[0]], L"the same row selected");
        }


        //  Rows brought in by scrolling up are not dropped as they arrive,
        //  though they take the pane past kMaxRows, while the list is on
        //  older rows.
        TEST_METHOD (RowsScrollingUpBroughtInAreNotDropped)
        {
            constexpr size_t           kOlder   = 5000;
            DxuiListView               list;
            std::vector<std::string>   requests;
            DiskLogPane                pane (&list, [&requests] (const std::string & request) { requests.push_back (request); });
            DiskLogDelta               delta;
            size_t                     i        = 0;
            uint64_t                   first    = kOlder * 10;



            pane.Configure();
            pane.SetShown (true);
            list.SetVisible (true);
            list.SetRect    (RECT { 0, 0, 600, 400 });

            delta.isReset  = true;
            delta.heldFrom = first;

            for (i = 0; i < DiskLogPane::kMaxRows - 1; i++)
            {
                delta.newer.push_back (MakeDiskRow (first + i * 10, Disk2EventType::DriveSelect));
            }

            pane.Apply   (delta);
            pane.Refresh();
            list.SetTopRow (0);
            pane.FollowScroll();
            Assert::AreEqual (std::string ("earlier"), requests.back(), L"the start row is in view");

            delta          = DiskLogDelta();
            delta.heldFrom = 0;

            for (i = 0; i < kOlder; i++)
            {
                delta.older.push_back (MakeDiskRow (i * 10, Disk2EventType::DriveSelect));
            }

            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (DiskLogPane::kMaxRows - 1 + kOlder, pane.GetRows().GetRows().size(), L"nothing dropped");
            Assert::AreEqual ((uint64_t) 0, pane.GetRows().GetRows().front().position, L"the rows brought in are first");
        }


        //  While a request for older rows is outstanding nothing is dropped,
        //  even past kHardMaxRows, since the machine's thread sends the rows
        //  before where the rows started then.
        TEST_METHOD (NothingIsDroppedWhileEarlierRowsAreRequested)
        {
            DxuiListView               list;
            std::vector<std::string>   requests;
            DiskLogPane                pane (&list, [&requests] (const std::string & request) { requests.push_back (request); });
            DiskLogDelta               delta;
            size_t                     i        = 0;
            uint64_t                   first    = 1000;



            pane.Configure();
            pane.SetShown (true);
            list.SetVisible (true);
            list.SetRect    (RECT { 0, 0, 600, 400 });

            delta.isReset  = true;
            delta.heldFrom = first;
            delta.newer    = { MakeDiskRow (first, Disk2EventType::DriveSelect) };

            pane.Apply   (delta);
            pane.Refresh();
            pane.FollowScroll();
            Assert::AreEqual (std::string ("earlier"), requests.back());

            delta          = DiskLogDelta();
            delta.heldFrom = first;

            for (i = 1; i <= DiskLogPane::kHardMaxRows; i++)
            {
                delta.newer.push_back (MakeDiskRow (first + i, Disk2EventType::DriveSelect));
            }

            pane.Apply (delta);

            Assert::AreEqual (DiskLogPane::kHardMaxRows + 1, pane.GetRows().GetRows().size(), L"nothing dropped");
            Assert::AreEqual (first, pane.GetRows().GetHeldFrom(), L"and no new start sent");
        }


        //  A frame can apply a delta of older rows and then one of newer rows
        //  before one refresh; the row at the top stays the one the user was
        //  reading.
        TEST_METHOD (TheTopRowStaysThroughTwoDeltasInAFrame)
        {
            constexpr size_t           kRows   = 100;
            constexpr size_t           kOlder  = 10;
            DxuiListView               list;
            DiskLogPane                pane (&list, [] (const std::string &) {});
            DiskLogDelta               delta;
            size_t                     i       = 0;
            uint64_t                   first   = 1000;



            pane.Configure();
            pane.SetShown (true);
            list.SetVisible (true);
            list.SetRect    (RECT { 0, 0, 600, 400 });

            delta.isReset  = true;
            delta.heldFrom = first;

            for (i = 0; i < kRows; i++)
            {
                delta.newer.push_back (MakeDiskRow (first + i, Disk2EventType::DriveSelect));
            }

            pane.Apply   (delta);
            pane.Refresh();
            list.SetTopRow (1);

            delta          = DiskLogDelta();
            delta.heldFrom = first - kOlder;

            for (i = 0; i < kOlder; i++)
            {
                delta.older.push_back (MakeDiskRow (first - kOlder + i, Disk2EventType::DriveSelect));
            }

            pane.Apply (delta);

            delta          = DiskLogDelta();
            delta.heldFrom = first - kOlder;
            delta.newer    = { MakeDiskRow (first + kRows, Disk2EventType::MotorCommandOn) };

            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (std::wstring (EventLogText::FormatCycle (first * 10)), GetColumn (pane, DiskLogPane::kCycle)[(size_t) list.GetTopRow()], L"the row at the top before");
        }


        //  The time is in the locale's format, its clock, markers and
        //  separators, with the milliseconds after the seconds.
        TEST_METHOD (TheTimeFollowsTheLocaleWithMilliseconds)
        {
            SYSTEMTIME  time = {};



            time.wYear         = 2026;
            time.wMonth        = 10;
            time.wDay          = 14;
            time.wHour         = 22;
            time.wMinute       = 42;
            time.wSecond       = 7;
            time.wMilliseconds = 45;

            Assert::AreEqual (std::wstring (L"10:42:07.045 PM"), EventLogText::FormatClockTime (time, L"en-US"));
            Assert::AreEqual (std::wstring (L"22:42:07,045"),    EventLogText::FormatClockTime (time, L"de-DE"));
        }


        TEST_METHOD (MillisecondsGoAfterTheLastSecondsFieldOutsideQuotes)
        {
            Assert::AreEqual (std::wstring (L"h:mm:ss'.123' tt"),     EventLogText::AddMilliseconds (L"h:mm:ss tt",     L".", 123));
            Assert::AreEqual (std::wstring (L"HH:mm:s',007'"),   EventLogText::AddMilliseconds (L"HH:mm:s",        L",", 7));
            Assert::AreEqual (std::wstring (L"H'h's'.500' 'sec'"),    EventLogText::AddMilliseconds (L"H'h's 'sec'",    L".", 500));
            Assert::AreEqual (std::wstring (L"HH:mm"),                EventLogText::AddMilliseconds (L"HH:mm",          L".", 1), L"no seconds, nothing added");
        }
    };




    //  A video or sound row at a position; its cycle is given.
    static IoLogRow  MakeIoRow (uint64_t position, uint64_t cycle, IoEventKind kind, Word address = 0, Byte chip = 0)
    {
        IoLogRow  row;


        row.position      = position;
        row.cycle         = cycle;
        row.event.cycle   = cycle;
        row.event.kind    = kind;
        row.event.address = address;
        row.event.chip    = chip;

        return row;
    }


    static IoLogRow  MakeGapRow (uint64_t position)
    {
        IoLogRow  row;


        row.kind     = DebugLogRowKind::Gap;
        row.position = position;

        return row;
    }




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IoLogPaneTests
    //
    //  The video and sound log panes (FR-198): a video switch's frame, beam
    //  and modes, a sound row's cycles since the last of the same as SOUNDLOG
    //  gives them, the sound log's filter by source, and its options text.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IoLogPaneTests)
    {
    public:

        TEST_METHOD (AVideoRowHasItsFrameBeamAndModes)
        {
            DxuiListView  list;
            IoLogPane     pane (&list, [] (const std::string &) {}, true);
            IoLogDelta    delta;
            IoLogRow      row = MakeIoRow (10, 1000, IoEventKind::VideoMode, 0xC057);



            row.event.cycleInFrame = 400;
            row.event.scanline     = 100;
            row.event.cycleInLine  = 30;
            row.event.turnedOn     = IoEventLog::kHires;

            pane.Configure();

            delta.isReset   = true;
            delta.isAtStart = true;
            delta.newer     = { row };

            pane.Apply   (delta);
            pane.Refresh();

            Assert::AreEqual (std::wstring (L"600"),   GetColumn (pane, IoLogPane::kFrame)[1],      L"the frame's first cycle");
            Assert::AreEqual (std::wstring (L"100"),   GetColumn (pane, IoLogPane::kLine)[1]);
            Assert::AreEqual (std::wstring (L"30"),    GetColumn (pane, IoLogPane::kHorizontal)[1]);
            Assert::AreEqual (std::wstring (L"$C057"), GetColumn (pane, IoLogPane::kAddress)[1]);
            Assert::AreEqual (TextEncoding::NarrowToWide (IoEventLog::FormatEvent (row.event)), GetColumn (pane, IoLogPane::kEvent)[1], L"as VIDEOLOG writes it");
            Assert::IsTrue   (list.IsColumnVisible (IoLogPane::kLine) && !list.IsColumnVisible (IoLogPane::kSince), L"the beam shown, the gap between writes not");
        }


        //  As SOUNDLOG: the speaker's since its last toggle, a board's since
        //  the last write to any board, and a gap starts both again.
        TEST_METHOD (ASoundRowsSinceIsFromTheLastOfTheSame)
        {
            DxuiListView                list;
            IoLogPane                   pane (&list, [] (const std::string &) {}, false);
            IoLogDelta                  delta;
            std::vector<std::wstring>   since;



            pane.Configure();

            delta.isReset   = true;
            delta.isAtStart = true;
            delta.newer     = { MakeIoRow (1, 100, IoEventKind::Speaker,  0xC030),
                                MakeIoRow (2, 150, IoEventKind::ViaWrite, 0xC401),
                                MakeIoRow (3, 400, IoEventKind::Speaker,  0xC030),
                                MakeIoRow (4, 450, IoEventKind::AyWrite),
                                MakeGapRow (5),
                                MakeIoRow (6, 900, IoEventKind::Speaker,  0xC030) };

            pane.Apply   (delta);
            pane.Refresh();

            since = GetColumn (pane, IoLogPane::kSince);

            Assert::AreEqual (std::wstring (L""),     since[1], L"the first toggle");
            Assert::AreEqual (std::wstring (L""),     since[2], L"the first board write");
            Assert::AreEqual (std::wstring (L"+300"), since[3], L"from the speaker's last toggle, not the board's write");
            Assert::AreEqual (std::wstring (L"+300"), since[4], L"from the board's last write");
            Assert::AreEqual (std::wstring (L""),     since[6], L"a gap starts it again");
            Assert::AreEqual (std::wstring (L"Speaker"),        GetColumn (pane, IoLogPane::kSource)[1]);
            Assert::AreEqual (std::wstring (L"Mockingboard 1"), GetColumn (pane, IoLogPane::kSource)[2]);
            Assert::AreEqual (std::wstring (L""),               GetColumn (pane, IoLogPane::kAddress)[4], L"an AY write has no address of its own");
        }


        //  A sort on Since orders by number, not by text, where +1000 would
        //  come before +300; the frame and the gap have thousands separators.
        TEST_METHOD (SinceSortsByNumberAndCountsAreGrouped)
        {
            DxuiListView                list;
            IoLogPane                   pane (&list, [] (const std::string &) {}, false);
            IoLogDelta                  delta;
            std::vector<std::wstring>   since;



            pane.Configure();

            delta.isReset   = true;
            delta.isAtStart = true;
            delta.newer     = { MakeIoRow (1, 100,  IoEventKind::Speaker, 0xC030),
                                MakeIoRow (2, 1400, IoEventKind::Speaker, 0xC030),
                                MakeIoRow (3, 1700, IoEventKind::Speaker, 0xC030) };

            pane.Apply   (delta);
            pane.Refresh();

            since = GetColumn (pane, IoLogPane::kSince);
            Assert::AreEqual (L"+" + DebuggerStatusText::FormatCount (1300), since[2], L"grouped as other counts are");

            pane.SortBy  (IoLogPane::kSince);
            pane.Refresh();
            since = GetColumn (pane, IoLogPane::kSince);

            Assert::IsTrue (std::ranges::find (since, std::wstring (L"+300")) < std::ranges::find (since, L"+" + DebuggerStatusText::FormatCount (1300)), L"300 before 1,300");
        }


        TEST_METHOD (TheSoundLogsFilterLeavesOutASource)
        {
            DxuiListView  list;
            IoLogPane     pane (&list, [] (const std::string &) {}, false);
            IoLogDelta    delta;



            pane.Configure();

            delta.isReset   = true;
            delta.isAtStart = true;
            delta.newer     = { MakeIoRow (1, 100, IoEventKind::Speaker,  0xC030),
                                MakeIoRow (2, 150, IoEventKind::ViaWrite, 0xC401),
                                MakeIoRow (3, 200, IoEventKind::AyWrite) };

            pane.SetLaneShown (IoLogPane::kSpeaker, false);
            pane.Apply        (delta);
            pane.Refresh();

            Assert::AreEqual ((size_t) 3, pane.GetView().size(), L"the start row and the two board rows");
            Assert::AreEqual (std::wstring (L"Mockingboard 1"), GetColumn (pane, IoLogPane::kSource)[1]);
        }


        TEST_METHOD (TheSoundLogsOptionsRoundTrip)
        {
            DxuiListView  list;
            IoLogPane     pane (&list, [] (const std::string &) {}, false);
            IoLogPane     read (&list, [] (const std::string &) {}, false);



            pane.Configure();
            pane.SetLaneShown (IoLogPane::kAy, false);

            read.SetOptionsText (pane.GetOptionsText());

            Assert::IsTrue  (read.IsLaneShown (IoLogPane::kSpeaker));
            Assert::IsTrue  (read.IsLaneShown (IoLogPane::kVia));
            Assert::IsFalse (read.IsLaneShown (IoLogPane::kAy));
        }
    };
}
