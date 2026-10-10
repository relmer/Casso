#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/MemoryBarCommands.h"

#include "CppUnitTest.h"
#include "Core/TextEncoding.h"

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
        using DebuggerWindow::SubmitMemoryBox;
        using DebuggerWindow::GetMemoryBox;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::GetMemoryBar;
        using DebuggerWindow::GetMemoryHistory;
        using DebuggerWindow::RunMemoryBarEntry;
        using DebuggerWindow::ChooseMemoryColumns;
        using DebuggerWindow::ChooseMemoryGrouping;
        using DebuggerWindow::GetFindBarTip;
        using DebuggerWindow::GetFindBox;
        using DebuggerWindow::GetFindControls;
        using DebuggerWindow::IsFindOpen;

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

        TEST_METHOD (TheBarHoldsVisualStudiosEntriesAndNoPokeOrCloseButton)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost     host;
            MemoryBarWindow   window (theme, host);
            std::vector<int>  ids;



            window.Build();

            for (int i = 0; i < window.GetMemoryBar()->GetEntryCount(); i++)
            {
                ids.push_back (window.GetMemoryBar()->GetEntryCommandId (i));
            }

            for (int id : { MemoryBarCommands::kAddress, MemoryBarCommands::kRefresh, MemoryBarCommands::kColumns,
                            MemoryBarCommands::kGrouping })
            {
                Assert::IsTrue (std::find (ids.begin(), ids.end(), id) != ids.end(), std::format (L"entry {} is on the bar", id).c_str());
            }

            Assert::IsNull (window.FindButton (L"- Memory"), L"a window closes from its tab");
            Assert::IsNull (window.FindButton (L"Poke"),     L"memory is written in the view or from the console");
        }


        TEST_METHOD (EnteredAddressesAreKeptNewestFirstAndOnce)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);



            window.Build();

            for (const wchar_t * text : { L"0300", L"PC", L"0300" })
            {
                window.GetMemoryBox()->SetText (text);
                window.SubmitMemoryBox();
            }

            Assert::AreEqual ((size_t) 2,           window.GetMemoryHistory().size());
            Assert::AreEqual (std::wstring (L"0300"), window.GetMemoryHistory()[0], L"the newest first");
            Assert::AreEqual (std::wstring (L"PC"),   window.GetMemoryHistory()[1]);
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
            window.FocusControl   (window.GetMemoryBox());

            for (const DxuiKeyEvent & key : { overDown, space, enter })
            {
                window.GetMemoryBox()->SetText (L"");
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


        TEST_METHOD (ColumnsAndGroupingSetTheActiveWindow)        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);



            window.Build();

            window.ChooseMemoryColumns (4);
            Assert::AreEqual (4, window.GetFirstHexView()->GetBytesPerRow(), L"four bytes a row");

            window.ChooseMemoryGrouping (2);
            Assert::AreEqual (8, window.GetFirstHexView()->GetBytesPerRow(), L"four words a row");

            window.ChooseMemoryColumns (0);
            Assert::AreEqual (0, window.GetFirstHexView()->GetColumns(), L"Auto fits the width");
        }


        TEST_METHOD (EveryMemoryBarEntryHasATip)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MemoryBarHost    host;
            MemoryBarWindow  window (theme, host);
            DxuiToolbar    * strip  = nullptr;



            window.Build();
            strip = window.GetMemoryBar();

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

            Assert::IsTrue (window.IsFindOpen(), L"Ctrl+F opens the find bar");

            for (IDxuiControl * control : window.GetFindControls())
            {
                RECT             anchor = {};
                const wchar_t  * tip    = nullptr;

                Assert::IsNotNull (control);

                tip = window.GetFindBarTip (GetCenter (control->GetBounds()), anchor);

                Assert::IsTrue (tip != nullptr && *tip != L'\0', (L"a tip for " + control->GetAccessibleName()).c_str());
            }
        }
    };
}
