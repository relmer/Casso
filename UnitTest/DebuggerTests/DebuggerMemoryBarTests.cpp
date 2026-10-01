#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Widgets/DxuiHexView.h"
#include "Widgets/DxuiToolbar.h"

#include "CppUnitTest.h"

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
        void  RunDebuggerCommand      (const std::string &)                      override {}
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

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

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


        TEST_METHOD (ColumnsAndGroupingSetTheActiveWindow)
        {
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
