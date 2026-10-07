#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace HeatMapRangesPaneTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RangesHost
    //
    //  A host that keeps the ranges' setting as the user's settings would.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class RangesHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        std::string  GetDebuggerHeatMapRanges()                          override { return ranges; }
        void         SetDebuggerHeatMapRanges (const std::string & text) override { ranges = text; }

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::string  ranges;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RangesWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class RangesWindow : public DebuggerWindow
    {
    public:
        RangesWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetViewMenuPanes;
        using DebuggerWindow::GetHeatMapView;
        using DebuggerWindow::GetHeatMapBar;
        using DebuggerWindow::GetHeatMapBarLabel;
        using DebuggerWindow::ApplyHeatMap;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::GetHeatRanges;
        using DebuggerWindow::GetEditedRangeSet;
        using DebuggerWindow::GetHeatRangeList;
        using DebuggerWindow::GetHeatRangeEditor;
        using DebuggerWindow::GetHeatRangeError;
        using DebuggerWindow::IsEditingHeatRange;
        using DebuggerWindow::ShowRangeSet;
        using DebuggerWindow::EditRangeSet;
        using DebuggerWindow::RunHeatRangeBarEntry;
        using DebuggerWindow::IsHeatRangeBarEnabled;
        using DebuggerWindow::BeginHeatRangeEdit;
        using DebuggerWindow::EndHeatRangeEdit;
        using DebuggerWindow::ToggleHeatRange;
        using DebuggerWindow::GetRangeSetCommands;
        using DebuggerWindow::FocusControl;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HeatMapRangesPaneTests
    //
    //  The pane that edits the heat map's sets of ranges: closed until first
    //  opened, from Edit ranges or the View menu; a set made, named and
    //  chosen; ranges added, edited in place, checked and reordered, with
    //  what does not read refused and said; the built-in ranges kept; every
    //  change kept by the host; and the heat map's drop-down showing a set.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HeatMapRangesPaneTests)
    {
    public:

        using Field = HeatMapRangeSets::Field;

        //  Tall enough that the memory windows' group, at the foot of the
        //  default layout, has room for the list's rows.
        static constexpr int  kWidth  = 1400;
        static constexpr int  kHeight = 2400;

        static void Build (RangesWindow & window)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWidth, kHeight }, scaler);
        }

        static void Relayout (RangesWindow & window)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.Layout (RECT { 0, 0, kWidth, kHeight }, scaler);
        }

        //  Built, the pane open, and one set made and named.
        static void BuildWithSet (RangesWindow & window, const std::wstring & name)
        {
            Build (window);
            window.ShowPane (DebuggerLayout::kHeatRanges);
            Relayout (window);

            window.RunHeatRangeBarEntry (HeatMapRangeBarCommands::kNewSet);
            window.GetHeatRangeEditor()->SetText (name);
            window.EndHeatRangeEdit (true);
        }

        //  A new range typed into the start of a new row.
        static void AddRange (RangesWindow & window, const std::wstring & text)
        {
            window.RunHeatRangeBarEntry (HeatMapRangeBarCommands::kNew);
            window.GetHeatRangeEditor()->SetText (text);
            window.EndHeatRangeEdit (true);
        }

        //  The row of the list whose name cell reads a name.
        static int FindRow (RangesWindow & window, const std::wstring & name)
        {
            DxuiListView  * list = window.GetHeatRangeList();



            for (int row = 0; row < list->GetRowCount(); row++)
            {
                if (list->GetCellsOfRow (row)[0].text == name)
                {
                    return row;
                }
            }

            return -1;
        }

        static std::wstring GetCell (RangesWindow & window, int row, size_t column)
        {
            return window.GetHeatRangeList()->GetCellsOfRow (row)[column].text;
        }



        TEST_METHOD (ThePaneOpensClosedUntilOpenedOnceAndTheViewMenuListsIt)
        {
            CassoTheme                 theme = CassoTheme::MakeSkeuomorphic();
            RangesHost                 host;
            RangesWindow               window (theme, host);
            std::unique_ptr<RangesWindow>  again = std::make_unique<RangesWindow> (theme, host);
            std::vector<std::wstring>      panes;



            Build (window);
            panes = window.GetViewMenuPanes();

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kHeatRanges), L"closed by default");
            Assert::IsTrue  (std::ranges::find (panes, std::wstring (DebuggerLayout::kHeatRanges)) != panes.end(), L"the View menu lists it");

            window.ShowPane (DebuggerLayout::kHeatRanges);
            Assert::IsTrue  (window.IsPaneShown (DebuggerLayout::kHeatRanges));
            Assert::IsFalse (host.ranges.empty(), L"opening it keeps its setting");

            Build (*again);
            Assert::IsTrue  (again->IsPaneShown (DebuggerLayout::kHeatRanges), L"so it opens next time");
        }



        TEST_METHOD (EditRangesOnTheHeatMapBarOpensThePane)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            RangesHost     host;
            RangesWindow   window (theme, host);
            constexpr int   kWideWindow = 3000;
            DxuiDpiScaler   scaler;
            RECT            entry       = {};
            POINT           at          = {};
            DxuiMouseEvent  ev;



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatMap);
            scaler.SetDpi (96);
            window.Layout (RECT { 0, 0, kWideWindow, 900 }, scaler);

            Assert::IsFalse (window.GetHeatMapBar()->IsInSeeMore (HeatMapBarCommands::kEditRanges), L"room for it on the strip");
            Assert::IsTrue  (window.GetHeatMapBar()->TryGetEntryRect (HeatMapBarCommands::kEditRanges, entry), L"Edit ranges is on the bar");

            at             = { (entry.left + entry.right) / 2, (entry.top + entry.bottom) / 2 };
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = at;

            for (DxuiMouseEventKind kind : { DxuiMouseEventKind::Move, DxuiMouseEventKind::Down, DxuiMouseEventKind::Up })
            {
                ev.kind = kind;
                (void) window.OnMouse (ev);
            }

            Assert::IsTrue (window.IsPaneShown (DebuggerLayout::kHeatRanges));
        }



        TEST_METHOD (ANewSetIsNamedInPlaceAndKept)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            RangesHost     host;
            RangesWindow   window (theme, host);



            Build (window);
            window.ShowPane (DebuggerLayout::kHeatRanges);
            Relayout (window);

            Assert::IsFalse  (window.IsHeatRangeBarEnabled (HeatMapRangeBarCommands::kNew), L"no set, no range to add");

            window.RunHeatRangeBarEntry (HeatMapRangeBarCommands::kNewSet);

            Assert::IsTrue   (window.IsEditingHeatRange(), L"its name opens for typing");
            Assert::AreEqual (std::wstring (L"Set 1"), window.GetHeatRangeEditor()->GetText());

            window.GetHeatRangeEditor()->SetText (L"All memory");
            window.EndHeatRangeEdit (true);
            Assert::IsTrue   (window.IsEditingHeatRange(), L"a name the drop-down uses is refused, the box left open");
            Assert::IsTrue   (window.GetHeatRangeError().starts_with (L"Invalid set name: "), window.GetHeatRangeError().c_str());

            window.GetHeatRangeEditor()->SetText (L"Game");
            window.EndHeatRangeEdit (true);

            Assert::IsFalse  (window.IsEditingHeatRange());
            Assert::IsTrue   (window.GetHeatRangeError().empty(), L"the reason goes with the edit");
            Assert::AreEqual (std::string ("Game"), window.GetEditedRangeSet());
            Assert::IsTrue   (host.ranges.find ("set \"Game\"") != std::string::npos, L"kept");
            Assert::AreEqual (HeatMapRangeSets::GetPresets().size() - 1, (size_t) window.GetHeatRangeList()->GetRowCount(),
                              L"every built-in range listed, My program aside with no symbols");
        }



        TEST_METHOD (ARangeIsAddedEditedInPlaceAndShownAsItReads)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            RangesHost     host;
            RangesWindow   window (theme, host);
            int            row    = -1;



            BuildWithSet (window, L"Game");
            AddRange (window, L"$6000-$95FF");

            row = window.GetHeatRangeList()->GetRowCount() - 1;

            Assert::IsFalse  (window.IsEditingHeatRange());
            Assert::AreEqual (std::wstring (L"$6000"), GetCell (window, row, 1));
            Assert::AreEqual (std::wstring (L"$95FF"), GetCell (window, row, 2));
            Assert::AreEqual (std::wstring (L"$3600"), GetCell (window, row, 3));
            Assert::AreEqual (std::wstring (L""),      GetCell (window, row, 4), L"not built in");

            window.BeginHeatRangeEdit (row, Field::Name);
            window.GetHeatRangeEditor()->SetText (L"Sprites");
            window.EndHeatRangeEdit (true);
            Assert::AreEqual (row, FindRow (window, L"Sprites"), L"named in place");

            window.BeginHeatRangeEdit (row, Field::Size);
            window.GetHeatRangeEditor()->SetText (L"$100");
            window.EndHeatRangeEdit (true);
            Assert::AreEqual (std::wstring (L"$60FF  +$100"), GetCell (window, row, 2), L"a size, its length shown beside the end");

            Assert::IsTrue   (host.ranges.find ("range on \"Sprites\" \"$6000\" \"+$100\"") != std::string::npos, L"kept as typed");
        }



        TEST_METHOD (WhatDoesNotReadIsRefusedWithTheReasonAndANewRowAbandonedGoes)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            RangesHost     host;
            RangesWindow   window (theme, host);
            int            rows   = 0;



            BuildWithSet (window, L"Game");
            rows = window.GetHeatRangeList()->GetRowCount();

            window.RunHeatRangeBarEntry (HeatMapRangeBarCommands::kNew);
            window.GetHeatRangeEditor()->SetText (L"NOSUCH..+4");
            window.EndHeatRangeEdit (true);

            Assert::IsTrue   (window.IsEditingHeatRange(), L"the box stays open over what was typed");
            Assert::AreEqual (std::wstring (L"NOSUCH..+4"), window.GetHeatRangeEditor()->GetText());
            Assert::IsTrue   (window.GetHeatRangeError().starts_with (L"Invalid range: "), window.GetHeatRangeError().c_str());

            window.GetHeatRangeEditor()->SetText (L"$2000-$1000");
            window.EndHeatRangeEdit (true);
            Assert::IsTrue   (window.GetHeatRangeError().find (L"before its start") != std::wstring::npos, window.GetHeatRangeError().c_str());

            window.EndHeatRangeEdit (false);
            Assert::IsFalse  (window.IsEditingHeatRange());
            Assert::AreEqual (rows, window.GetHeatRangeList()->GetRowCount(), L"Escape takes the new row away");
            Assert::IsTrue   (window.GetHeatRangeError().empty());
        }



        TEST_METHOD (ABuiltInRangeIsCheckedInOrOutButNotEditedOrRemoved)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            RangesHost     host;
            RangesWindow   window (theme, host);
            int            row    = -1;



            BuildWithSet (window, L"Game");
            row = FindRow (window, L"Hi-res page 1");

            Assert::IsTrue   (row >= 0);
            Assert::AreEqual (std::wstring (L"Yes"), GetCell (window, row, 4));
            Assert::IsFalse  (window.GetHeatRangeList()->GetCellsOfRow (row)[0].check.value_or (true), L"left out to start");

            window.GetHeatRangeList()->SetSelectedRow (row);
            Assert::IsFalse  (window.IsHeatRangeBarEnabled (HeatMapRangeBarCommands::kRemove), L"no Remove");
            Assert::IsFalse  (window.IsHeatRangeBarEnabled (HeatMapRangeBarCommands::kEdit),   L"no Edit");
            Assert::IsTrue   (window.IsHeatRangeBarEnabled (HeatMapRangeBarCommands::kUp),     L"but it moves");

            window.RunHeatRangeBarEntry (HeatMapRangeBarCommands::kRemove);
            window.BeginHeatRangeEdit (row, Field::Start);
            Assert::IsFalse  (window.IsEditingHeatRange(), L"no box opens over it");
            Assert::AreEqual (row, FindRow (window, L"Hi-res page 1"), L"still there");

            window.ToggleHeatRange (row, true);
            Assert::IsTrue   (window.GetHeatRangeList()->GetCellsOfRow (row)[0].check.value_or (false), L"checked in");
            Assert::IsTrue   (host.ranges.find ("preset hires1 on") != std::string::npos);

            window.RunHeatRangeBarEntry (HeatMapRangeBarCommands::kUp);
            Assert::AreEqual (row - 1, FindRow (window, L"Hi-res page 1"), L"Move up");
            Assert::AreEqual (row - 1, window.GetHeatRangeList()->GetSelectedRow(), L"the selection goes with it");
        }



        TEST_METHOD (TheHeatMapsDropDownShowsASetsRangesAndAllMemory)
        {
            CassoTheme       theme = CassoTheme::MakeSkeuomorphic();
            RangesHost       host;
            RangesWindow     window (theme, host);
            int                        row    = -1;
            std::vector<std::wstring>  labels;



            BuildWithSet (window, L"Game");
            AddRange (window, L"$6000-$95FF");
            row = FindRow (window, L"Text page 1");
            window.ToggleHeatRange (row, true);

            for (const std::shared_ptr<DxuiCommand> & command : window.GetRangeSetCommands())
            {
                labels.push_back (command->label);
            }

            Assert::IsTrue   ((std::vector<std::wstring> { L"All memory", L"Game" }) == labels);
            Assert::AreEqual (std::wstring (L"All memory"), window.GetHeatMapBarLabel (HeatMapBarCommands::kRangeSet));
            Assert::IsFalse  (window.GetHeatMapView()->HasRanges());

            window.GetRangeSetCommands()[1]->dispatch();

            Assert::IsTrue   (window.GetHeatMapView()->HasRanges(), L"the map shows the set");
            Assert::AreEqual (std::wstring (L"Game"), window.GetHeatMapBarLabel (HeatMapBarCommands::kRangeSet));
            Assert::IsTrue   (window.GetHeatMapView()->GetCellRect (0x0400).bottom < window.GetHeatMapView()->GetCellRect (0x6000).top,
                              L"Text page 1 over the range added at the set's end, in the set's order");
            Assert::IsTrue   (host.ranges.starts_with ("show \"Game\""), L"kept");

            window.GetRangeSetCommands()[0]->dispatch();
            Assert::IsFalse  (window.GetHeatMapView()->HasRanges(), L"All memory again");
        }



        TEST_METHOD (TheSetsAreKeptAndReadBackInTheNextWindow)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            RangesHost     host;
            RangesWindow   window (theme, host);
            std::unique_ptr<RangesWindow>  again = std::make_unique<RangesWindow> (theme, host);



            BuildWithSet (window, L"Game");
            AddRange (window, L"$6000-$95FF");
            window.ShowRangeSet ("Game");

            Build (*again);

            Assert::IsTrue   (again->GetHeatRanges() == window.GetHeatRanges(), L"every set and the set shown");
            Assert::IsTrue   (again->GetHeatMapView()->HasRanges(), L"the map opens on the set it showed");
            Assert::AreEqual (std::string ("Game"), again->GetEditedRangeSet());
        }



        TEST_METHOD (SymbolsFromASnapshotReadARangeAndBringMyProgram)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            RangesHost                             host;
            RangesWindow                           window (theme, host);
            std::shared_ptr<HeatMapSymbols>        symbols  = std::make_shared<HeatMapSymbols>();
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();
            int                                    rows     = 0;
            int                                    row      = -1;



            BuildWithSet (window, L"Game");
            rows = window.GetHeatRangeList()->GetRowCount();

            symbols->Add ("ZP_VARS", 0x0083, 0x0010, true);
            snapshot->heatMapSymbols = symbols;
            window.SetSnapshotForTest (snapshot);
            window.ApplyHeatMap();

            Assert::AreEqual (rows + 1, window.GetHeatRangeList()->GetRowCount(), L"My program, with a program loaded");
            Assert::IsTrue   (FindRow (window, L"My program") >= 0);

            AddRange (window, L"ZP_VARS");
            row = window.GetHeatRangeList()->GetRowCount() - 1;

            Assert::AreEqual (std::wstring (L"$0083  ZP_VARS"), GetCell (window, row, 1), L"a symbol's own size");
            Assert::AreEqual (std::wstring (L"$0092"),          GetCell (window, row, 2));
        }



        TEST_METHOD (TheListsKeysDeleteRenameAndAdd)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            RangesHost     host;
            RangesWindow   window (theme, host);
            DxuiKeyEvent   key;
            int            rows   = 0;



            BuildWithSet (window, L"Game");
            AddRange (window, L"$300:$3FF");
            rows = window.GetHeatRangeList()->GetRowCount();

            window.GetHeatRangeList()->SetSelectedRow (rows - 1);
            window.FocusControl (window.GetHeatRangeList());

            key.kind = DxuiKeyEventKind::Down;
            key.vk   = VK_F2;
            (void) window.OnKey (key);
            Assert::IsTrue  (window.IsEditingHeatRange(), L"F2 renames");

            key.vk = VK_ESCAPE;
            (void) window.OnKey (key);
            Assert::IsFalse (window.IsEditingHeatRange(), L"Escape leaves it");

            key.vk = VK_DELETE;
            (void) window.OnKey (key);
            Assert::AreEqual (rows - 1, window.GetHeatRangeList()->GetRowCount(), L"Delete removes it");
        }
    };
}





