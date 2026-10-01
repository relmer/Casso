#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiDarkTheme.h"
#include "Theme/DxuiLightTheme.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerFindWidgetLayerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LayerHost
    //
    ////////////////////////////////////////////////////////////////////////////////

    class LayerHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode)        override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LayerWindow
    //
    //  The debugger window with no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class LayerWindow : public DebuggerWindow
    {
    public:
        LayerWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::AppendConsole;
        using DebuggerWindow::CloseFind;
        using DebuggerWindow::GetFindBox;
        using DebuggerWindow::HasTopLayer;
        using DebuggerWindow::OpenFindIn;

        void  Build()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1100, 840 }, scaler);
            AppendConsole ({ "LDA #$00", "STA $C030" });
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerFindWidgetLayerTests
    //
    //  An open find widget paints in the top layer, so its plate covers the
    //  text of the pane under it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerFindWidgetLayerTests)
    {
    public:

        TEST_METHOD (AnOpenFindWidgetIsInTheTopLayer)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            LayerHost    host;
            LayerWindow  window (theme, host);



            window.Build();
            Assert::IsFalse (window.HasTopLayer(), L"nothing floats before find opens");

            window.OpenFindIn (DebuggerLayout::kConsole);
            Assert::IsTrue (window.HasTopLayer(), L"the open widget paints over the pane's text");

            window.CloseFind();
            Assert::IsFalse (window.HasTopLayer(), L"a closed widget leaves the top layer");
        }
    };
}