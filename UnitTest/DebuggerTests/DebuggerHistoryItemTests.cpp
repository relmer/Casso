#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerHistoryItemTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HistoryItemDebuggerHost
    //
    //  A host that keeps nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class HistoryItemDebuggerHost : public IDebuggerWindowHost
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

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }
        bool  IsReplayingHistory()                                               override { return isReplaying; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        bool  isReplaying = false;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HistoryItemWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class HistoryItemWindow : public DebuggerWindow
    {
    public:
        HistoryItemWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::FitHistoryField;
        using DebuggerWindow::UpdateStatusBar;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::kStatusHistory;

        void  Build()
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1100, 840 }, scaler);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerHistoryItemTests
    //
    //  The status bar's history item: when history began and how much of the
    //  buffer is left while it fills, a width fixed from the start at its
    //  longest text, and a thin bar under the text in place of a fill.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerHistoryItemTests)
    {
    public:

        static HistoryStatus  MakeFilling (size_t used, uint64_t wallTime)
        {
            HistoryStatus  status;

            status.isRecording   = true;
            status.hasHistory    = true;
            status.usedBytes     = used;
            status.budgetBytes   = 100;
            status.beginWallTime = wallTime;
            return status;
        }


        TEST_METHOD (WhileFilling_TheTextGivesTheBeginTimeAndTheBufferLeft)
        {
            std::wstring  text = DebuggerStatusText::GetHistoryText (MakeFilling (63, 134000000000000000ull), L"en-US");

            Assert::IsTrue (text.starts_with (L"History since "),              text.c_str());
            Assert::IsTrue (text.ends_with (L"M, buffer remaining 37%"),       text.c_str());

            text = DebuggerStatusText::GetHistoryText (MakeFilling (0, 134000000000000000ull), L"de-DE");
            Assert::IsTrue (text.ends_with (L", buffer remaining 100%"),       text.c_str());
            Assert::IsTrue (text.find (L"M,") == std::wstring::npos,          text.c_str());
        }


        TEST_METHOD (WhileFillingWithoutAHostTime_TheTextGivesTheBufferLeft)
        {
            Assert::AreEqual (std::wstring (L"History buffer remaining 37%"), DebuggerStatusText::GetHistoryText (MakeFilling (63, 0), L"en-US"));
        }


        TEST_METHOD (FitTexts_HoldEveryFormAtItsLongest)
        {
            std::vector<std::wstring>  texts = DebuggerStatusText::GetHistoryFitTexts (L"en-US");
            bool                       full  = false;
            bool                       since = false;
            bool                       off   = false;



            for (const std::wstring & text : texts)
            {
                full  |= text.starts_with (L"History begins at 11:58:58 PM (Power + 99:59:59, cycle 9,999,999,999,999)");
                since |= text == L"History since 10:58:58 AM, buffer remaining 100%";
                off   |= text == L"History off";
            }

            Assert::IsTrue (full,  L"the full form at its longest");
            Assert::IsTrue (since, L"the filling form at every hour");
            Assert::IsTrue (off,   L"the item off");
        }


        TEST_METHOD (MeasureFieldWidth_IsTheWidestTextPlusPadding)
        {
            MockDxuiTextRenderer  text;

            text.SetCannedMetrics (L"short",   SIZE { 40, 16 });
            text.SetCannedMetrics (L"longest", SIZE { 300, 16 });

            Assert::AreEqual (300 + 2 * DxuiStatusBar::kFieldPadDip, DxuiStatusBar::MeasureFieldWidthDip (text, { L"short", L"longest" }));

            text.SetMeasureReturnsZero (true);
            Assert::AreEqual (0, DxuiStatusBar::MeasureFieldWidthDip (text, { L"short" }));
        }


        TEST_METHOD (TheHistoryItemIsFittedToItsLongestText)
        {
            CassoTheme               theme = CassoTheme::MakeSkeuomorphic();
            HistoryItemDebuggerHost  host;
            HistoryItemWindow        window (theme, host);
            MockDxuiTextRenderer     text;
            int                      want  = DxuiStatusBar::MeasureFieldWidthDip (text, DebuggerStatusText::GetHistoryFitTexts (LOCALE_NAME_USER_DEFAULT));



            window.Build();
            window.FitHistoryField (text);

            Assert::IsTrue (want > 0);
            Assert::AreEqual (want, window.GetStatusBar()->GetField (window.kStatusHistory).widthDip);
        }


        TEST_METHOD (WhileFilling_TheItemHasABarAndNoFill)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            HistoryItemDebuggerHost                host;
            HistoryItemWindow                      window   (theme, host);
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();



            window.Build();
            snapshot->history = MakeFilling (40, 0);
            window.SetSnapshotForTest (snapshot);
            window.UpdateStatusBar();

            const DxuiStatusBar::Field &  history = window.GetStatusBar()->GetField (window.kStatusHistory);

            Assert::AreEqual (0.4f,  history.bar, 0.001f);
            Assert::AreEqual (-1.0f, history.fill);
        }


        TEST_METHOD (TheBarIsAThinGradientAlongTheBottomUnderPlainText)
        {
            DxuiStatusBar         bar;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiDpiScaler         scaler;
            int                   gradients = 0;
            int                   strings   = 0;



            scaler.SetDpi (192);
            bar.SetFields ({ { L"History", 200, false } });
            bar.Layout (RECT { 0, 0, 400, 48 }, scaler);
            bar.SetBar (0, 0.5f, 0xFF00FF00u, 0xFF0000FFu);
            bar.Paint (painter, text, theme);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind != RecordedPaintKind::FillHorizontalGradientRect)
                {
                    continue;
                }

                gradients++;
                Assert::AreEqual (6.0f,   call.height);
                Assert::AreEqual (38.0f,  call.y);
                Assert::AreEqual (16.0f,  call.x);
                Assert::AreEqual (184.0f, call.width);
                Assert::AreEqual (0xFF0000FFu, call.argbSecond);
            }

            for (const RecordedTextCall & call : text.Calls())
            {
                strings += (call.kind == RecordedTextKind::DrawString) ? 1 : 0;
            }

            Assert::AreEqual (1, gradients, L"one thin bar");
            Assert::AreEqual (1, strings,   L"the text once, with no shadow");
        }
    };
}
