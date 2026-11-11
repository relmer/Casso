#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Ui/Debugger/DebuggerIcons.h"

#include "CppUnitTest.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/Panes/MemoryWindows.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryBarHost
    //
    //  A host that keeps the memory windows asked for and the Go to text sent.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MemoryBarHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { lines.push_back (line); }
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int id, std::optional<Word>)              override { windows.push_back (id); }
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string & text)            override { goTos.push_back (text); }
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode) override { lines.push_back (line); }
        void  RunDebuggerAction        (const DebuggerAction & action)         override { lines.push_back (action.echo); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<int>          windows;
        std::vector<std::string>  goTos;
        std::vector<std::string>  lines;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryBarWindow
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MemoryBarWindow : public DebuggerWindow
    {
    public:
        MemoryBarWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::RouteMappedKey;
        using DebuggerWindow::ApplyKeyScheme;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::GetMemoryGroup;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::GetFindWidgets;

        void  Build()
        {
            DxuiDpiScaler                          scaler;
            auto                                   snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::MemoryWindow     memory;



            scaler.SetDpi (kDpi);
            OnCreate();
            ApplyKeyScheme (DebuggerKeyScheme::VisualStudio);

            snapshot->machine  = "Apple2e";
            snapshot->isPaused = true;
            memory.id          = 1;
            snapshot->memoryWindows.push_back (memory);
            TakeSnapshot (snapshot);

            Layout (RECT { 0, 0, kWidth, kHeight }, scaler);
        }

        void  Relayout()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (kDpi);
            Layout (RECT { 0, 0, kWidth, kHeight }, scaler);
        }

        DxuiHexView *  GetFirstHexView() const
        {
            for (size_t i = 0; i < GetChildCount(); i++)
            {
                if (auto * hex = dynamic_cast<DxuiHexView *> (GetChild (i)))
                {
                    return hex;
                }
            }

            return nullptr;
        }

        DxuiButton *  FindButton (const std::wstring & label) const
        {
            for (size_t i = 0; i < GetChildCount(); i++)
            {
                auto * button = dynamic_cast<DxuiButton *> (GetChild (i));

                if (button != nullptr && button->GetAccessibleName() == label)
                {
                    return button;
                }
            }

            return nullptr;
        }

        static constexpr int   kDpi    = 96;
        static constexpr LONG  kWidth  = 1600;
        static constexpr LONG  kHeight = 900;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerMemoryBarTests
    //
    //  The memory window's bar as Visual Studio's: an Address box with a history
    //  of what was entered, Refresh, Columns, the grouping and New memory
    //  window, on a toolbar whose every entry has a tip. No poke box and no
    //  button to close a window; the tab closes it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerMemoryBarTests)
    {
    public:

        static POINT GetCenter (const RECT & rc)
        {
            return POINT { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        }


        static DxuiMouseEvent MakeMove (int x, int y)
        {
            DxuiMouseEvent  ev;

            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = POINT { x, y };
            return ev;
        }

        //  The Heat toggle is the flame icon alone, in the ink the bar's other
        //  icons are drawn in.
        TEST_METHOD (TheHeatToggleIsTheFlameIcon)
        {
            MemoryBarCommands                commands ({});
            std::vector<DxuiToolbar::Entry>  entries  = commands.BuildEntries (nullptr);
            auto                             heat     = std::ranges::find_if (entries, [] (const DxuiToolbar::Entry & each) { return each.command->id == MemoryBarCommands::kHeat; });



            Assert::IsTrue (heat != entries.end());
            Assert::IsTrue (heat->vectorIcon != nullptr && std::wstring (heat->vectorIcon->pathData) == DebuggerIcons::s_kHeat.pathData, L"the flame");
            Assert::IsTrue (heat->iconOnly, L"without its label");
            Assert::IsTrue (heat->kind == DxuiToolbar::Kind::Toggle);

            for (size_t i = 0; i < DebuggerIcons::s_kHeat.layerCount; i++)
            {
                Assert::IsFalse (DebuggerIcons::s_kHeat.layers[i].accent, L"no accent ink");
            }
        }


        TEST_METHOD (TheBarHoldsVisualStudiosEntriesAndNoPokeOrCloseButton)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost     host;
            MemoryBarWindow   window (theme, host);
            std::vector<int>  ids;



            window.Build();

            for (int i = 0; i < window.GetMemoryGroup().GetBar()->GetEntryCount(); i++)
            {
                ids.push_back (window.GetMemoryGroup().GetBar()->GetEntryCommandId (i));
            }

            std::erase_if (ids, [] (int id) { return id < MemoryBarCommands::kAddress || id > MemoryBarCommands::kHeat; });

            Assert::IsTrue (ids == std::vector<int> { MemoryBarCommands::kUndo, MemoryBarCommands::kRedo, MemoryBarCommands::kAddress,
                                                      MemoryBarCommands::kHeat, MemoryBarCommands::kRefresh },
                            L"Undo, Redo, the address box, Heat and Refresh, in that order; Columns is on the right-click menu");

            Assert::IsNull (window.FindButton (L"- Memory"), L"a window closes from its tab");
            Assert::IsNull (window.FindButton (L"Poke"),     L"memory is written in the view or from the console");
        }


        //  The memory window's menu starts with the hex view's, as Casso
        //  Explorer shows it: the value sizes and formats checked as they are
        //  set, the Columns choices in their submenu, and Copy, disabled with
        //  nothing selected.
        TEST_METHOD (TheViewsOwnMenuBecomesThePaneMenusRows)
        {
            DxuiHexView                     view;
            std::vector<DxuiPopupMenuItem>  from;
            PaneMenuItems                   rows;
            auto                            find = [&rows] (const std::wstring & label)
            {
                return std::ranges::find_if (rows, [&label] (const auto & row) { return row.first == label; });
            };



            (void) view.SetGrouping (2);
            view.BuildContextMenu (from);
            MemoryWindowGroup::AppendViewItems (from, std::wstring(), rows);

            Assert::IsTrue (find (std::wstring (1, PaneMenu::kChecked) + L"2-byte integer") != rows.end(), L"the grouping in force is checked");
            Assert::IsTrue (find (L"1-byte integer") != rows.end(), L"another is not");
            Assert::AreEqual ((ptrdiff_t) 6, std::ranges::count_if (rows, [] (const auto & row) { return row.first.starts_with (PaneMenu::MakeInSubmenu (L"Columns", L"")); }), L"the six Columns choices are in their submenu");
            Assert::IsTrue (find (L"Copy") != rows.end() && !find (L"Copy")->second, L"Copy, with nothing selected, is disabled");

            find (L"1-byte integer")->second();
            Assert::AreEqual (1, view.GetGrouping(), L"a row does what the view's own menu does");
        }

        //  The undo history the window opens with is the machine's the first
        //  snapshot is from, so that snapshot clears none of it; a later
        //  change of machine does.
        TEST_METHOD (ARestoredUndoHistoryOutlastsTheFirstSnapshot)
        {
            class RestoringHost : public MemoryBarHost
            {
            public:
                std::string  GetDebuggerWindowState (const std::string & name) override
                {
                    return (name == "undo") ? std::string ("memory1=0300:00:A9|\n") : std::string();
                }
            };

            CassoTheme                          theme  = CassoTheme::MakeSkeuomorphic();
            RestoringHost                       host;
            MemoryBarWindow                     window (theme, host);
            auto                                other  = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::MemoryWindow  memory;



            window.Build();
            Assert::IsTrue  (window.GetMemoryGroup().IsBarEnabled (MemoryBarCommands::kUndo), L"the restored edit is there to undo");

            other->machine  = "Apple2c";
            other->isPaused = true;
            memory.id       = 1;
            other->memoryWindows.push_back (memory);
            window.TakeSnapshot (other);
            Assert::IsFalse (window.GetMemoryGroup().IsBarEnabled (MemoryBarCommands::kUndo), L"another machine's snapshot clears it");
        }


        TEST_METHOD (EnteredAddressesAreKeptNewestFirstAndOnce)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);



            window.Build();

            for (const wchar_t * text : { L"0300", L"PC", L"0300" })
            {
                window.GetMemoryGroup().GetBox()->SetText (text);
                window.GetMemoryGroup().SubmitBox();
            }

            Assert::AreEqual ((size_t) 2,           window.GetMemoryGroup().GetHistory().size());
            Assert::AreEqual (std::wstring (L"0300"), window.GetMemoryGroup().GetHistory()[0], L"the newest first");
            Assert::AreEqual (std::wstring (L"PC"),   window.GetMemoryGroup().GetHistory()[1]);
            Assert::AreEqual ((size_t) 1,           host.goTos.size(), L"PC still goes through the host, as the Go to box did");
        }


        //  Only the console steps on Space or runs on Enter from an empty line,
        //  as AppleWin's does. The Address box is not a command line: an empty
        //  one keeps both keys, so starting an address with a space sends no
        //  step to the console.
        TEST_METHOD (AnEmptyAddressBoxKeepsSpaceAndEnter)
        {
            CassoTheme       theme     = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);
            DxuiKeyEvent     space     = { DxuiKeyEventKind::Down, VK_SPACE,  false, false, false, false };
            DxuiKeyEvent     overDown  = { DxuiKeyEventKind::Down, VK_SPACE,  false, true,  false, false };
            DxuiKeyEvent     enter     = { DxuiKeyEventKind::Down, VK_RETURN, false, false, false, false };



            window.Build();
            window.ApplyKeyScheme (DebuggerKeyScheme::AppleWin);
            window.FocusControl   (window.GetMemoryGroup().GetBox());

            for (const DxuiKeyEvent & key : { overDown, space, enter })
            {
                window.GetMemoryGroup().GetBox()->SetText (L"");
                (void) (window.OnKey (key) || window.RouteMappedKey (key));
            }

            Assert::AreEqual ((size_t) 0, host.lines.size(), host.lines.empty() ? L"" : TextEncoding::NarrowToWide (host.lines[0]).c_str());
        }


        //  Over a byte the memory window shows its address at once, in either
        //  column, and a press that starts a selection takes the tip away.
        TEST_METHOD (TheAddressUnderThePointerShowsAtOnce)
        {
            constexpr int    kCellWidthDip  = 8;
            constexpr int    kCellHeightDip = 16;
            CassoTheme       theme          = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);
            DxuiHexView    * view           = nullptr;
            RECT             hex            = {};
            RECT             text           = {};
            DxuiMouseEvent   press;



            window.Build();
            view = window.GetFirstHexView();
            Assert::IsNotNull (view);

            //  No device measures the face here, so the cell is given.
            view->SetCellSizeDip (kCellWidthDip, kCellHeightDip);
            window.Relayout();

            hex  = view->GetByteRect (view->GetTopRow() * (uint64_t) view->GetBytesPerRow() + 3, DxuiHexView::Column::Hex);
            text = view->GetByteRect (view->GetTopRow() * (uint64_t) view->GetBytesPerRow() + 3, DxuiHexView::Column::Text);

            (void) window.OnMouse (MakeMove (hex.left + 1, hex.top + 1));
            Assert::IsTrue   (hex.right > hex.left, L"the byte is on screen");
            Assert::IsTrue   (window.GetTooltip().IsVisible(), L"no dwell");
            Assert::IsTrue   (window.GetTooltip().GetText().starts_with (L"$"), window.GetTooltip().GetText().c_str());
            Assert::IsTrue   (window.GetTooltip().GetText().size() == 5 || window.GetTooltip().GetText()[5] == L' ', L"a four-digit address");

            (void) window.OnMouse (MakeMove (text.left + 1, text.top + 1));
            Assert::IsTrue   (window.GetTooltip().IsVisible(), L"the text column too");

            press.kind        = DxuiMouseEventKind::Down;
            press.button      = DxuiMouseButton::Left;
            press.positionDip = POINT { hex.left + 1, hex.top + 1 };
            (void) window.OnMouse (press);
            Assert::IsFalse  (window.GetTooltip().IsVisible(), L"a press hides it");
        }


        //  The tip follows the pointer across the bytes: one balloon, raised
        //  once and moved with the pointer, its text replaced in place, and
        //  placed against the pointer rather than snapped to each byte.
        TEST_METHOD (TheAddressTipFollowsThePointerWithoutBeingRaisedAgain)
        {
            constexpr int    kCellWidthDip  = 8;
            constexpr int    kCellHeightDip = 16;
            CassoTheme       theme          = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);
            DxuiHexView    * view           = nullptr;
            uint64_t         first          = 0;
            RECT             hex            = {};
            RECT             next           = {};
            int              shown          = 0;
            std::wstring     firstText;



            window.Build();
            view = window.GetFirstHexView();
            Assert::IsNotNull (view);

            view->SetCellSizeDip (kCellWidthDip, kCellHeightDip);
            window.Relayout();

            first = view->GetTopRow() * (uint64_t) view->GetBytesPerRow();
            hex   = view->GetByteRect (first + 3, DxuiHexView::Column::Hex);
            next  = view->GetByteRect (first + 4, DxuiHexView::Column::Hex);

            (void) window.OnMouse (MakeMove (hex.left + 1, hex.top + 1));
            Assert::IsTrue (window.GetTooltip().IsVisible());

            shown     = window.GetTooltip().GetShowCount();
            firstText = window.GetTooltip().GetText();

            Assert::AreEqual (hex.left + 1, window.GetTooltip().GetAnchor().left, L"at the pointer, not the byte's edge");

            (void) window.OnMouse (MakeMove (hex.left + 3, hex.top + 2));
            (void) window.OnMouse (MakeMove (next.left + 1, next.top + 1));

            Assert::AreEqual (shown, window.GetTooltip().GetShowCount(), L"the balloon was hidden and raised again");
            Assert::AreEqual (next.left + 1, window.GetTooltip().GetAnchor().left, L"the tip moved with the pointer");
            Assert::AreEqual (next.top  + 1, window.GetTooltip().GetAnchor().top);
            Assert::IsTrue   (firstText != window.GetTooltip().GetText(), L"the next byte's address, in place");
        }


        //  A tip that waited its dwell and is up moves to the next part's
        //  anchor with the next part's text in the same window, rather than
        //  being closed and raised again, which flickered over the
        //  Mockingboard panel's parts.
        TEST_METHOD (ATipThatIsUpMovesToTheNextPartInPlace)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);
            DxuiTooltip    & tip    = const_cast<DxuiTooltip &> (window.GetTooltip());   // the window has only a const accessor
            int              shown  = 0;



            window.Build();

            tip.SetDwellOpenMs (100);
            tip.RequestShow (RECT { 10, 10, 11, 11 }, L"Channel A", 0);
            tip.Tick (100);
            Assert::IsTrue (tip.IsVisible());

            shown = tip.GetShowCount();

            tip.RequestShow (RECT { 40, 10, 41, 11 }, L"Channel A", 150);
            tip.RequestShow (RECT { 80, 30, 81, 31 }, L"Channel B", 200);

            Assert::AreEqual (shown, tip.GetShowCount(), L"the same window, not a new one");
            Assert::AreEqual (std::wstring (L"Channel B"), tip.GetText());
            Assert::AreEqual ((LONG) 80, tip.GetAnchor().left);
            Assert::IsTrue   (tip.IsVisible());
        }


        TEST_METHOD (ColumnsAndTheViewsGroupingSizeTheActiveWindow)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);



            window.Build();

            window.GetMemoryGroup().ChooseColumns (4);
            Assert::AreEqual (4, window.GetFirstHexView()->GetBytesPerRow(), L"four bytes a row");

            (void) window.GetFirstHexView()->SetGrouping (2);
            Assert::AreEqual (8, window.GetFirstHexView()->GetBytesPerRow(), L"four words a row");

            window.GetMemoryGroup().ChooseColumns (0);
            Assert::AreEqual (0, window.GetFirstHexView()->GetColumns(), L"Auto fits the width");
        }


        TEST_METHOD (EveryMemoryBarEntryHasATip)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);
            DxuiToolbar    * strip  = nullptr;



            window.Build();
            strip = window.GetMemoryGroup().GetBar();

            Assert::IsTrue (strip->IsVisible(), L"the bar shows over the open memory window");

            for (int i = 0; i < strip->GetEntryCount(); i++)
            {
                RECT             entry  = {};
                RECT             anchor = {};
                POINT            at     = {};
                const wchar_t  * tip    = nullptr;
                int              id     = strip->GetEntryCommandId (i);

                //  Undo and Redo are disabled with nothing to act on, and a disabled
                //  button gives no tip.
                if (id == MemoryBarCommands::kUndo || id == MemoryBarCommands::kRedo)
                {
                    continue;
                }

                if (!strip->IsEntryShown (i) || !strip->TryGetEntryRect (id, entry))
                {
                    continue;
                }

                at  = GetCenter (entry);
                tip = strip->GetTooltipAt (at.x, at.y, anchor);

                Assert::IsTrue (tip != nullptr && *tip != L'\0', std::format (L"entry {} has a tip", id).c_str());
            }
        }


        TEST_METHOD (EveryFindBarControlHasATip)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);
            DxuiKeyEvent     find   = { DxuiKeyEventKind::Down, 'F', false, false, true, false };



            window.Build();
            (void) (window.OnKey (find) || window.RouteMappedKey (find));
            window.Relayout();

            Assert::IsTrue (window.GetFindWidgets().IsOpen(), L"Ctrl+F opens the find bar");

            for (IDxuiControl * control : window.GetFindWidgets().GetActiveControls())
            {
                DxuiTip  tip;

                Assert::IsNotNull (control);
                Assert::IsTrue    (static_cast<const IDxuiControl &> (window).TryGetTip (GetCenter (control->GetBounds()), tip) && !tip.text.empty(),
                                   (L"a tip for " + control->GetAccessibleName()).c_str());
            }
        }
    };
}
