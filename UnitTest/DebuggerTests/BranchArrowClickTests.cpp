#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace BranchArrowClickTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ArrowHost
    //
    //  A host that finds a long main.a65 and adds each address the window
    //  moves a code view to, none for the PC, to addresses.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ArrowHost : public NullDebuggerWindowHost
    {
    public:
        static constexpr int  kSourceLines = 200;

        void  SetDebuggerCodeAddress (std::optional<Word> address, int) override { addresses.push_back (address); }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override
        {
            SourceLookup  lookup;



            lookup.match = SourceMatch::Exact;
            lookup.path  = L"C:\\Work\\main.a65";

            for (int line = 0; line < kSourceLines; line++)
            {
                lookup.text += "        nop\n";
            }

            return lookup;
        }

        std::vector<std::optional<Word>>  addresses;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ArrowWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ArrowWindow : public DebuggerWindow
    {
    public:
        ArrowWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            DxuiDpiScaler  scaler;



            m_theme = &theme;
            m_host  = &host;

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }

        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::Layout;
        using DebuggerWindow::GetCodeGroup;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::GetSourceGroup;
        using DebuggerWindow::OnMouse;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BranchArrowClickTests
    //
    //  A click on the PC's branch arrow goes to its target and a click on the
    //  dotted arrow goes to the branch the last step took, the next click on
    //  either going back to the PC; in a source document the click scrolls
    //  the document.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BranchArrowClickTests)
    {
    public:
        static constexpr Word  kBranchedFrom = 0x0300;
        static constexpr Word  kPc           = 0x0304;
        static constexpr Word  kPcTarget     = 0x0310;
        static constexpr int   kCellPx       = 16;


        //  Stopped at a BNE to $0310, brought there by the BEQ at $0300, every
        //  line from $0300 to $0312 in the first view.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeCodeSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  line;



            for (Word address = 0x0300; address <= 0x0312; address += 2)
            {
                line                = DebuggerViewSnapshot::CodeLine();
                line.address        = address;
                line.bytes          = "EA";
                line.instruction    = "NOP";
                line.isCurrent      = address == kPc;
                line.isBranchedFrom = address == kBranchedFrom;

                if (address == kBranchedFrom)
                {
                    line.bytes       = "F0 02";
                    line.instruction = "BEQ $0304";
                    line.target      = kPc;
                }
                else if (address == kPc)
                {
                    line.bytes       = "D0 0A";
                    line.instruction = "BNE $0310";
                    line.target      = kPcTarget;
                }

                snapshot->codeViews[0].push_back (line);
            }

            snapshot->isPaused        = true;
            snapshot->codeOpen[0]     = true;
            snapshot->code            = snapshot->codeViews[0];
            snapshot->pc              = kPc;
            snapshot->pcTarget        = kPcTarget;
            snapshot->isPcTargetTaken = true;
            snapshot->branchedFrom    = kBranchedFrom;
            return snapshot;
        }


        //  The window stopped at the BNE, its first view painted so its cells
        //  are measured.
        static void  ShowCode (ArrowWindow & window, const CassoTheme & theme)
        {
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;



            window.TakeSnapshot (MakeCodeSnapshot());
            window.GetCodeList (0)->Paint (painter, text, theme);
        }


        //  The points of a view where each arrow is hit: on the PC's alone, on
        //  the dotted one alone, and on both.
        struct ArrowPoints
        {
            std::optional<POINT>  pcOnly;
            std::optional<POINT>  dottedOnly;
            std::optional<POINT>  both;
        };


        static ArrowPoints  FindArrowPoints (ArrowWindow & window, int view)
        {
            ArrowPoints         points;
            BranchArrow::Input  pc;
            BranchArrow::Input  dotted;
            Word                goesTo  = 0;
            bool                isTaken = true;
            bool                hasPc   = false;
            bool                hasDot  = false;
            bool                onPc    = false;
            bool                onDot   = false;
            RECT                bounds  = window.GetCodeList (0)->GetBounds();



            hasPc  = window.GetCodeGroup().GetBranchArrow (view, pc, goesTo, isTaken, false);
            hasDot = window.GetCodeGroup().GetBranchArrow (view, dotted, goesTo, isTaken, true);

            for (LONG y = bounds.top; y < bounds.bottom; y++)
            {
                for (LONG x = bounds.left; x < bounds.right; x++)
                {
                    onPc  = hasPc  && BranchArrow::HitTest (pc,     (float) x, (float) y);
                    onDot = hasDot && BranchArrow::HitTest (dotted, (float) x, (float) y);

                    if (onPc && onDot && !points.both.has_value())
                    {
                        points.both = POINT { x, y };
                    }
                    else if (onPc && !onDot && !points.pcOnly.has_value())
                    {
                        points.pcOnly = POINT { x, y };
                    }
                    else if (onDot && !onPc && !points.dottedOnly.has_value())
                    {
                        points.dottedOnly = POINT { x, y };
                    }
                }
            }

            return points;
        }


        //  A point on a source document's arrow, or nullopt when no arrow is drawn.
        static std::optional<POINT>  FindSourceArrowPoint (ArrowWindow & window)
        {
            BranchArrow::Input  input;
            Word                goesTo  = 0;
            bool                isTaken = true;
            RECT                bounds  = window.GetSourceGroup().GetDocument (0).GetView()->GetBounds();



            if (!window.GetCodeGroup().GetBranchArrow (DebuggerViewState::kMaxCodeViews, input, goesTo, isTaken))
            {
                return std::nullopt;
            }

            for (LONG y = bounds.top; y < bounds.bottom; y++)
            {
                for (LONG x = bounds.left; x < bounds.right; x++)
                {
                    if (BranchArrow::HitTest (input, (float) x, (float) y))
                    {
                        return POINT { x, y };
                    }
                }
            }

            return std::nullopt;
        }


        static void  Click (ArrowWindow & window, POINT at)
        {
            DxuiMouseEvent  ev;



            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = at;

            for (DxuiMouseEventKind kind : { DxuiMouseEventKind::Down, DxuiMouseEventKind::Up })
            {
                ev.kind = kind;
                (void) window.OnMouse (ev);
            }
        }


        //  A click on the PC's arrow goes to the BNE's target, and the next
        //  click on it goes back to the PC.
        TEST_METHOD (AClickOnThePcsArrowGoesToItsTargetAndTheNextBack)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            ArrowHost    host;
            ArrowWindow  window (theme, host);
            ArrowPoints  points;



            ShowCode (window, theme);
            points = FindArrowPoints (window, 0);
            Assert::IsTrue (points.pcOnly.has_value(), L"the PC's arrow is drawn in the view");

            Click (window, *points.pcOnly);
            Assert::AreEqual ((size_t) 1, host.addresses.size(), L"the click moves the view");
            Assert::IsTrue   (host.addresses.back() == std::optional<Word> (kPcTarget), L"to the BNE's target");

            Click (window, *points.pcOnly);
            Assert::AreEqual ((size_t) 2, host.addresses.size());
            Assert::IsFalse  (host.addresses.back().has_value(), L"the next click goes back to the PC");
        }


        //  A click on the dotted arrow goes to the branch the last step took,
        //  also where it lies over the PC's arrow; the next click goes back to
        //  the PC.
        TEST_METHOD (AClickOnTheDottedArrowGoesToTheBranchTakenEvenWhereTheyOverlap)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            ArrowHost    host;
            ArrowWindow  window (theme, host);
            ArrowPoints  points;



            ShowCode (window, theme);
            points = FindArrowPoints (window, 0);
            Assert::IsTrue (points.dottedOnly.has_value(), L"the dotted arrow is drawn in the view");
            Assert::IsTrue (points.both.has_value(),       L"the two arrows overlap at the PC's row");

            Click (window, *points.dottedOnly);
            Assert::IsTrue  (host.addresses.back() == std::optional<Word> (kBranchedFrom), L"the dotted arrow goes to the BEQ");

            Click (window, *points.dottedOnly);
            Assert::IsFalse (host.addresses.back().has_value(), L"and the next click back to the PC");

            Click (window, *points.both);
            Assert::IsTrue  (host.addresses.back() == std::optional<Word> (kBranchedFrom), L"where they overlap, the dotted arrow's branch");

            Click (window, *points.both);
            Assert::IsFalse (host.addresses.back().has_value(), L"and the next click back to the PC");
        }


        //  Stopped at line 2 of main.a65, at $0300, whose branch goes to
        //  `target`, which line 150 starts at.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeSourceSnapshot (Word target)
        {
            auto             snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebugSourceFile  file;



            file.id   = 0;
            file.name = "main.a65";

            snapshot->isPaused              = true;
            snapshot->codeOpen[0]           = true;
            snapshot->pc                    = 0x0300;
            snapshot->source                = DebuggerViewSnapshot::SourceState();
            snapshot->source->debugFilePath = L"C:\\Work\\main.dbg";
            snapshot->source->programKey    = "key";
            snapshot->source->fileId        = 0;
            snapshot->source->line          = 2;
            snapshot->source->pcTarget      = target;
            snapshot->source->lineAddresses = std::make_shared<const std::map<std::pair<int, int>, Word>> (
                                                  std::map<std::pair<int, int>, Word> { { { 0, 2 }, 0x0300 }, { { 0, 150 }, 0x0400 } });
            snapshot->source->files.push_back (file);
            return snapshot;
        }


        //  The window with main.a65's document in front, painted so its lines
        //  are measured, and the document's view.
        static DxuiTextView *  OpenDocument (ArrowWindow & window, const CassoTheme & theme, Word target)
        {
            DxuiDpiScaler          scaler;
            MockDxuiPainter        painter;
            MockDxuiTextRenderer   text;
            DxuiTextView         * view = nullptr;



            scaler.SetDpi (96);
            window.TakeSnapshot (MakeSourceSnapshot (target));
            window.GetSourceGroup().Activate (0);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            view = window.GetSourceGroup().GetDocument (0).GetView();
            Assert::IsTrue (view->IsVisible(), L"the document is in front of its group");

            view->SetCellSize (8, kCellPx);
            view->Paint (painter, text, theme);
            return view;
        }


        //  In a source document, a click on the arrow scrolls the document to
        //  the target's line, and the next click scrolls it back to the PC's.
        //  The disassembly isn't moved or brought forward.
        TEST_METHOD (InASourceDocumentTheClickScrollsTheDocument)
        {
            CassoTheme             theme  = CassoTheme::MakeSkeuomorphic();
            ArrowHost              host;
            ArrowWindow            window (theme, host);
            DxuiTextView         * view   = OpenDocument (window, theme, 0x0400);
            std::optional<POINT>   at;



            Assert::AreEqual (0, view->GetTopLine(), L"the PC's line is in view");

            at = FindSourceArrowPoint (window);
            Assert::IsTrue (at.has_value(), L"the document draws the arrow");

            Click (window, *at);
            Assert::IsTrue (view->GetTopLine() > 0 && view->GetTopLine() < 150 && 150 - view->GetTopLine() < view->GetLineCap(),
                            L"line 150 is scrolled into view");
            Assert::IsTrue (host.addresses.empty(), L"the disassembly isn't moved");
            Assert::IsTrue (view->IsVisible(), L"the document is in front");

            at = FindSourceArrowPoint (window);
            Assert::IsTrue (at.has_value(), L"the arrow runs on from the PC's line out of view");

            Click (window, *at);
            Assert::AreEqual (0, view->GetTopLine(), L"the next click scrolls back to the PC's line");
            Assert::IsTrue   (host.addresses.empty());
        }
    };
}
