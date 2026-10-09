#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerDockDragTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DragHost
    //
    //  A host that does nothing; the tests read only the window's own state.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DragHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                      override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)         override {}
        void  RunDebuggerAction        (const DebuggerAction &)                   override {}
        void  RunEmulatorCommand       (int)                                      override {}
        void  PauseDebugger            ()                                         override {}
        void  SetDebuggerCodeLines     (int, int)                                 override {}
        void  SetDebuggerCodeAddress   (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop       (Word, int)                                override {}
        void  SetDebuggerFollowView    (int)                                      override {}
        void  CloseDebuggerCodeView    (int)                                      override {}
        void  SetDebuggerTraceTop      (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory       (int, const std::string &)                 override {}
        void  ScrollDebuggerCode       (int, int)                                 override {}
        void  OnDebuggerWindowClosed   ()                                         override {}
        void  SetDebuggerKeyScheme     (const std::string &)                      override {}
        void  SetDebuggerLayout        (const std::string &)                      override {}
        void  SetDebuggerOpenViews     (const std::string &)                      override {}
        void  SetDebuggerPlacement     (const RECT &)                             override {}
        void  SetDebuggerMemoryWindow  (int, std::optional<Word>)                 override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        FakeHostDialogs  dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DragWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DragWindow : public DebuggerWindow
    {
    public:
        static constexpr int  kWidth  = 1400;
        static constexpr int  kHeight = 900;

        DragWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnMouse;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::GetCommandBar;
        using DebuggerWindow::GetDockSite;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::HasDragLayer;
        using DebuggerWindow::PaintDragLayer;

        void  Build()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, kWidth, kHeight }, scaler);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerDockDragTests
    //
    //  A pane dragged in the debugger takes every mouse event until the button
    //  comes up: over the command bar its cross and shade go at once and no
    //  tip shows. Escape cancels it, leaving every pane where it was, and its
    //  shade paints in the drag layer, over the panes' pictures.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerDockDragTests)
    {
    public:

        static void  Send (DragWindow & window, DxuiMouseEventKind kind, POINT at)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = at;

            (void) window.OnMouse (ev);
        }


        static POINT  GetCenter (const RECT & rect)
        {
            return POINT { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
        }


        //  The first group of `kind` holding at least `tabs` panes, or null.
        static DxuiTabGroup *  FindGroup (DragWindow & window, DxuiTabGroup::Kind kind, size_t tabs)
        {
            DxuiDockSite  * site = window.GetDockSite();



            for (size_t i = 0; i < site->GetGroupCount(); i++)
            {
                if (site->GetGroup (i)->IsVisible() && site->GetGroup (i)->GetKind() == kind && site->GetGroup (i)->GetTabCount() >= tabs)
                {
                    return site->GetGroup (i);
                }
            }

            return nullptr;
        }


        //  A tool window group of several panes dragged by its title bar, then
        //  held over the middle of the disassembly's group, its cross's center
        //  button; `documents` is that point.
        static bool  TryDragOverTheDocuments (DragWindow & window, POINT & documents)
        {
            DxuiTabGroup  * tools = FindGroup (window, DxuiTabGroup::Kind::ToolWindow, 2);
            DxuiTabGroup  * code  = FindGroup (window, DxuiTabGroup::Kind::Document,   1);
            RECT            title = {};



            if (tools == nullptr || code == nullptr)
            {
                return false;
            }

            title     = tools->GetTitleRect();
            documents = GetCenter (code->GetBounds());

            Send (window, DxuiMouseEventKind::Down, POINT { title.left + 20, GetCenter (title).y });
            Send (window, DxuiMouseEventKind::Move, POINT { title.left + 60, GetCenter (title).y });
            Send (window, DxuiMouseEventKind::Move, documents);

            return window.GetDockSite()->HasPointerDrag() && window.GetDockSite()->GetHoveredZone() != nullptr;
        }


        static size_t  CountGuides (DragWindow & window, const IDxuiTheme & theme)
        {
            size_t  guides = 0;



            for (const DxuiDockDragMark & mark : window.GetDockSite()->GetDragMarks (theme))
            {
                guides += (mark.image != nullptr) ? 1 : 0;
            }

            return guides;
        }


        static size_t  CountShades (const MockDxuiPainter & painter, const IDxuiTheme & theme)
        {
            return (size_t) std::count_if (painter.Calls().begin(), painter.Calls().end(), [&theme] (const RecordedPaintCall & call)
            {
                return call.kind == RecordedPaintKind::FillRect && call.argb == theme.DockPreview();
            });
        }


        //  Over the command bar, Break's button under the pointer: the drag
        //  targets nothing and shows only its edge guides, and the move is
        //  not routed to the bar, so no tip is on its way.
        TEST_METHOD (OverTheCommandBarADragShowsNoCrossAndNoTip)
        {
            CassoTheme  theme     = CassoTheme::MakeSkeuomorphic();
            DragHost    host;
            DragWindow  window (theme, host);
            POINT       documents = {};
            RECT        brk       = {};



            window.Build();

            Assert::IsTrue (TryDragOverTheDocuments (window, documents), L"the drag is over the documents' center button");
            Assert::IsTrue (window.GetCommandBar()->TryGetEntryRect (DebuggerCommands::kPause, brk));

            Send (window, DxuiMouseEventKind::Move, GetCenter (brk));

            Assert::IsTrue   (window.GetDockSite()->IsDragging(),        L"the drag goes on over the bar");
            Assert::IsNull   (window.GetDockSite()->GetHoveredZone(),    L"and targets nothing there");
            Assert::AreEqual ((size_t) 4, CountGuides (window, theme),   L"the edge guides alone, no cross");
            Assert::IsFalse  (window.GetTooltip().WantsTick(),           L"no tip is on its way over Break");
            Assert::IsFalse  (window.GetTooltip().IsVisible());
        }


        //  Escape cancels the drag over the documents' center button, and the
        //  release there docks nothing.
        TEST_METHOD (EscapeCancelsAPaneDragAndLeavesTheLayout)
        {
            CassoTheme    theme     = CassoTheme::MakeSkeuomorphic();
            DragHost      host;
            DragWindow    window (theme, host);
            POINT         documents = {};
            DxuiKeyEvent  escape;
            std::wstring  before;



            window.Build();
            before = window.GetDockSite()->GetPaneLayout().ToText();

            Assert::IsTrue (TryDragOverTheDocuments (window, documents));

            escape.kind = DxuiKeyEventKind::Down;
            escape.vk   = VK_ESCAPE;

            Assert::IsTrue  (window.OnKey (escape),              L"the window takes Escape");
            Assert::IsFalse (window.GetDockSite()->IsDragging(), L"and cancels the drag");
            Assert::IsFalse (window.HasDragLayer(),              L"so nothing is left to paint");

            Send (window, DxuiMouseEventKind::Up, documents);

            Assert::AreEqual (before, window.GetDockSite()->GetPaneLayout().ToText(), L"every pane is where it was");
        }


        //  The page's pass over the site fills no shade, which the panes'
        //  pictures would cover; the drag layer fills it, and draws the
        //  guides, after the page.
        TEST_METHOD (TheShadePaintsInTheDragLayer)
        {
            CassoTheme            theme     = CassoTheme::MakeSkeuomorphic();
            DragHost              host;
            DragWindow            window (theme, host);
            POINT                 documents = {};
            MockDxuiPainter       page;
            MockDxuiTextRenderer  pageText;
            MockDxuiPainter       layer;
            MockDxuiTextRenderer  layerText;



            window.Build();

            Assert::IsTrue (TryDragOverTheDocuments (window, documents));

            window.GetDockSite()->Paint              (page, pageText, theme);
            window.GetDockSite()->PaintAfterSiblings (page, pageText, theme);

            Assert::AreEqual ((size_t) 0, CountShades (page, theme), L"the page fills no shade");
            Assert::IsTrue   (window.HasDragLayer(),                 L"the window flushes a drag layer");

            window.PaintDragLayer (layer, layerText, theme);

            Assert::IsTrue   (CountShades (layer, theme) > 0,                            L"which fills the shade");
            Assert::AreEqual (CountGuides (window, theme), layerText.IconCalls().size(), L"and draws the guides");
        }
    };
}
