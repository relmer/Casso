#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Widgets/DxuiHexView.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextView.h"
#include "Widgets/DxuiToolbar.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  QuietDebuggerHost
    //
    //  A host that keeps nothing and answers every question with nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class QuietDebuggerHost : public IDebuggerWindowHost
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
        void  GoToDebuggerMemory      (int, const std::string & text)            override { goTos.push_back (text); }
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  goTos;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TextSizeWindow
    //
    //  The debugger window with its controls built and no HWND: OnCreate makes
    //  every pane's control, docked or not, and a text-size change is applied
    //  as the keys apply it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TextSizeWindow : public DebuggerWindow
    {
    public:
        TextSizeWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::ApplyTextZoom;
        using DebuggerWindow::StepTextZoom;
        using DebuggerWindow::GetTextZoom;
        using DebuggerWindow::SubmitMemoryBox;
        using DebuggerWindow::GetMemoryBox;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowTextSizeTests
    //
    //  Ctrl+Plus, Ctrl+Minus and Ctrl+0 size the text of every content pane and
    //  nothing else. The controls are found by walking the window's children
    //  rather than through the list the size change itself uses, so a pane that
    //  list leaves out is caught. A floating pane is not floated here -- its
    //  window needs an HWND -- but it is the same control in another host, and
    //  the walk reaches it all the same.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowTextSizeTests)
    {
    public:

        TEST_METHOD (ATextSizeChangeReachesEveryContentPaneAndNothingElse)
        {
            static constexpr float  kZoom    = 1.5f;
            CassoTheme              theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost       host;
            TextSizeWindow          window   (theme, host);
            DxuiDpiScaler           scaler;
            DxuiToolbar           * bar      = nullptr;
            RECT                    barRect  = {};
            RECT                    barAfter = {};
            int                     lists    = 0;
            std::vector<float>      before;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            for (size_t i = 0; i < window.GetChildCount(); i++)
            {
                IDxuiControl  * child = window.GetChild (i);

                if (auto * list = dynamic_cast<DxuiListView *> (child))
                {
                    before.push_back (list->GetFontSizeDip());
                }
                else if (auto * toolbar = dynamic_cast<DxuiToolbar *> (child))
                {
                    bar     = toolbar;
                    barRect = toolbar->GetBounds();
                }
            }

            Assert::IsNotNull (bar, L"the command bar is one of the window's controls");

            window.ApplyTextZoom (kZoom);

            for (size_t i = 0; i < window.GetChildCount(); i++)
            {
                IDxuiControl  * child = window.GetChild (i);

                if (auto * list = dynamic_cast<DxuiListView *> (child))
                {
                    Assert::AreEqual (before[(size_t) lists] * kZoom, list->GetFontSizeDip(), 0.01f,
                                      std::format (L"list {} of the window's lists grew", lists).c_str());
                    lists++;
                }
                else if (auto * hex = dynamic_cast<DxuiHexView *> (child))
                {
                    Assert::AreEqual (kZoom, hex->GetZoom(), 0.001f, L"every memory window, open or not");
                }
                else if (auto * text = dynamic_cast<DxuiTextView *> (child))
                {
                    Assert::AreEqual (kZoom, text->GetZoom(), 0.001f, L"the source and the console");
                }
            }

            barAfter = bar->GetBounds();

            Assert::IsTrue (lists > 10,                                L"the code views, the panes and the device panels are all lists");
            Assert::IsTrue (EqualRect (&barRect, &barAfter) != FALSE, L"the command bar keeps its size");
        }





        TEST_METHOD (StepsBackFromALimitLandOnTheUsualSizes)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            for (int i = 0; i < 40; i++) { window.StepTextZoom (+1); }
            for (int i = 0; i < 20; i++) { window.StepTextZoom (-1); }

            Assert::AreEqual (1.0f, window.GetTextZoom(), 0.001f, L"up past the largest size and back down");

            for (int i = 0; i < 20; i++) { window.StepTextZoom (-1); }
            for (int i = 0; i < 5;  i++) { window.StepTextZoom (+1); }

            Assert::AreEqual (1.0f, window.GetTextZoom(), 0.001f, L"down past the smallest size and back up");
        }





        TEST_METHOD (EachStepIsTenPercentagePointsFromHalfToThreeTimes)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            window.StepTextZoom (+1);
            Assert::AreEqual (1.1f, window.GetTextZoom(), 0.001f, L"one step up");

            window.StepTextZoom (+1);
            Assert::AreEqual (1.2f, window.GetTextZoom(), 0.001f, L"two steps up add, not multiply");

            for (int i = 0; i < 40; i++) { window.StepTextZoom (+1); }

            Assert::AreEqual (3.0f, window.GetTextZoom(), 0.001f, L"the largest size");

            for (int i = 0; i < 40; i++) { window.StepTextZoom (-1); }

            Assert::AreEqual (0.5f, window.GetTextZoom(), 0.001f, L"the smallest size");

            window.StepTextZoom (+1);
            Assert::AreEqual (0.6f, window.GetTextZoom(), 0.001f, L"one step up from the smallest");
        }





        TEST_METHOD (CtrlPlusMinusAndZeroStepAndReset)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;
            DxuiKeyEvent        plus   = { DxuiKeyEventKind::Down, VK_OEM_PLUS,  false, false, true, false };
            DxuiKeyEvent        minus  = { DxuiKeyEventKind::Down, VK_OEM_MINUS, false, false, true, false };
            DxuiKeyEvent        zero   = { DxuiKeyEventKind::Down, '0',          false, false, true, false };



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            Assert::IsTrue (window.OnKey (plus));
            Assert::IsTrue (window.OnKey (plus));
            Assert::IsTrue (window.OnKey (plus));
            Assert::AreEqual (1.3f, window.GetTextZoom(), 0.001f, L"three steps up with Ctrl+Plus");

            Assert::IsTrue (window.OnKey (minus));
            Assert::AreEqual (1.2f, window.GetTextZoom(), 0.001f, L"one back with Ctrl+Minus");

            Assert::IsTrue (window.OnKey (zero));
            Assert::AreEqual (1.0f, window.GetTextZoom(), 0.001f, L"Ctrl+0 back to the usual size");
        }





        TEST_METHOD (GoToSendsRegisterAForResolutionAndIgnoresABlankBox)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            window.OnCreate();

            window.GetMemoryBox()->SetText (L"  ");
            window.SubmitMemoryBox();

            Assert::AreEqual ((size_t) 0, host.goTos.size(), L"a blank box goes nowhere");

            window.GetMemoryBox()->SetText (L"a");
            window.SubmitMemoryBox();

            Assert::AreEqual ((size_t) 1, host.goTos.size(), L"A is the register, resolved against the machine");
            Assert::AreEqual (std::string ("a"), host.goTos[0]);
        }
    };
}
