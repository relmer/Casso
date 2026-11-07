#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/OperandResultTip.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace OperandResultTipTests
{
    //  A host that does nothing; the tests read only the window's own state.
    using TipHost = NullDebuggerWindowHost;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceTipHost
    //
    //  A host with the viewing options the window starts with and one source
    //  file, main.a65, for a disassembly with source rows.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SourceTipHost : public NullDebuggerWindowHost
    {
    public:
        std::string  GetDebuggerDisassemblyOptions() override { return options; }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override
        {
            SourceLookup  lookup;



            lookup.match = SourceMatch::Exact;
            lookup.text  = sourceText;
            return lookup;
        }

        std::string  options;
        std::string  sourceText;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TipWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TipWindow : public DebuggerWindow
    {
    public:
        TipWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OperandTipCell;
        using DebuggerWindow::OperandTipScreen;
        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::GetOperandTips;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::GetDockSite;
        using DebuggerWindow::EditPaneLayout;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetColorKeys;
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::IsZoomPopupOpen;
        using DebuggerWindow::GetZoomPopupRect;
        using DebuggerWindow::kStatusZoom;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::GetTipList;
        using DebuggerWindow::kBasicTipView;
        using DebuggerWindow::FindPaneForTest;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OperandResultTipTests
    //
    //  A code row's text drawn whole over the row where its view cuts it
    //  off: only then, each line filled with the row's highlight from the
    //  row's start, the row's text where the view draws it, the operand
    //  and result below under the code, in the row's colors and italics,
    //  staying on the monitor the row starts on and the one beside it.
    //  Never over selected text or a window lying over the pane, and a color
    //  tip below it goes with it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OperandResultTipTests)
    {
    public:

        //  The mock renderer measures every character this wide.
        static constexpr int       kGlyphPx     = 7;
        static constexpr int       kRowLeft     = 20;
        static constexpr int       kTextLeft    = 100;
        static constexpr int       kCodeLeft    = 60;
        static constexpr int       kRowTop      = 40;
        static constexpr int       kRowHeight   = 16;
        static constexpr int       kPadPx       = 4;
        static constexpr int       kScreenRight = 4000;
        static constexpr int       kScreenBelow = 3000;
        static constexpr float     kFontPx      = 12.0f;
        static constexpr uint32_t  kFill        = 0xFF2A2D2E;
        static constexpr uint32_t  kInk         = 0xFFDCDCDC;
        static constexpr uint32_t  kOperand     = 0xFF57A64A;
        static constexpr uint32_t  kResult      = 0xFF4EC9B0;
        static constexpr RECT      kScreen      = { 0, 0, kScreenRight, kScreenBelow };


        //  Room for `chars` characters of text from the operand's start, to
        //  the view's edge.
        static int  GetEdgeFor (int chars)
        {
            return kTextLeft + chars * kGlyphPx;
        }


        //  A row from kRowLeft to the view's right edge, on one screen.
        static OperandResultTip::Placement  MakePlacement (int visibleRight)
        {
            OperandResultTip::Placement  placement;



            placement.row         = RECT { kRowLeft, kRowTop, visibleRight, kRowTop + kRowHeight };
            placement.visibleLeft = kRowLeft;
            placement.codeLeftPx  = (float) kCodeLeft;
            placement.padLeftPx   = kPadPx;
            placement.padRightPx  = kPadPx;
            placement.fontPx      = kFontPx;
            placement.face        = DxuiTheme::kMonoFace;
            placement.fill        = kFill;
            placement.workArea    = kScreen;
            placement.symbolInk   = kOperand;

            return placement;
        }


        //  A part of the row in `argb`, from `left`, clipped at the view's
        //  edge.
        static OperandResultTip::Segment  MakeSegment (const std::wstring & text, int left, const OperandResultTip::Placement & placement,
                                                      uint32_t argb = kInk)
        {
            OperandResultTip::Segment  segment;



            segment.text        = text;
            segment.colors.assign (text.size(), argb);
            segment.leftPx      = (float) left;
            segment.clipRightPx = (float) placement.row.right;

            return segment;
        }


        //  The disassembly's operand and result cell, from kTextLeft: the
        //  operand in its color, the result after it in the result color.
        static OperandResultTip::Segment  MakeOperand (const std::string & annotation, const std::string & effect, const OperandResultTip::Placement & placement)
        {
            DxuiListView::Cell         cell    = DisassemblyCells::GetOperandAndResultCell (annotation, effect, kResult);
            OperandResultTip::Segment  segment = MakeSegment (cell.text, kTextLeft, placement, kOperand);
            size_t                     at      = cell.text.find (SourcePane::kpszResultPrefix);



            for (const auto & [first, end, argb] : cell.colorRanges)
            {
                for (int k = first; k < end; k++)
                {
                    segment.colors[(size_t) k] = argb;
                }
            }

            segment.resultAt = (at == std::wstring::npos) ? -1 : (int) at;
            return segment;
        }


        //  A row of an instruction and its operand and result, the operand
        //  the row's last part.
        static OperandResultTip::Placement  MakeRow (const std::string & annotation, const std::string & effect, int visibleRight)
        {
            OperandResultTip::Placement  placement = MakePlacement (visibleRight);



            placement.segments.push_back (MakeSegment (L"LDA $10", kCodeLeft, placement));
            placement.segments.push_back (MakeOperand (annotation, effect, placement));
            placement.operand = 1;

            return placement;
        }


        //  Where a disassembly row's line starts, as the list highlights it:
        //  the address column's left edge, past the gutter and heat columns.
        static LONG  GetRowStart (DxuiListView & list, int row)
        {
            RECT           address = {};
            DxuiDpiScaler  scaler;



            Assert::IsTrue (list.GetCellTextRectPx (row, DisassemblyCells::kAddressColumn, address));
            return list.GetBounds().left + address.left - scaler.ToPx (list.GetCellPadLeftDip());
        }


        //  The runs on a line of the tip, left to right.
        static std::vector<OperandResultTip::Run>  GetLine (const OperandResultTip::Layout & layout, int line)
        {
            std::vector<OperandResultTip::Run>  runs;



            std::ranges::copy_if (layout.runs, std::back_inserter (runs), [line] (const OperandResultTip::Run & run) { return run.line == line; });
            return runs;
        }


        static std::wstring  GetLineText (const OperandResultTip::Layout & layout, int line)
        {
            std::vector<OperandResultTip::Run>  runs = GetLine (layout, line);



            Assert::AreEqual ((size_t) 1, runs.size(), L"one run on the line");
            return runs[0].text;
        }


        static std::vector<RecordedPaintCall>  GetFills (const MockDxuiPainter & painter)
        {
            std::vector<RecordedPaintCall>  fills;



            std::ranges::copy_if (painter.Calls(), std::back_inserter (fills), [] (const RecordedPaintCall & call) { return call.kind == RecordedPaintKind::FillRect; });
            return fills;
        }


        TEST_METHOD (AppearsOnlyWhereTheViewCutsTextOff)
        {
            OperandResultTip::Placement  placement = MakeRow ("$0010=41", "A=41", kScreenRight);
            int                          size      = (int) placement.segments[1].text.size();
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  Text that ends exactly at the view's edge fits.
            placement = MakeRow ("$0010=41", "A=41", GetEdgeFor (size));
            Assert::IsFalse (OperandResultTip::TryMakeLayout (placement, text, layout), L"a row that fits has no tip");

            placement = MakeRow ("$0010=41", "A=41", GetEdgeFor (size) - 1);
            Assert::IsTrue  (OperandResultTip::TryMakeLayout (placement, text, layout), L"a pixel short of the text is cut off");
        }


        //  Measured against the box the view clips the text to, from a start
        //  between two pixels as a text view's can be: text a pixel wider
        //  than the box has a tip, and text that fills it exactly has no tip.
        TEST_METHOD (TheTipIsUpExactlyWhenTheViewClipsTheText)
        {
            constexpr int                kTextPx   = 907;
            constexpr float              kStartPx  = (float) kTextLeft + 0.5f;

            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 $1234=41", "A=41", kScreenRight);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            text.SetCannedMetrics (placement.segments[1].text, SIZE { kTextPx, kRowHeight });

            placement.segments[1].leftPx      = kStartPx;
            placement.segments[1].clipRightPx = kStartPx + (float) kTextPx;

            Assert::IsFalse (OperandResultTip::TryMakeLayout (placement, text, layout), L"text that fills the box exactly has no tip");

            placement.segments[1].clipRightPx -= 1.0f;

            Assert::IsTrue  (OperandResultTip::TryMakeLayout (placement, text, layout), L"text a pixel wider than the box has a tip");
        }


        TEST_METHOD (NothingToShowOrMeasureHasNoTip)
        {
            OperandResultTip::Placement  empty  = MakePlacement (GetEdgeFor (4));
            OperandResultTip::Placement  row    = MakeRow ("A=00 X=01 $1234=41", "A=41", GetEdgeFor (4));
            MockDxuiTextRenderer         text;
            DxuiNullTextRenderer         none;
            OperandResultTip::Layout     layout;



            Assert::IsFalse (OperandResultTip::TryMakeLayout (empty, text, layout), L"a row with no text");
            Assert::IsFalse (OperandResultTip::TryMakeLayout (row,   none, layout), L"text that cannot be measured");
        }


        //  Every line of the tip is filled with the row's highlight across
        //  the whole row: the row's line from its first redrawn text, the
        //  view's highlight showing left of it, and each line below from the
        //  row's start to the tip's right edge, which is the view's edge or
        //  past it.
        TEST_METHOD (EveryLineIsFilledAcrossTheWholeRow)
        {
            OperandResultTip::Placement     placement = MakeRow ("Z=0", "PC=$60BA; 3 cycles", GetEdgeFor (2));
            MockDxuiTextRenderer            text;
            MockDxuiPainter                 painter;
            OperandResultTip::Layout        layout;
            std::vector<RecordedPaintCall>  fills;



            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (2, layout.lineCount, L"the operand on the row's line, the result below it");
            Assert::AreEqual ((LONG) kRowLeft, layout.rect.left, L"the lines below start at the row's start");
            Assert::AreEqual (kTextLeft - kPadPx, layout.firstLeft, L"the row's line from the operand's padding");
            Assert::AreEqual (kFill, layout.fill, L"the row's highlight");

            OperandResultTip::Paint (layout, painter, text);
            fills = GetFills (painter);

            Assert::AreEqual ((size_t) 2, fills.size());
            Assert::AreEqual ((float) (kTextLeft - kPadPx - kRowLeft), fills[0].x, L"the row's line from its redrawn text");
            Assert::AreEqual ((float) kRowHeight,                       fills[0].height);
            Assert::AreEqual ((float) (layout.rect.right - kTextLeft + kPadPx), fills[0].width, L"to the tip's right edge");
            Assert::AreEqual (0.0f,                                     fills[1].x, L"the line below from the row's start");
            Assert::AreEqual ((float) kRowHeight,                       fills[1].y);
            Assert::AreEqual ((float) (layout.rect.right - layout.rect.left), fills[1].width, L"across the whole row to the tip's right edge");
            Assert::AreEqual (kFill,                                    fills[1].argb);
            Assert::IsFalse  (std::ranges::any_of (painter.Calls(), [] (const RecordedPaintCall & call) { return call.kind == RecordedPaintKind::OutlineRect; }),
                              L"the tip has no outline");

            //  Text that ends short of the view's edge fills to it too.
            placement = MakeRow ("A=00 X=01", "A=41", GetEdgeFor (20));

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual ((LONG) GetEdgeFor (20), layout.rect.right, L"the tip stops short of the view's edge");
        }


        //  A cut-off part of nothing but spaces doesn't add text to the tip, and
        //  the parts after it are laid out as usual.
        TEST_METHOD (APartOfOnlySpacesAddsNothing)
        {
            OperandResultTip::Placement  placement = MakePlacement (kCodeLeft + 2 * kGlyphPx);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (std::wstring (10, L' '), kCodeLeft, placement));
            placement.segments.push_back (MakeSegment (L"BNE w_inner", kCodeLeft + 11 * kGlyphPx, placement));

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (std::wstring (L"BNE w_inner"), GetLineText (layout, 0));
        }


        //  The row's line is its text as the view draws it, run on past
        //  the view's edge to the end of the part the view cuts off.
        TEST_METHOD (TheRowsOwnLineRunsOnPastTheViewsEdge)
        {
            OperandResultTip::Placement  placement = MakePlacement (kCodeLeft + 4 * kGlyphPx);
            MockDxuiTextRenderer         text;
            MockDxuiPainter              painter;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (L"0300", kRowLeft + kPadPx, placement));
            placement.segments.push_back (MakeSegment (L"BNE w_inner", kCodeLeft, placement));

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (1, layout.lineCount, L"one line");
            Assert::AreEqual (std::wstring (L"BNE w_inner"), GetLineText (layout, 0), L"from the part the view cuts off, the address left to the view");
            Assert::AreEqual ((float) kCodeLeft,             GetLine (layout, 0)[0].leftPx, L"where the view draws it");
            Assert::AreEqual ((LONG) (kCodeLeft + 11 * kGlyphPx + kPadPx), layout.rect.right, L"run on past the view to its end");
            Assert::AreEqual ((LONG) (kCodeLeft - kPadPx), layout.rect.left, L"a tip of the row's line alone starts at its redrawn text");

            OperandResultTip::Paint (layout, painter, text);

            Assert::AreEqual ((float) kCodeLeft, (float) layout.rect.left + text.Calls()[0].x, L"the text lands on the view's");
            Assert::AreEqual ((float) kRowTop,   (float) layout.rect.top  + text.Calls()[0].y);
            Assert::AreEqual ((float) kRowHeight, text.Calls()[0].height, L"a row high, as the row is");
        }


        //  A part drawn whole that the part ahead of it, run on, would run
        //  into, follows that part, a padding's width apart, as columns are.
        TEST_METHOD (APartRunIntoByTheOneAheadFollowsIt)
        {
            OperandResultTip::Placement         placement = MakePlacement (kScreenRight);
            MockDxuiTextRenderer                text;
            OperandResultTip::Layout            layout;
            std::vector<OperandResultTip::Run>  runs;



            placement.segments.push_back (MakeSegment (L"a_long_label", kCodeLeft, placement));
            placement.segments.push_back (MakeSegment (L"RTS", kTextLeft, placement));
            placement.segments[0].clipRightPx = (float) (kTextLeft - kPadPx);

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout), L"a column narrower than its label cuts it off");

            runs = GetLine (layout, 0);

            Assert::AreEqual ((size_t) 2, runs.size());
            Assert::AreEqual ((float) kCodeLeft, runs[0].leftPx);
            Assert::AreEqual ((float) (kCodeLeft + 12 * kGlyphPx + 2 * kPadPx), runs[1].leftPx, L"pushed along past the label");
        }


        //  A part pushed along to the screen's edge by a cut-off part ahead of
        //  it wraps there, as the last part does, and the part after it
        //  follows it. A part whose place is past the edge is cut there. None
        //  of this takes the tip down.
        TEST_METHOD (APartPushedToTheScreensEdgeWrapsThere)
        {
            constexpr int                       kEdgePx   = 210;
            constexpr int                       kAfterPx  = 180;

            OperandResultTip::Placement         placement = MakePlacement (kScreenRight);
            MockDxuiTextRenderer                text;
            OperandResultTip::Layout            layout;
            float                               pushedPx  = (float) (kCodeLeft + 12 * kGlyphPx + 2 * kPadPx);



            placement.segments.push_back (MakeSegment (L"a_long_label", kCodeLeft, placement));
            placement.segments.push_back (MakeSegment (L"LDA (PTR),Y", kTextLeft, placement));
            placement.segments.push_back (MakeSegment (L"; go", kAfterPx, placement));
            placement.segments[0].clipRightPx = (float) (kTextLeft - kPadPx);
            placement.workArea.right          = kEdgePx + kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout), L"a part pushed off the screen takes the tip down");
            Assert::AreEqual (3, layout.lineCount);
            Assert::AreEqual ((size_t) 2, GetLine (layout, 0).size());
            Assert::AreEqual (std::wstring (L"a_long_label"), GetLine (layout, 0)[0].text);
            Assert::AreEqual (std::wstring (L"LDA"),          GetLine (layout, 0)[1].text, L"broken at its space at the screen's edge");
            Assert::AreEqual (pushedPx,                       GetLine (layout, 0)[1].leftPx);
            Assert::AreEqual (std::wstring (L"(PTR),Y"),      GetLineText (layout, 1));
            Assert::AreEqual (pushedPx,                       GetLine (layout, 1)[0].leftPx, L"continued at its left");
            Assert::AreEqual (std::wstring (L"; go"),         GetLineText (layout, 2), L"the next part on the next line");
            Assert::AreEqual ((float) kAfterPx,               GetLine (layout, 2)[0].leftPx, L"at its left");
            Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"on the screen");

            //  A part that starts past the screen's edge is cut there.
            placement.segments[2].leftPx = (float) (kEdgePx + kPadPx);

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout), L"a part past the screen's edge takes the tip down");
            Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"on the screen");
            Assert::AreEqual (std::wstring (L"(PTR),Y"),      GetLine (layout, 1)[0].text);
        }


        //  A narrow pane cuts the branch off at its edge, and its operand and
        //  result lie out of view. The branch runs on along the row's line,
        //  and the operand, with its symbol, and the result are each on a
        //  line below, under the mnemonic.
        TEST_METHOD (AnOperandOutOfViewLiesBelowUnderTheCode)
        {
            OperandResultTip::Placement  placement = MakePlacement (kCodeLeft + 4 * kGlyphPx);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (L"BNE w_inner", kCodeLeft, placement));
            placement.segments.push_back (MakeOperand ("Z=0", "PC=$60BA; 3 cycles", placement));
            placement.operand = 1;
            placement.symbol  = L"w_inner = $60BA";

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (3, layout.lineCount);
            Assert::AreEqual (std::wstring (L"BNE w_inner"),                GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"Z=0 w_inner = $60BA"),        GetLineText (layout, 1), L"the symbol another of the operand's values");
            Assert::AreEqual (std::wstring (L"Result: PC=$60BA; 3 cycles"), GetLineText (layout, 2));
            Assert::AreEqual ((float) kCodeLeft, GetLine (layout, 1)[0].leftPx, L"the operand under the mnemonic");
            Assert::AreEqual ((float) kCodeLeft, GetLine (layout, 2)[0].leftPx, L"the result under the mnemonic");
            Assert::AreEqual (kOperand, GetLine (layout, 1)[0].colors.back(), L"the symbol in the operand's color");
            Assert::IsTrue   (layout.hasSymbol);

            //  With nothing ahead of it cut off, the tip starts below the row.
            placement.segments[0].text = L"BNE";

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::IsTrue   (GetLine (layout, 0).empty(), L"nothing on the row's line");
            Assert::AreEqual ((LONG) (kRowTop + kRowHeight), layout.rect.top, L"from the line below the row");
            Assert::AreEqual ((LONG) kRowLeft,                layout.rect.left);
        }


        //  An operand in view but cut off stays on the row's line where it
        //  is, and its result goes on the line below, under its start.
        TEST_METHOD (ACutOffOperandStaysOnItsLineAndItsResultGoesUnderIt)
        {
            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 $1234=41", "A=41", GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (2, layout.lineCount);
            Assert::AreEqual (std::wstring (L"A=00 X=01 $1234=41"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"Result: A=41"),       GetLineText (layout, 1));
            Assert::AreEqual ((float) kTextLeft, GetLine (layout, 0)[0].leftPx, L"the operand where the view draws it");
            Assert::AreEqual ((float) kTextLeft, GetLine (layout, 1)[0].leftPx, L"the result under the operand's start");
            Assert::AreEqual (kOperand, GetLine (layout, 0)[0].colors.front(), L"the operand's color");
            Assert::AreEqual (kResult,  GetLine (layout, 1)[0].colors.front(), L"the result's color");

            //  The symbol joins the operand on its line.
            placement.symbol = L"w_inner = $60BA";

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (std::wstring (L"A=00 X=01 $1234=41 w_inner = $60BA"), GetLineText (layout, 0));
        }


        //  Only the screen's right edge wraps a part: the operand at its
        //  spaces, and then the result from the start of a separate line.
        TEST_METHOD (ALongOperandWrapsAtItsSpacesBeforeTheResult)
        {
            constexpr int                kRoomChars = 24;

            std::string                  operand   = "A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0";
            OperandResultTip::Placement  placement = MakeRow (operand, "A=41", GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.workArea.right = GetEdgeFor (kRoomChars) + kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (3, layout.lineCount, L"two lines of operand, one of result");
            Assert::AreEqual (std::wstring (L"A=00 X=01 Y=02 $1234=41"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"C=1 Z=0 N=1 V=0"),         GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"Result: A=41"),            GetLineText (layout, 2));

            for (const OperandResultTip::Run & run : layout.runs)
            {
                Assert::IsTrue   ((int) run.text.size() <= kRoomChars, run.text.c_str());
                Assert::AreEqual ((float) kTextLeft, run.leftPx, L"every line of a part starts at the same left");
            }

            Assert::AreEqual (TextEncoding::NarrowToWide (operand), GetLineText (layout, 0) + L" " + GetLineText (layout, 1), L"a break drops part of the operand");
        }


        TEST_METHOD (AWordWiderThanALineBreaksWhereTheLineEnds)
        {
            constexpr int                kRoomChars = 24;

            OperandResultTip::Placement  placement = MakeRow ("Turn_on_the_80_column_display_now", "", GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.workArea.right = GetEdgeFor (kRoomChars) + kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (2, layout.lineCount);
            Assert::AreEqual (std::wstring (L"Turn_on_the_80_column_di"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"splay_now"),                GetLineText (layout, 1));
        }


        //  A view with room for three characters wraps nothing: the tip runs
        //  on past it, onto the screen.
        TEST_METHOD (ANarrowViewDoesNotWrapThePartsOfTheTip)
        {
            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0", "A=41", GetEdgeFor (3));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (2, layout.lineCount, L"the operand on one line, the result on the next");
            Assert::AreEqual (std::wstring (L"A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0"), GetLineText (layout, 0));
            Assert::AreEqual ((LONG) (GetEdgeFor (39) + kPadPx), layout.rect.right, L"the tip runs on past the view to the end of the operand");
        }


        //  A source row's operand cell with the line's comment after its
        //  result: the comment takes the row's line, in the operand's
        //  place, as part of the source line, and the operand and then the
        //  result go below, under the code.
        TEST_METHOD (ASourceRowsCommentTakesItsLineAndItsOperandGoesBelow)
        {
            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;
            std::wstring                 cell      = L"Z=0  Result: PC=$60BA  ; spin until the counter wraps";



            placement.segments.push_back (MakeSegment (L"bne w_inner", kCodeLeft, placement));
            placement.segments.push_back (MakeSegment (cell, kTextLeft, placement, kOperand));
            placement.segments[1].resultAt  = (int) cell.find (SourcePane::kpszResultPrefix);
            placement.segments[1].commentAt = (int) cell.find (L';');
            placement.operand               = 1;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (3, layout.lineCount);
            Assert::AreEqual (std::wstring (L"; spin until the counter wraps"), GetLineText (layout, 0), L"the comment on the row's line");
            Assert::AreEqual ((float) kTextLeft,                       GetLine (layout, 0)[0].leftPx, L"in the operand's place");
            Assert::AreEqual (std::wstring (L"Z=0"),                   GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"Result: PC=$60BA"),      GetLineText (layout, 2));
            Assert::AreEqual ((float) kCodeLeft, GetLine (layout, 1)[0].leftPx, L"the operand under the code");
            Assert::AreEqual ((float) kCodeLeft, GetLine (layout, 2)[0].leftPx, L"the result under the code");

            //  Wider than the screen's room, the comment wraps under its
            //  first character, and the operand and result follow it.
            placement.workArea.right = GetEdgeFor (20) + kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (std::wstring (L"; spin until the"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"counter wraps"),    GetLineText (layout, 1));
            Assert::AreEqual ((float) (kTextLeft + 2 * kGlyphPx), GetLine (layout, 1)[0].leftPx, L"under the comment's text");
            Assert::AreEqual (std::wstring (L"Z=0"),              GetLineText (layout, 2));
        }


        //  A source or data line the view cuts off has the tip too, a comment
        //  and all: one line from where the view draws it, run on past the
        //  view.
        TEST_METHOD (ACutOffSourceLineHasATip)
        {
            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (20));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;
            std::wstring                 line      = L".byte \"WATCH THE BAR FILL\",0 ; on row 2";



            placement.segments.push_back (MakeSegment (L"hint_text", kRowLeft + kPadPx, placement));
            placement.segments.push_back (MakeSegment (line, kTextLeft, placement));
            placement.segments[1].isWholeLine = true;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (1, layout.lineCount, L"the whole line on one line");
            Assert::AreEqual (line, GetLineText (layout, 0), L"the line the view cuts off; its label is the view's");
            Assert::AreEqual ((LONG) (GetEdgeFor ((int) line.size()) + kPadPx), layout.rect.right, L"run on to the end of the line");
        }


        //  A source line wider than the screen's room breaks in its comment,
        //  and every line after the first starts under the comment's first
        //  character, past the ; and its space.
        TEST_METHOD (ALongCommentWrapsUnderTheCommentsText)
        {
            constexpr int                kRoomChars   = 40;
            constexpr int                kCommentChar = 11;

            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (20));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (L"lda #$41 ; set the accumulator to the letter A before the loop starts", kTextLeft, placement));
            placement.segments[0].isWholeLine = true;
            placement.workArea.right          = GetEdgeFor (kRoomChars) + kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (3, layout.lineCount);
            Assert::AreEqual (std::wstring (L"lda #$41 ; set the accumulator to the"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"letter A before the loop"),              GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"starts"),                                GetLineText (layout, 2));
            Assert::AreEqual ((float) (kTextLeft + kCommentChar * kGlyphPx), GetLine (layout, 1)[0].leftPx, L"under the comment's first character");
            Assert::AreEqual ((float) (kTextLeft + kCommentChar * kGlyphPx), GetLine (layout, 2)[0].leftPx);
            Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"on the screen");
        }


        //  A line's comment starts at its first ; outside a string or a
        //  character, with or without a space before it. A quote with no
        //  closing quote after it doesn't start a string.
        TEST_METHOD (TheCommentFinderSkipsStringsAndCharacters)
        {
            Assert::AreEqual ((size_t) 8,  SourceSyntax::FindComment (L"lda #$41;no space", 0));
            Assert::AreEqual ((size_t) 9,  SourceSyntax::FindComment (L"lda #$41 ; one space", 0));
            Assert::AreEqual ((size_t) 9,  SourceSyntax::FindComment (L"cmp #';' ; a semicolon", 0), L"a ; in a character literal");
            Assert::AreEqual ((size_t) 12, SourceSyntax::FindComment (L"asc \"a;b\"   ; text", 0), L"a ; in a string");
            Assert::AreEqual ((size_t) 8,  SourceSyntax::FindComment (L"cmp #'a ; a letter", 0), L"an unclosed quote");
            Assert::AreEqual ((size_t) 7,  SourceSyntax::FindComment (L"asc \"a\"", 0), L"no comment");
            Assert::AreEqual ((size_t) 9,  SourceSyntax::FindComment (L"; label  ; rest", 1), L"from where the search starts");
        }


        //  A source line with no comment, wider than the screen's room,
        //  continues four characters right of where its code starts. A ;
        //  inside a string is not a comment.
        TEST_METHOD (ALongLineWithNoCommentWrapsFourCharactersRightOfItsCode)
        {
            constexpr int                kRoomChars = 30;
            constexpr int                kCodeChar  = 2;

            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (20));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (L"  dc.b \"ONE;TWO THREE FOUR FIVE SIX SEVEN EIGHT NINE TEN\",0", kTextLeft, placement));
            placement.segments[0].isWholeLine = true;
            placement.workArea.right          = GetEdgeFor (kRoomChars) + kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::IsTrue   (layout.lineCount > 1, L"too wide for the screen's room");
            Assert::AreEqual (std::wstring (L"  dc.b \"ONE;TWO THREE FOUR"), GetLineText (layout, 0), L"broken at a space");

            for (int line = 1; line < layout.lineCount; line++)
            {
                Assert::AreEqual ((float) (kTextLeft + (kCodeChar + OperandResultTip::kCodeIndentChars) * kGlyphPx), GetLine (layout, line)[0].leftPx);
            }

            Assert::IsTrue (layout.rect.right <= placement.workArea.right, L"on the screen");
        }


        //  An operand scrolled out of sight to the left lies below the row
        //  too, from the view's text where the code is out of sight with it.
        TEST_METHOD (AnOperandScrolledLeftOfTheViewLiesBelowTheRow)
        {
            constexpr int                kViewLeft = 300;

            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01", "A=41", GetEdgeFor (30) + kViewLeft);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.visibleLeft = kViewLeft;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::IsTrue   (GetLine (layout, 0).empty());
            Assert::AreEqual (std::wstring (L"A=00 X=01"),    GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"Result: A=41"), GetLineText (layout, 2));
            Assert::AreEqual ((float) (kViewLeft + kPadPx), GetLine (layout, 1)[0].leftPx, L"from the view's text");
        }


        TEST_METHOD (AtTheScreensRightEdgeTheTipGrowsDown)
        {
            constexpr int                kRoomChars = 12;

            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 Y=02 $1234=41 C=1 Z=0", "A=41", GetEdgeFor (30));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  The view runs past the screen's edge.
            placement.workArea.right = GetEdgeFor (kRoomChars) + kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"the tip runs off the screen, where its popup would be moved off the row");
            Assert::IsTrue   (layout.lineCount > 3, L"the tip grows down instead");

            for (const OperandResultTip::Run & run : layout.runs)
            {
                Assert::IsTrue   ((int) run.text.size() <= kRoomChars, run.text.c_str());
                Assert::AreEqual ((float) kTextLeft, run.leftPx, L"the text starts where the operand's does");
            }
        }


        TEST_METHOD (ATipTallerThanTheScreenBelowItsRowHasNone)
        {
            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0", "A=41", GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  The operand on one line and the result on a second; the screen
            //  has room below the row for two.
            placement.workArea.bottom = kRowTop + 2 * kRowHeight;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (2, layout.lineCount);
            Assert::IsTrue   (layout.rect.bottom <= placement.workArea.bottom, L"the tip runs off the bottom of the screen");

            //  With room for a single row, nothing fits.
            placement.workArea.bottom = kRowTop + kRowHeight;

            Assert::IsFalse (OperandResultTip::TryMakeLayout (placement, text, layout), L"a tip too tall for the screen");
        }


        //  The tip is laid out on the monitor the row starts on: a row whose
        //  start is off that screen doesn't have a tip.
        TEST_METHOD (ARowStartingOffTheScreenHasNoTip)
        {
            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 Y=02 $1234=41", "A=41", GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.workArea.left = kRowLeft;

            Assert::IsTrue  (OperandResultTip::TryMakeLayout (placement, text, layout), L"a row starting at the screen's edge");

            placement.workArea.left = kRowLeft + 1;

            Assert::IsFalse (OperandResultTip::TryMakeLayout (placement, text, layout), L"a row starting off the screen");
        }


        TEST_METHOD (TheTipsSizeSurvivesItsPopupsTripThroughDips)
        {
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;



            for (UINT dpi : { 120u, 144u, 168u })
            {
                OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 Y=02 $1234=41 C=1 Z=0", "A=41", GetEdgeFor (30));
                int                          width     = 0;
                int                          height    = 0;

                placement.dpi = dpi;

                Assert::IsTrue (OperandResultTip::TryMakeLayout (placement, text, layout));

                width  = layout.rect.right  - layout.rect.left;
                height = layout.rect.bottom - layout.rect.top;

                Assert::AreEqual (width,  OperandResultTip::GetPopupPx (width,  dpi), L"the popup comes up wider than the tip");
                Assert::AreEqual (height, OperandResultTip::GetPopupPx (height, dpi), L"the popup comes up taller than the tip");
                Assert::IsTrue   (layout.rect.right >= GetEdgeFor (30), L"the tip ends short of the view's edge");

                //  Against the screen's edge it cannot grow, so it shrinks.
                placement.workArea.right = GetEdgeFor (31) + kPadPx;

                Assert::IsTrue (OperandResultTip::TryMakeLayout (placement, text, layout));

                width = layout.rect.right - layout.rect.left;

                Assert::AreEqual (width, OperandResultTip::GetPopupPx (width, dpi));
                Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"the tip runs off the screen");
            }
        }


        //  Each run in its part's colors, made to read on the fill, a row
        //  below the one before.
        TEST_METHOD (TheTipDrawsInTheRowsColorsOnItsFill)
        {
            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 $1234=41", "A=41", GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            MockDxuiPainter              painter;
            OperandResultTip::Layout     layout;



            placement.fill = 0xFF264F78;

            Assert::IsTrue (OperandResultTip::TryMakeLayout (placement, text, layout));
            OperandResultTip::Paint (layout, painter, text);

            Assert::AreEqual ((size_t) 2, text.Calls().size(), L"one run a line, each in one color");
            Assert::AreEqual (DxuiColor::ComputeInkForContrast (kOperand, placement.fill, DebuggerTextColors::s_kMinTextContrast), text.Calls()[0].argb,
                              L"the operand's color, reading on the fill");
            Assert::AreEqual (DxuiColor::ComputeInkForContrast (kResult,  placement.fill, DebuggerTextColors::s_kMinTextContrast), text.Calls()[1].argb);
            Assert::AreEqual ((float) kRowHeight, text.Calls()[1].y - text.Calls()[0].y, L"the result a row below");
            Assert::IsTrue   (text.Calls()[0].fontSizeDip == kFontPx, L"the row's size");
        }


        //  A row the view draws in italics, a data line's, draws its tip in
        //  italics too, in the same face and size.
        TEST_METHOD (AnItalicRowDrawsAnItalicTip)
        {
            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            MockDxuiPainter              painter;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (L"ASC \"A LONG STRING OF DATA\"", kTextLeft, placement));
            placement.segments[0].isItalic    = true;
            placement.segments[0].isWholeLine = true;

            Assert::IsTrue (OperandResultTip::TryMakeLayout (placement, text, layout));
            OperandResultTip::Paint (layout, painter, text);

            Assert::IsFalse (text.Calls().empty());
            Assert::IsTrue  (std::ranges::all_of (text.Calls(), [] (const RecordedTextCall & call) { return call.weight == DxuiFontWeight::Italic; }),
                             L"the tip's text is upright");

            placement.segments[0].isItalic = false;

            Assert::IsTrue (OperandResultTip::TryMakeLayout (placement, text, layout));
            text.Reset();
            OperandResultTip::Paint (layout, painter, text);

            Assert::IsTrue  (std::ranges::all_of (text.Calls(), [] (const RecordedTextCall & call) { return call.weight == DxuiFontWeight::Normal; }));
        }


        //  The debugger window, with a code pane whose first row's operand
        //  the pane cuts off and whose second row's fits.
        static void  Build (TipWindow & window, const CassoTheme & theme, MockDxuiTextRenderer & text)
        {
            DxuiDpiScaler    scaler;
            MockDxuiPainter  painter;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreen());

            //  A paint fits the columns to their text, which places the cells.
            window.GetCodeList (0)->Paint (painter, text, theme);
        }


        //  A point on a row's operand, just past where its text starts.
        static POINT  GetOperandPoint (TipWindow & window, int row)
        {
            DxuiListView  * list   = window.GetCodeList (0);
            RECT            bounds = list->GetBounds();
            RECT            cell   = {};



            Assert::IsTrue (list->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell), L"the row's operand is in view");

            return POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 };
        }


        //  A point on another row's operand that fits and that the tip up now
        //  doesn't cover, or else just below the tip and the rows: a tip that
        //  lies below its row is the pointer's to move into.
        static POINT  GetPointOffTip (TipWindow & window)
        {
            DxuiListView  * list   = window.GetCodeList (0);
            RECT            tip    = window.GetOperandTips().GetLayout().rect;
            POINT           first  = GetOperandPoint (window, 0);
            LONG            below  = tip.bottom;
            int             row    = 0;
            RECT            cell   = {};



            for (row = 1; list->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell); row++)
            {
                POINT  at     = GetOperandPoint (window, row);
                bool   isFull = (int) list->GetCellsOfRow (row)[DisassemblyCells::kOperandColumn].text.size() * kGlyphPx > cell.right - cell.left;



                below = (std::max) (below, list->GetBounds().top + cell.bottom);

                if (PtInRect (&tip, at) == FALSE && !isFull)
                {
                    return at;
                }
            }

            return POINT { first.x, below + 1 };
        }


        static void  Send (TipWindow & window, DxuiMouseEventKind kind, POINT at)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = (kind == DxuiMouseEventKind::Move) ? DxuiMouseButton::None : DxuiMouseButton::Left;
            ev.positionDip = at;
            ev.wheelDelta  = (kind == DxuiMouseEventKind::Wheel) ? -1.0f : 0.0f;

            (void) window.OnMouse (ev);
        }


        //  A row's operand, where the list draws its text, in the list's
        //  pixels.
        static RECT  GetCellRect (TipWindow & window, int row)
        {
            RECT  cell = {};



            Assert::IsTrue (window.GetCodeList (0)->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell), L"the row's operand is in view");

            return cell;
        }


        //  A drag along a row's operand from just past where its text starts,
        //  which selects the characters it passes over.
        static void  DragAlong (TipWindow & window, int row)
        {
            DxuiMouseEvent  ev;



            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = GetOperandPoint (window, row);
            (void) window.OnMouse (ev);

            ev.kind           = DxuiMouseEventKind::Move;
            ev.positionDip.x += kDragPx;
            (void) window.OnMouse (ev);

            ev.kind = DxuiMouseEventKind::Up;
            (void) window.OnMouse (ev);
        }


        //  The screen the window's tips are laid out on: one monitor, and
        //  nothing lying over the window.
        static TipWindow::OperandTipScreen  MakeScreen()
        {
            TipWindow::OperandTipScreen  screen;



            screen.workAreas.push_back (kScreen);
            return screen;
        }


        static bool  TryGetTip (TipWindow & window, POINT at, MockDxuiTextRenderer & text, OperandResultTip::Layout & layout)
        {
            DxuiDpiScaler             scaler;
            TipWindow::OperandTipCell cell;



            scaler.SetDpi (96);

            return window.GetOperandTips().TryGet (at, scaler, text, MakeScreen(), layout, cell);
        }


        TEST_METHOD (TheWindowLaysTheTipOverACutOffCodeCell)
        {
            CassoTheme                 theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                    host;
            TipWindow                  window (theme, host);
            DxuiDpiScaler              scaler;
            MockDxuiTextRenderer       text;
            DxuiListView             * list    = nullptr;
            RECT                       bounds  = {};
            RECT                       cell    = {};
            OperandResultTip::Layout   layout;
            TipWindow::OperandTipCell  tipCell;



            scaler.SetDpi (96);
            Build (window, theme, text);

            list   = window.GetCodeList (0);
            bounds = list->GetBounds();

            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kOperandColumn, cell), L"the long row's operand is in view");
            Assert::IsTrue (window.GetOperandTips().TryGet (GetOperandPoint (window, 0), scaler, text, MakeScreen(), layout, tipCell),
                            L"the pane cuts the long operand off");

            Assert::AreEqual (0, tipCell.view);
            Assert::AreEqual (0, tipCell.row);
            Assert::AreEqual (2, layout.lineCount, L"the operand on the row's line and the result on the next");
            Assert::AreEqual ((float) (bounds.left + cell.left), GetLine (layout, 0)[0].leftPx, L"the tip's text starts at the cell's");
            Assert::AreEqual ((float) (bounds.left + cell.left), GetLine (layout, 1)[0].leftPx, L"the result from the operand's left");
            Assert::AreEqual (bounds.top + cell.top, (LONG) layout.rowTop);
            Assert::IsTrue   (layout.rect.right > bounds.right, L"the tip runs on past the pane to the end of the operand");
            Assert::AreEqual (GetRowStart (*list, 0), layout.rect.left, L"from the row's address, as the list highlights it");
            Assert::AreEqual ((int) (bounds.left + cell.left - scaler.ToPx (list->GetCellPadLeftDip())), layout.firstLeft,
                              L"the row's line from the cell's padding");

            Assert::IsFalse (window.GetOperandTips().TryGet (GetOperandPoint (window, 1), scaler, text, MakeScreen(), layout, tipCell),
                             L"a short operand fits");
        }


        //  The tip is up exactly when the list's drawing of the operand is
        //  clipped: to the box its paint draws the text in, or the list's
        //  clip short of its scrollbar, whichever comes first.
        TEST_METHOD (TheWindowsTipIsUpExactlyWhenTheListClipsTheOperand)
        {
            CassoTheme                theme     = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            MockDxuiPainter           painter;
            DxuiListView            * list      = nullptr;
            RECT                      bounds    = {};
            RECT                      cell      = {};
            float                     textLeft  = 0.0f;
            float                     boxRight  = 0.0f;
            float                     clipRight = 0.0f;
            bool                      hasClip   = false;
            bool                      hasRun    = false;
            int                       roomPx    = 0;
            std::wstring              operand;
            OperandResultTip::Layout  layout;



            Build (window, theme, text);

            list    = window.GetCodeList (0);
            bounds  = list->GetBounds();
            operand = list->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text;

            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kOperandColumn, cell), L"the long row's operand is in view");

            textLeft = (float) (bounds.left + cell.left);

            text.Reset();
            list->Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::PushClipRect && !hasClip)
                {
                    clipRight = call.x + call.width;
                    hasClip   = true;
                }

                if (call.kind == RecordedTextKind::DrawString && !hasRun && call.x == textLeft && !call.text.empty() && operand.starts_with (call.text))
                {
                    boxRight = call.x + call.width;
                    hasRun   = true;
                }
            }

            Assert::IsTrue (hasClip, L"the list clips its paint");
            Assert::IsTrue (hasRun,  L"the list draws the operand where its text starts");

            roomPx = (int) ((std::min) (boxRight, clipRight) - textLeft);

            text.SetCannedMetrics (operand, SIZE { roomPx, kRowHeight });
            Assert::IsFalse (TryGetTip (window, GetOperandPoint (window, 0), text, layout), L"an operand that fills its box exactly has no tip");

            text.SetCannedMetrics (operand, SIZE { roomPx + 1, kRowHeight });
            Assert::IsTrue  (TryGetTip (window, GetOperandPoint (window, 0), text, layout), L"an operand a pixel wider than its box has a tip");
        }


        //  With the columns ahead of it widened past the pane's right edge,
        //  the operand and result are wholly out of view. With the pointer
        //  anywhere on the row, the tip is up with the operand on the row's
        //  line, two spaces after the code rather than at its column past the
        //  pane, and the list draws the row in the tip's color from its
        //  address.
        TEST_METHOD (AnOperandPastThePanesEdgeRunsOnAfterTheCode)
        {
            CassoTheme                 theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                    host;
            TipWindow                  window (theme, host);
            MockDxuiTextRenderer       text;
            MockDxuiPainter            painter;
            DxuiListView             * list    = nullptr;
            RECT                       bounds  = {};
            RECT                       cell    = {};
            RECT                       address = {};
            RECT                       code    = {};
            int                        right   = 0;
            POINT                      at      = {};



            Build (window, theme, text);

            list = window.GetCodeList (0);
            list->SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, kWindowWidth * 2);
            list->Paint (painter, text, theme);

            bounds = list->GetBounds();
            right  = bounds.right - (list->IsScrollbarVisible() ? list->GetScrollbarWidthPx() : 0);

            Assert::IsTrue (list->GetCellTextRectPx (kLastRow, DisassemblyCells::kOperandColumn, cell));
            Assert::IsTrue (bounds.left + cell.left >= right, L"the operand starts past the pane's right edge");
            Assert::IsTrue (list->GetCellTextRectPx (kLastRow, DisassemblyCells::kAddressColumn, address));

            at = POINT { bounds.left + (address.left + address.right) / 2, bounds.top + (address.top + address.bottom) / 2 };
            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"no tip after a move onto the row's address");
            Assert::AreEqual (kLastRow, window.GetOperandTips().GetCell().row);
            Assert::AreEqual (GetRowStart (*list, kLastRow), window.GetOperandTips().GetCell().area.left, L"the row in view from its address is the tip's");
            Assert::AreEqual ((LONG) right, window.GetOperandTips().GetCell().area.right);
            Assert::AreEqual (bounds.top + cell.top, window.GetOperandTips().GetLayout().rect.top, L"from the row's line");
            Assert::AreEqual (1, window.GetOperandTips().GetLayout().lineCount, L"the operand, which has no result, on the row's line");
            Assert::AreEqual (kLastRow, list->GetHighlightRow(), L"the list draws the row in the tip's color");

            Assert::IsTrue   (list->GetCellTextRectPx (kLastRow, DisassemblyCells::kInstructionColumn, code));
            Assert::IsTrue   (GetLine (window.GetOperandTips().GetLayout(), 0)[0].leftPx > (float) (bounds.left + code.left), L"after the code");
            Assert::IsTrue   (GetLine (window.GetOperandTips().GetLayout(), 0)[0].leftPx < (float) right, L"not at its column past the pane");


            Send (window, DxuiMouseEventKind::Move, POINT { bounds.left + 1, bounds.bottom + kGapPx });
            Assert::IsFalse  (window.GetOperandTips().HasTip(), L"the tip is up with the pointer off the pane");
        }


        //  A row whose text at the right edge runs on from a cell ahead of
        //  the operand, as a data line's string does, has the tip over that
        //  cell, on one line where the list draws it, run on past the pane,
        //  in the italics the list draws a data line in. The label is the
        //  list's. The pointer on the label puts it up too.
        TEST_METHOD (TheWindowLaysTheTipOverACutOffCellThatSpansTheRow)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                         host;
            TipWindow                       window   (theme, host);
            DxuiDpiScaler                   scaler;
            MockDxuiTextRenderer            text;
            MockDxuiPainter                 painter;
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  data;
            DxuiListView                  * list     = nullptr;
            RECT                            bounds   = {};
            RECT                            cell     = {};
            RECT                            label    = {};
            OperandResultTip::Layout        layout;
            TipWindow::OperandTipCell       tipCell;



            data.address     = 0x0300;
            data.bytes       = "48 49";
            data.label       = "msg";
            data.instruction = "ASC \"" + std::string (300, 'X') + "\"";
            data.dataOrigin  = DataOrigin::Marked;

            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { data };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreen());

            list = window.GetCodeList (0);
            list->Paint (painter, text, theme);
            bounds = list->GetBounds();

            Assert::IsTrue   (list->GetCellTextRectPx (0, DisassemblyCells::kInstructionColumn, cell), L"the data line's text is in view");
            Assert::IsTrue   (window.GetOperandTips().TryGet (POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 },
                                                              scaler, text, MakeScreen(), layout, tipCell),
                              L"the pane cuts the string off");
            Assert::AreEqual (1, layout.lineCount, L"the whole line on one line");
            Assert::AreEqual (list->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].text, GetLine (layout, 0)[0].text, L"the string");
            Assert::AreEqual ((float) (bounds.left + cell.left), GetLine (layout, 0)[0].leftPx, L"the tip's text starts at the cell's");
            Assert::IsTrue   (GetLine (layout, 0)[0].isItalic, L"a data line's tip is upright");
            Assert::IsTrue   (layout.rect.right > bounds.right, L"run on past the pane");

            Assert::IsTrue   (list->IsColumnVisible (DisassemblyCells::kLabelColumn));
            Assert::IsTrue   (list->GetCellTextRectPx (0, DisassemblyCells::kLabelColumn, label), L"the label is in view");

            Assert::IsTrue   (window.GetOperandTips().TryGet (POINT { bounds.left + label.left + 1, bounds.top + (label.top + label.bottom) / 2 },
                                                              scaler, text, MakeScreen(), layout, tipCell),
                              L"the pointer on the label puts the line's tip up");
        }


        //  A branch written with a symbol: with its operand and result out of
        //  view, the tip has the symbol's name and value among the operand's
        //  values, and the symbol tip doesn't come up on the row while the
        //  in-place tip is up. With the row whole in view, the symbol tip
        //  comes up.
        TEST_METHOD (AnOperandsSymbolJoinsTheTipInPlaceOfTheSymbolTip)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                         host;
            TipWindow                       window   (theme, host);
            DxuiDpiScaler                   scaler;
            MockDxuiTextRenderer            text;
            MockDxuiPainter                 painter;
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  bne;
            DxuiListView                  * list     = nullptr;
            RECT                            bounds   = {};
            RECT                            code     = {};
            POINT                           at       = {};
            POINT                           local    = {};
            DxuiTip                         tip;



            bne.address               = 0x6100;
            bne.bytes                 = "D0 F8";
            bne.instruction           = "BNE w_inner";
            bne.shownOperand          = "w_inner";
            bne.memoryOperand         = "$60BA";
            bne.annotationParts.flags = "Z=0";
            bne.effect                = "PC=$60BA; 3 cycles";
            bne.isCurrent             = true;

            snapshot->pc           = 0x6100;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { bne };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreen());

            list = window.GetCodeList (0);
            Assert::IsTrue (list->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text.starts_with (L"w_inner = $60BA  Z=0"), L"the operand column has the symbol");

            list->Paint (painter, text, theme);

            bounds = list->GetBounds();
            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kInstructionColumn, code));
            at    = POINT { bounds.left + code.left + 1, bounds.top + (code.top + code.bottom) / 2 };
            local = POINT { at.x - bounds.left, at.y - bounds.top };

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the row fits");
            Assert::IsTrue  (list->TryGetTip (local, tip), L"the symbol has no tip");
            Assert::IsTrue  (tip.text.starts_with (L"w_inner = $60BA"), tip.text.c_str());

            list->SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, kWindowWidth * 2);
            list->Paint (painter, text, theme);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"the operand out of view has no tip");
            Assert::AreEqual (std::wstring (L"w_inner = $60BA  Z=0"), GetLine (window.GetOperandTips().GetLayout(), 0)[0].text,
                              L"the symbol first among the operand's values, as in its column");
            Assert::AreEqual (std::wstring (L"Result: PC=$60BA; 3 cycles"), GetLine (window.GetOperandTips().GetLayout(), 1)[0].text);

            tip = DxuiTip();
            Assert::IsFalse (list->TryGetTip (local, tip) && tip.text.find (L"w_inner = ") != std::wstring::npos, L"the symbol tip over the in-place tip");
        }


        //  While the in-place tip has the operand's symbol, the label's symbol
        //  tip on the same row comes up as usual.
        TEST_METHOD (ALabelsSymbolTipComesUpWhileTheOperandsIsInTheTip)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                         host;
            TipWindow                       window   (theme, host);
            DxuiDpiScaler                   scaler;
            MockDxuiTextRenderer            text;
            MockDxuiPainter                 painter;
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  bne;
            DxuiListView                  * list     = nullptr;
            RECT                            bounds   = {};
            RECT                            code     = {};
            RECT                            label    = {};
            POINT                           at       = {};
            DxuiTip                         tip;



            bne.address               = 0x6100;
            bne.bytes                 = "D0 F8";
            bne.label                 = "w_outer";
            bne.instruction           = "BNE w_inner";
            bne.shownOperand          = "w_inner";
            bne.memoryOperand         = "$60BA";
            bne.annotationParts.flags = "Z=0";
            bne.effect                = "PC=$60BA; 3 cycles";
            bne.isCurrent             = true;

            snapshot->pc           = 0x6100;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { bne };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreen());

            list = window.GetCodeList (0);
            list->SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, kWindowWidth * 2);
            list->Paint (painter, text, theme);

            bounds = list->GetBounds();
            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kInstructionColumn, code));
            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kLabelColumn, label), L"the label is in view");

            at = POINT { bounds.left + code.left + 1, bounds.top + (code.top + code.bottom) / 2 };
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the operand out of view has no tip");
            Assert::IsTrue (window.GetOperandTips().GetLayout().hasSymbol);

            Assert::IsTrue (list->TryGetTip (POINT { label.left + 1, (label.top + label.bottom) / 2 }, tip) && tip.text.starts_with (L"w_outer = $6100"),
                            L"the label's symbol tip doesn't come up");

            tip = DxuiTip();
            Assert::IsFalse (list->TryGetTip (POINT { at.x - bounds.left, at.y - bounds.top }, tip) && tip.text.find (L"w_inner = ") != std::wstring::npos,
                             L"the operand's symbol tip over the in-place tip");
        }


        //  A disassembly with LDA $10 at the PC, the second line of main.a65
        //  above it as a source row, LDA's operand and result on that row.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeSourceSnapshot (const std::string & registers, const std::string & effect)
        {
            auto                               snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine     lda;
            DebuggerViewSnapshot::SourceState  source;



            lda.address                   = 0x0300;
            lda.bytes                     = "A5 10";
            lda.instruction               = "LDA $10";
            lda.annotationParts.registers = registers;
            lda.effect                    = effect;
            lda.isCurrent                 = true;
            lda.sourceFileId              = 0;
            lda.sourceLine                = 2;

            source.debugFilePath = L"C:\\Work\\main.dbg";
            source.programKey    = "key";
            source.files         = { { 0, "main.a65", 45, 0, "", 0 } };

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];
            snapshot->source       = source;

            return snapshot;
        }


        //  The window over a source snapshot, with the host's options and
        //  text, its tips laid out on `screen` and its list painted.
        static void  BuildSource (TipWindow & window, const CassoTheme & theme, MockDxuiTextRenderer & text,
                                  const std::shared_ptr<DebuggerViewSnapshot> & snapshot, const TipWindow::OperandTipScreen & screen)
        {
            DxuiDpiScaler    scaler;
            MockDxuiPainter  painter;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetOperandTips().SetDeviceForTest (&text, screen);
            window.GetCodeList (0)->Paint (painter, text, theme);
        }


        //  A source row drawn as one cell, with the label column hidden or
        //  the instruction column dragged too narrow for its code: the row's
        //  line is the source line, its comment and all, run on past the
        //  pane, and the operand and then the result go below it, under where
        //  the code starts.
        TEST_METHOD (ASourceRowInOneCellHasItsOperandBelowUnderItsCode)
        {
            const std::wstring  line = L"loop    lda $10 ; " + std::wstring (300, L'c');

            for (bool isLabelShown : { false, true })
            {
                CassoTheme                 theme    = CassoTheme::MakeSkeuomorphic();
                SourceTipHost              host;
                TipWindow                  window   (theme, host);
                MockDxuiTextRenderer       text;
                MockDxuiPainter            painter;
                auto                       snapshot = MakeSourceSnapshot ("A=00 X=01", "A=41");
                size_t                     column   = isLabelShown ? DisassemblyCells::kInstructionColumn : DisassemblyCells::kAddressColumn;
                DxuiListView             * list     = nullptr;
                RECT                       bounds   = {};
                RECT                       cell     = {};
                float                      codeLeft = 0.0f;
                OperandResultTip::Layout   layout;

                host.options    = isLabelShown ? "address bytes source symbols" : "address bytes source";
                host.sourceText = "; main\n" + TextEncoding::WideToNarrow (line) + "\n";

                BuildSource (window, theme, text, snapshot, MakeScreen());
                list = window.GetCodeList (0);

                if (isLabelShown)
                {
                    list->SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, list->GetFittedWidthPx (4));
                    window.ApplyCodeSnapshot (std::make_shared<DebuggerViewSnapshot> (*snapshot), 0);
                    list->Paint (painter, text, theme);
                }

                bounds = list->GetBounds();

                Assert::IsTrue (list->GetCellsOfRow (0)[column].spansRow, L"the line in one cell");
                Assert::IsTrue (list->GetCellTextRectPx (0, column, cell));

                codeLeft = (float) (bounds.left + cell.left + (isLabelShown ? 0 : 8 * kGlyphPx));

                Assert::IsTrue   (TryGetTip (window, POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 }, text, layout),
                                  L"the pane cuts the line off");
                Assert::AreEqual (3, layout.lineCount);
                Assert::AreEqual (isLabelShown ? line.substr (8) : line, GetLineText (layout, 0), L"the source line, its comment and all");
                Assert::AreEqual (std::wstring (L"A=00 X=01"),    GetLineText (layout, 1));
                Assert::AreEqual (std::wstring (L"Result: A=41"), GetLineText (layout, 2));
                Assert::AreEqual (codeLeft, GetLine (layout, 1)[0].leftPx, L"the operand under the code");
                Assert::AreEqual (codeLeft, GetLine (layout, 2)[0].leftPx, L"the result under the code");
            }
        }


        //  A source row's comment after an operand cell with no values, so
        //  with no space before its ;, is the line's comment: it wraps at the
        //  screen's edge under the comment's text.
        TEST_METHOD (ASourceRowsCommentWithNoSpaceBeforeItIsFound)
        {
            constexpr int                  kRoomChars = 40;

            CassoTheme                     theme      = CassoTheme::MakeSkeuomorphic();
            SourceTipHost                  host;
            TipWindow                      window     (theme, host);
            MockDxuiTextRenderer           text;
            auto                           snapshot   = MakeSourceSnapshot ("", "");
            std::wstring                   comment    = L";";
            DxuiListView                 * list       = nullptr;
            RECT                           bounds     = {};
            RECT                           cell       = {};
            DxuiDpiScaler                  scaler;
            OperandResultTip::Layout       layout;
            TipWindow::OperandTipCell      tipCell;
            TipWindow::OperandTipScreen    screen;



            for (int i = 0; i < 60; i++)
            {
                comment += L" word";
            }

            host.options    = "address bytes source symbols";
            host.sourceText = "; main\n        clc " + TextEncoding::WideToNarrow (comment) + "\n";

            BuildSource (window, theme, text, snapshot, MakeScreen());
            list   = window.GetCodeList (0);
            bounds = list->GetBounds();

            Assert::AreEqual (comment, list->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text, L"the comment alone in the operand's cell");
            Assert::IsTrue   (list->GetCellTextRectPx (0, DisassemblyCells::kOperandColumn, cell));

            screen.workAreas.push_back (RECT { 0, 0, bounds.left + cell.left + kRoomChars * kGlyphPx + kPadPx, kScreenBelow });
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            scaler.SetDpi (96);

            Assert::IsTrue   (window.GetOperandTips().TryGet (POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 },
                                                              scaler, text, screen, layout, tipCell),
                              L"the pane cuts the comment off");
            Assert::IsTrue   (layout.lineCount > 1, L"wider than the screen's room");
            Assert::AreEqual ((float) (bounds.left + cell.left), GetLine (layout, 0)[0].leftPx);
            Assert::AreEqual ((float) (bounds.left + cell.left + 2 * kGlyphPx), GetLine (layout, 1)[0].leftPx, L"under the comment's text");
        }


        //  A BASIC line's values that the pane cuts off are laid over their
        //  cell the same way.
        TEST_METHOD (TheWindowLaysTheTipOverACutOffBasicValuesCell)
        {
            CassoTheme                                    theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                                       host;
            TipWindow                                     window (theme, host);
            DxuiDpiScaler                                 scaler;
            MockDxuiTextRenderer                          text;
            MockDxuiPainter                               painter;
            DxuiListView                                * list    = nullptr;
            RECT                                          bounds  = {};
            RECT                                          cell    = {};
            OperandResultTip::Layout                      layout;
            TipWindow::OperandTipCell                     tipCell;
            std::vector<std::vector<DxuiListView::Cell>>  rows;



            scaler.SetDpi (96);
            Build (window, theme, text);
            window.ShowPane (DebuggerLayout::kBasicProgram);
            window.Layout   (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);

            list = window.GetTipList (TipWindow::kBasicTipView);
            Assert::IsNotNull (list, L"the BASIC program pane's list");

            rows.push_back ({ DxuiListView::Cell {}, DxuiListView::Cell {}, DxuiListView::Cell { L"10 PRINT A$" },
                              DxuiListView::Cell { L"A$=\"" + std::wstring (300, L'X') + L"\"" } });
            list->SetRows (std::move (rows));
            list->Paint   (painter, text, theme);

            bounds = list->GetBounds();
            Assert::IsTrue (list->GetCellTextRectPx (0, BasicProgramPane::kValuesColumn, cell), L"the values are in view");
            Assert::IsTrue (window.GetOperandTips().TryGet (POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 },
                                                            scaler, text, MakeScreen(), layout, tipCell),
                            L"the pane cuts the long values off");
            Assert::AreEqual (TipWindow::kBasicTipView, tipCell.view);
            Assert::AreEqual ((float) (bounds.left + cell.left), GetLine (layout, 0)[0].leftPx, L"the tip's text starts at the cell's");
        }


        //  The BASIC listing's menu copies, has the line's breakpoint, and
        //  loads and saves the program.
        TEST_METHOD (TheBasicListingsMenuCopiesLoadsSavesAndClears)
        {
            CassoTheme                                    theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                                       host;
            TipWindow                                     window (theme, host);
            MockDxuiTextRenderer                          text;
            PaneMenuItems                                 items;
            std::vector<std::wstring>                     labels;
            IDebuggerPane                               * pane   = nullptr;
            DxuiListView                                * list   = nullptr;



            Build (window, theme, text);
            pane = window.FindPaneForTest (DebuggerLayout::kBasicProgram);
            list = window.GetTipList (TipWindow::kBasicTipView);

            Assert::IsTrue (pane->AddMenuItems (list, -1, items), L"the listing has a menu of its own, not its pane's Dock menu");

            for (const auto & [label, action] : items)
            {
                labels.push_back (label);
            }

            Assert::IsTrue (labels == std::vector<std::wstring> { L"Copy", L"Paste", L"", L"Load BASIC program...", L"Save BASIC program...", L"Clear BASIC program..." });
        }


        TEST_METHOD (TheTipIsUpAfterAMoveOrTheWheelAndDownAfterAPressOrAKey)
        {
            CassoTheme                theme = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            POINT                     at    = {};
            DxuiKeyEvent              key   = { DxuiKeyEventKind::Down, VK_SHIFT, false, false, false, false };
            OperandResultTip::Layout  first;



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"a move onto the cut-off operand shows the tip");
            Assert::AreEqual (0, window.GetOperandTips().GetCell().view);
            Assert::AreEqual (0, window.GetOperandTips().GetCell().row);

            first = window.GetOperandTips().GetLayout();

            //  The wheel over rows that stay where they are.
            Send (window, DxuiMouseEventKind::Wheel, at);
            Assert::IsTrue  (window.GetOperandTips().HasTip(), L"the wheel takes the tip down");
            Assert::IsTrue  (OperandResultTip::IsSame (first, window.GetOperandTips().GetLayout()), L"the wheel moves the tip off its cell");

            (void) window.OnKey (key);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a key leaves the tip up");

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue  (window.GetOperandTips().HasTip());

            Send (window, DxuiMouseEventKind::Down, at);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a press leaves the tip up");

            Send (window, DxuiMouseEventKind::Up, at);
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue  (window.GetOperandTips().HasTip());

            Send (window, DxuiMouseEventKind::Move, GetPointOffTip (window));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip is up after a move off it, onto nothing cut off");
        }


        TEST_METHOD (AButtonHeldDownTakesTheTipDown)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler;
            DxuiMouseEvent        move;



            scaler.SetDpi (96);
            Build (window, theme, text);

            move.kind        = DxuiMouseEventKind::Move;
            move.positionDip = GetOperandPoint (window, 0);

            window.GetOperandTips().Place (move, false, scaler, text, MakeScreen());
            Assert::IsTrue  (window.GetOperandTips().HasTip());

            window.GetOperandTips().Place (move, true, scaler, text, MakeScreen());
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a drag over the cell leaves the tip up");

            //  As the window gets it: a move made with the left button down.
            (void) window.OnMouse (move);
            Assert::IsTrue  (window.GetOperandTips().HasTip());

            move.button = DxuiMouseButton::Left;

            (void) window.OnMouse (move);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a move with the button down leaves the tip up");
        }


        TEST_METHOD (MovingToAnotherCutOffRowReplacesTheTip)
        {
            CassoTheme                theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  first;
            DxuiListView            * list   = nullptr;
            RECT                      cell   = {};



            Build (window, theme, text);
            list = window.GetCodeList (0);

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            first = window.GetOperandTips().GetLayout();

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 2));
            Assert::IsTrue   (window.GetOperandTips().HasTip());
            Assert::AreEqual (2, window.GetOperandTips().GetCell().row);
            Assert::IsFalse  (OperandResultTip::IsSame (first, window.GetOperandTips().GetLayout()), L"the tip still shows the first row's");

            Assert::IsTrue   (list->GetCellTextRectPx (2, DisassemblyCells::kOperandColumn, cell));
            Assert::AreEqual ((int) (list->GetBounds().top + cell.top), window.GetOperandTips().GetLayout().rowTop, L"over the third row");
        }


        TEST_METHOD (ThePointerLeavingTheCellByAnyRouteTakesTheTipDown)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            POINT                 at    = {};



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);

            Send (window, DxuiMouseEventKind::Move, at);
            window.GetOperandTips().CheckPointer (POINT { at.x + 50, at.y });
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a pointer still on the cell keeps the tip");

            //  Off the window's client area, as a pointer leaving across the
            //  frame or the caption is, with no move reaching the window.
            window.GetOperandTips().CheckPointer (POINT { at.x, -1 });
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a pointer off the window leaves the tip up");

            Send (window, DxuiMouseEventKind::Move, at);
            window.GetOperandTips().CheckPointer (GetPointOffTip (window));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a pointer on another row leaves the tip up");

            //  The tip stays up for a pointer moved from the row down into a tip
            //  that wraps below it.
            Send (window, DxuiMouseEventKind::Move, at);
            window.GetOperandTips().CheckPointer (POINT { at.x, window.GetOperandTips().GetLayout().rect.bottom - 1 });
            Assert::IsTrue  (window.GetOperandTips().HasTip(), L"a pointer on the tip takes it down");
        }


        TEST_METHOD (ASelectedRowsTipTakesTheSelectionFill)
        {
            CassoTheme            theme    = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiListView        * list     = nullptr;
            POINT                 at       = {};
            uint32_t              cellFill = 0;



            Build (window, theme, text);
            list = window.GetCodeList (0);

            //  The third row: the PC's row has an opaque fill of its own, which
            //  hides the selection's.
            at = GetOperandPoint (window, 2);

            Send (window, DxuiMouseEventKind::Down, at);
            Send (window, DxuiMouseEventKind::Up,   at);

            Assert::IsTrue      (list->IsListFocused(), L"the click gives the list the focus");
            Assert::AreEqual    (2, list->GetSelectedRow(), L"and selects the row");
            Assert::IsFalse     (list->HasTextSelection());
            Assert::AreNotEqual (theme.ContentSelection(), theme.ContentBackground());

            Send (window, DxuiMouseEventKind::Move, at);
            cellFill = list->GetCellsOfRow (2)[DisassemblyCells::kOperandColumn].background;

            Assert::IsTrue   (window.GetOperandTips().HasTip());
            Assert::AreEqual (DxuiColor::Composite (cellFill, DxuiColor::Composite (theme.ContentSelection(), theme.ContentBackground())),
                              window.GetOperandTips().GetLayout().fill, L"the tip is not in the selected row's fill");
        }


        //  A disassembly's rows lie on a text view's background, which a theme
        //  can set apart from a list's, as the debugger's light theme does. The
        //  tip is in the selection color over that background, whatever the
        //  cell's fill or the hover, and the list draws the tip's row in
        //  the same color from its left edge.
        TEST_METHOD (ATipIsInTheSelectionColorAndItsRowIsToo)
        {
            constexpr uint32_t        kTextView = 0xFF2A2A40;
            constexpr uint32_t        kHover    = 0x40FFFFFF;
            CassoTheme                theme     = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            DxuiListView            * list      = nullptr;
            POINT                     at        = {};
            OperandResultTip::Layout  layout;



            theme.textViewBg   = kTextView;
            theme.contentHover = kHover;
            Assert::AreNotEqual (theme.ContentBackground(), theme.TextViewBackground());

            Build (window, theme, text);
            list = window.GetCodeList (0);
            at   = GetOperandPoint (window, 2);

            Assert::IsTrue   (TryGetTip (window, at, text, layout), L"the pane cuts the third row's operand off");
            Assert::AreEqual (DxuiColor::Composite (theme.ContentSelection(), kTextView), layout.fill, L"the selection over the text view's fill");

            list->SetHoveredRow (2);

            Assert::IsTrue   (TryGetTip (window, at, text, layout));
            Assert::AreEqual (DxuiColor::Composite (theme.ContentSelection(), kTextView), layout.fill, L"the hover doesn't change it");

            Send             (window, DxuiMouseEventKind::Move, at);
            Assert::AreEqual (2, list->GetHighlightRow(), L"the list draws the tip's row in the tip's color");
        }


        TEST_METHOD (NewRowsThatChangeNothingKeepTheTip)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;



            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));

            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the same rows again take the tip down");
        }


        //  After a step under a pointer that has not moved, the tip is up, laid
        //  out for the row now under the pointer: up with the new text where
        //  the pane cuts it off, down where it fits, and up again where the
        //  next step cuts it off, with no move in between.
        TEST_METHOD (AfterAStepTheTipIsLaidOutForTheRowUnderTheRestingPointer)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                   host;
            TipWindow                                 window (theme, host);
            MockDxuiTextRenderer                      text;
            std::shared_ptr<DebuggerViewSnapshot>     snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());
            std::string                               other;
            std::wstring                              first;



            for (int i = 0; i < kLongOperandWords; i++)
            {
                other += "Y=02 Z=1 ";
            }

            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            first = window.GetOperandTips().GetLayout().runs[0].text;

            snapshot->codeViews[0][0].annotationParts.registers = other;
            snapshot->code                                      = snapshot->codeViews[0];
            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"the step takes the tip down");
            Assert::AreEqual (0, window.GetOperandTips().GetCell().row);
            Assert::AreEqual (TextEncoding::NarrowToWide (other.substr (0, other.size() - 1)), window.GetOperandTips().GetLayout().runs[0].text,
                              L"the tip is the row's before the step");
            Assert::AreNotEqual (first, window.GetOperandTips().GetLayout().runs[0].text);

            snapshot->codeViews[0][0].annotationParts.registers = "S=FD";
            snapshot->code                                      = snapshot->codeViews[0];
            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a tip is up over an operand that fits");

            snapshot->codeViews[0][0].annotationParts.registers = other;
            snapshot->code                                      = snapshot->codeViews[0];
            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue  (window.GetOperandTips().HasTip(), L"no tip for the cut-off operand under the resting pointer");
        }


        //  The wheel scrolls the BASIC listing under a pointer that stays
        //  put: the tip is laid out for the line now under the pointer, with
        //  no move in between, and goes where that line's values fit.
        TEST_METHOD (AfterTheWheelTheTipIsLaidOutForTheLineScrolledUnderThePointer)
        {
            CassoTheme                                    theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                                       host;
            TipWindow                                     window (theme, host);
            DxuiDpiScaler                                 scaler;
            MockDxuiTextRenderer                          text;
            MockDxuiPainter                               painter;
            DxuiListView                                * list    = nullptr;
            RECT                                          bounds  = {};
            RECT                                          cell    = {};
            POINT                                         at      = {};
            int                                           top     = 0;
            std::vector<std::vector<DxuiListView::Cell>>  rows;

            constexpr int                                 kLines  = 200;



            scaler.SetDpi (96);
            Build (window, theme, text);
            window.ShowPane (DebuggerLayout::kBasicProgram);
            window.Layout   (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);

            list = window.GetTipList (TipWindow::kBasicTipView);
            Assert::IsNotNull (list, L"the BASIC program pane's list");

            //  The first half of the lines' values are cut off, each in a
            //  different letter; the rest fit.
            for (int i = 0; i < kLines; i++)
            {
                std::wstring  values = (i < kLines / 2) ? L"A$=\"" + std::wstring (300, (wchar_t) (L'A' + i)) + L"\"" : std::wstring (L"A=1");

                rows.push_back ({ DxuiListView::Cell {}, DxuiListView::Cell {}, DxuiListView::Cell { L"10 PRINT A$" }, DxuiListView::Cell { values } });
            }

            list->SetRows (std::move (rows));
            list->Paint   (painter, text, theme);

            bounds = list->GetBounds();
            Assert::IsTrue (list->GetCellTextRectPx (0, BasicProgramPane::kValuesColumn, cell), L"the values are in view");

            at = POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 };

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue   (window.GetOperandTips().HasTip());
            Assert::AreEqual (0, window.GetOperandTips().GetCell().row);

            Send (window, DxuiMouseEventKind::Wheel, at);
            top = list->GetTopRow();

            Assert::IsTrue   (top > 0, L"the wheel didn't scroll the listing");
            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"the wheel takes the tip down");
            Assert::AreEqual (top, window.GetOperandTips().GetCell().row, L"the tip is the line's that scrolled away");
            Assert::AreEqual (list->GetCellsOfRow (top)[BasicProgramPane::kValuesColumn].text, window.GetOperandTips().GetLayout().runs[0].text);

            for (int each = 0; each < kLines && list->GetTopRow() < kLines / 2; each++)
            {
                Send (window, DxuiMouseEventKind::Wheel, at);
            }

            Assert::IsTrue  (list->GetTopRow() >= kLines / 2, L"the wheel didn't scroll to the lines that fit");
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a tip is up over values that fit");
        }


        //  The tip is the row's, so new rows that move its operand along lay
        //  it out again where the operand is now.
        TEST_METHOD (NewRowsThatWidenAColumnAheadLayTheTipOutAgain)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                   host;
            TipWindow                                 window (theme, host);
            MockDxuiTextRenderer                      text;
            std::shared_ptr<DebuggerViewSnapshot>     snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());
            float                                     before   = 0.0f;



            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            before = window.GetOperandTips().GetLayout().runs[0].leftPx;

            //  The tip's own row is as it was; another's instruction is wider,
            //  which widens its column and moves the operand along.
            snapshot->codeViews[0][1].instruction = "LDA A_LONG_SYMBOL_WIDENING_THE_COLUMN,X";
            snapshot->code                        = snapshot->codeViews[0];

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the new rows take the tip down");
            Assert::IsTrue (window.GetOperandTips().GetLayout().runs[0].leftPx > before, L"the tip stays where the operand was");
        }


        TEST_METHOD (AfterThePcMovesOffTheRowTheTipKeepsItsColor)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                   host;
            TipWindow                                 window (theme, host);
            MockDxuiTextRenderer                      text;
            std::shared_ptr<DebuggerViewSnapshot>     snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());
            uint32_t                                  fill     = 0;



            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            fill = window.GetOperandTips().GetLayout().fill;

            //  The text is the same; only the PC's row fill leaves the row.
            snapshot->codeViews[0][0].isCurrent = false;
            snapshot->codeViews[0][1].isCurrent = true;
            snapshot->code                      = snapshot->codeViews[0];
            snapshot->pc                        = snapshot->codeViews[0][1].address;

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue      (window.GetOperandTips().HasTip(), L"the PC's move off the row takes the tip down");
            Assert::AreEqual    (fill, window.GetOperandTips().GetLayout().fill, L"the tip stays in the selection color, not the PC's row color");
        }


        TEST_METHOD (AfterANewLayoutTheTipIsLaidOutForThePaneAsItIsNow)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                    host;
            TipWindow                  window (theme, host);
            MockDxuiTextRenderer       text;
            DxuiDpiScaler              scaler;
            OperandResultTip::Layout   layout;



            scaler.SetDpi (96);
            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            window.Layout (RECT { 0, 0, kWindowWidth - kNarrowerPx, kWindowHeight }, scaler);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a new layout takes the tip down");
            Assert::IsTrue (TryGetTip (window, GetOperandPoint (window, 0), text, layout), L"the narrower pane cuts the operand off");
            Assert::IsTrue (OperandResultTip::IsSame (layout, window.GetOperandTips().GetLayout()), L"the tip is laid out for the pane as it was");
        }


        TEST_METHOD (TheColorTipShowsBelowTheOperandTip)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            RECT                  under = {};
            RECT                  tip   = {};



            Build (window, theme, text);

            //  The move that puts the operand tip up also starts the operand's
            //  color tip, which waits out its dwell.
            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            under = window.GetTooltip().GetPendingAnchor();
            tip   = window.GetOperandTips().GetLayout().rect;

            Assert::IsTrue (window.GetOperandTips().HasTip());
            Assert::IsTrue (window.GetTooltip().WantsTick(), L"the operand's color tip is not on its way");
            Assert::IsTrue (EqualRect (&under, &tip) != FALSE, L"the color tip is placed by its cell, under the operand tip");

            //  A plain row's operand has neither tip: the operand's colors are
            //  on the column's heading.
            Send (window, DxuiMouseEventKind::Move, GetPointOffTip (window));

            Assert::IsFalse (window.GetOperandTips().HasTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"a color tip over a plain row's operand");
        }


        TEST_METHOD (TheColorTipGoesWithTheOperandTip)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                host;
            TipWindow                              window (theme, host);
            MockDxuiTextRenderer                   text;
            DxuiKeyEvent                           key      = { DxuiKeyEventKind::Down, VK_SHIFT, false, false, false, false };
            POINT                                  at       = {};
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);

            //  A key.
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetTooltip().WantsTick());

            (void) window.OnKey (key);

            Assert::IsFalse (window.GetOperandTips().HasTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after a key, the color tip still comes up below a tip no longer there");

            //  The pointer leaving the cell with no move reaching the window.
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetTooltip().WantsTick());

            window.GetOperandTips().CheckPointer (POINT { at.x, -1 });

            Assert::IsFalse (window.GetOperandTips().HasTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after the pointer leaves, the color tip still comes up below a tip no longer there");

            //  New rows that move the cell.
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetTooltip().WantsTick());

            snapshot->codeViews[0][1].instruction = "LDA A_LONG_SYMBOL_WIDENING_THE_COLUMN,X";
            snapshot->code                        = snapshot->codeViews[0];

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue  (window.GetOperandTips().HasTip(), L"the tip is laid out again for the row under the pointer");
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after new rows, the color tip comes up below the tip laid out before them");
        }


        TEST_METHOD (NoTipCoversTextSelectedInTheCell)
        {
            CassoTheme            theme  = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiListView        * list   = nullptr;



            Build (window, theme, text);
            list = window.GetCodeList (0);

            DragAlong (window, 0);
            Assert::IsTrue (list->IsCellTextSelected (0, DisassemblyCells::kOperandColumn), L"the drag selects part of the operand");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip lies over the selection");

            //  A row whose tip reaches nowhere near the selection still has one.
            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 2));
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a selection in one row takes every row's tip away");
        }


        TEST_METHOD (NoTipCoversTextSelectedInARowItReachesOver)
        {
            CassoTheme            theme  = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiListView        * list   = nullptr;



            Build (window, theme, text);
            list = window.GetCodeList (0);

            //  The short row under the first, whose tip wraps over it.
            DragAlong (window, 1);
            Assert::IsTrue (list->IsCellTextSelected (1, DisassemblyCells::kOperandColumn), L"the drag selects part of the operand");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip of the row above lies over the selection");
        }


        //  Below its row the tip is taken to cover every column, so a
        //  selection in another column of a row it lies over takes it away
        //  too.
        TEST_METHOD (NoTipCoversTextSelectedInAnyColumnOfARowItWrapsOver)
        {
            CassoTheme            theme  = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiListView        * list   = nullptr;
            RECT                  bounds = {};
            RECT                  cell   = {};
            DxuiMouseEvent        ev;



            Build (window, theme, text);
            list   = window.GetCodeList (0);
            bounds = list->GetBounds();

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the first row has a tip before anything is selected");

            Assert::IsTrue (list->GetCellTextRectPx (1, DisassemblyCells::kInstructionColumn, cell), L"the row below's mnemonic is in view");

            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 };
            (void) window.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Move;
            ev.positionDip.x = bounds.left + cell.right - 1;
            (void) window.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Up;
            (void) window.OnMouse (ev);

            Assert::IsTrue (list->IsCellTextSelected (1, DisassemblyCells::kInstructionColumn), L"the drag selects the mnemonic");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip of the row above lies over the selected mnemonic");
        }


        TEST_METHOD (NoTipShowsUnderAWindowLyingOverThePane)
        {
            CassoTheme                   theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                      host;
            TipWindow                    window (theme, host);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;
            TipWindow::OperandTipScreen  screen = MakeScreen();
            POINT                        at     = {};



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);
            Assert::IsTrue (TryGetTip (window, at, text, layout));

            //  A floating command bar over the rows the tip reaches down over,
            //  clear of the pointer and of the cell.
            screen.overWindows.push_back (RECT { layout.rect.left + kOverGapPx, layout.rect.bottom - kOverGapPx,
                                                 layout.rect.left + kOverSizePx, layout.rect.bottom + kOverSizePx });
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip shows over the floating window");

            //  The same window clear of the tip.
            OffsetRect (&screen.overWindows[0], 0, kOverSizePx);
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a window clear of the tip takes it away");
        }


        TEST_METHOD (ACellAcrossTwoMonitorsHasItsTipOnBoth)
        {
            CassoTheme                   theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                      host;
            TipWindow                    window (theme, host);
            MockDxuiTextRenderer         text;
            TipWindow::OperandTipScreen  screen;
            DxuiListView               * list    = nullptr;
            RECT                         bounds  = {};
            RECT                         cell    = {};
            LONG                         start   = 0;
            LONG                         split   = 0;
            LONG                         visible = 0;
            POINT                        at      = {};



            Build (window, theme, text);

            list    = window.GetCodeList (0);
            bounds  = list->GetBounds();
            cell    = GetCellRect (window, 0);
            start   = bounds.left + cell.left;
            visible = bounds.right - (list->IsScrollbarVisible() ? list->GetScrollbarWidthPx() : 0);

            //  The monitors meet a little way into the cell's text, and the
            //  pointer is on the part of the cell on the second.
            split            = start + kSplitIntoTextPx;
            at               = POINT { (split + visible) / 2, GetOperandPoint (window, 0).y };
            screen.workAreas = { RECT { 0, 0, split, kScreenBelow }, RECT { split, 0, kScreenRight, kScreenBelow } };
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"a pointer on the second monitor has no tip");
            Assert::AreEqual ((float) start, window.GetOperandTips().GetLayout().runs[0].leftPx, L"the text starts where the cell's does, on the first monitor");
            Assert::IsTrue   (window.GetOperandTips().GetLayout().rect.right > visible, L"the tip runs on past the pane, on the second monitor");
            Assert::IsTrue   (window.GetOperandTips().GetLayout().rect.right <= kScreenRight, L"the tip runs off the second monitor");

            //  A gap between them, as a taskbar down the first one's right side
            //  leaves: the tip keeps to the first monitor, with the pointer on
            //  the second.
            screen.workAreas[0].right = split - kGapPx;
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a pointer on the second monitor has no tip");
            Assert::IsTrue (window.GetOperandTips().GetLayout().rect.right <= split - kGapPx, L"the tip runs into the gap");
        }


        //  The tip is on the monitor the row starts on and the one directly
        //  to its right, and doesn't extend onto a third monitor past those
        //  two, so the text wraps at the second one's edge.
        TEST_METHOD (TheTipLiesOnTheRowsMonitorAndTheOneBesideIt)
        {
            constexpr LONG               kSecondPx = 600;

            CassoTheme                   theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                      host;
            TipWindow                    window (theme, host);
            MockDxuiTextRenderer         text;
            TipWindow::OperandTipScreen  screen;
            LONG                         split   = 0;
            POINT                        at      = {};



            Build (window, theme, text);

            at    = GetOperandPoint (window, 0);
            split = window.GetCodeList (0)->GetBounds().left + GetCellRect (window, 0).left + kSplitIntoTextPx;

            screen.workAreas = { RECT { 0, 0, split, kScreenBelow }, RECT { split, 0, split + kSecondPx, kScreenBelow },
                                 RECT { split + kSecondPx, 0, kScreenRight, kScreenBelow } };
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue   (window.GetOperandTips().HasTip());
            Assert::IsTrue   (window.GetOperandTips().GetLayout().rect.right <= split + kSecondPx, L"the tip runs onto the third monitor");
            Assert::IsTrue   (window.GetOperandTips().GetLayout().lineCount > 2, L"the operand wraps at the second monitor's edge");
            Assert::AreEqual (GetRowStart (*window.GetCodeList (0), 0), window.GetOperandTips().GetLayout().rect.left, L"from the row's address, unmoved");
        }


        TEST_METHOD (NoTipShowsOverAHeldColorKey)
        {
            CassoTheme                theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;
            ColorKeyButton          * button = nullptr;
            RECT                      key    = {};
            RECT                      both   = {};
            POINT                     at     = {};



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);
            Assert::IsTrue (TryGetTip (window, at, text, layout));

            button = window.GetColorKeys().Find (DebuggerLayout::GetCodePaneId (0));
            Assert::IsNotNull (button);
            Assert::IsTrue    (button->IsVisible(), L"the disassembly's info button shows");

            Send (window, DxuiMouseEventKind::Down, GetCenter (button->GetBounds()));
            Send (window, DxuiMouseEventKind::Up,   GetCenter (button->GetBounds()));
            key = window.GetColorKeys().GetPopup().GetRect();

            Assert::IsTrue (window.GetColorKeys().GetPopup().IsHeld(), L"the click holds the key open");
            Assert::IsTrue (IntersectRect (&both, &key, &layout.rect) != FALSE, L"the key hangs over the tip's cell");

            Assert::IsFalse (TryGetTip (window, at, text, layout), L"the tip shows over the key");
        }


        TEST_METHOD (NoTipShowsOverAPaneSlidOutOverTheDisassembly)
        {
            CassoTheme                theme = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            MockDxuiPainter           painter;
            DxuiDpiScaler             scaler;
            OperandResultTip::Layout  layout;
            RECT                      slid  = {};
            RECT                      both  = {};
            POINT                     at    = {};



            scaler.SetDpi (96);
            Build (window, theme, text);

            Assert::IsTrue (window.EditPaneLayout().AutoHide (DebuggerLayout::kConsole, DxuiDockSide::Top));
            window.GetDockSite()->Relayout();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.GetCodeList (0)->Paint (painter, text, theme);

            at = GetOperandPoint (window, 0);
            Assert::IsTrue (TryGetTip (window, at, text, layout), L"with the console tucked away, the tip shows");

            window.GetDockSite()->SlideOut (DebuggerLayout::kConsole);
            slid = window.GetDockSite()->GetSlidRect();

            Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), window.GetDockSite()->GetSlidPane());
            Assert::IsTrue   (IntersectRect (&both, &slid, &layout.rect) != FALSE, L"the console slides out over the tip's cell");

            Assert::IsFalse (TryGetTip (window, at, text, layout), L"the tip shows over the slid-out console");
        }


        TEST_METHOD (NoTipShowsOverTheOpenZoomPopup)
        {
            CassoTheme                theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            MockDxuiPainter           painter;
            DxuiDpiScaler             scaler;
            DxuiListView            * list    = nullptr;
            OperandResultTip::Layout  layout;
            RECT                      zoom    = {};
            RECT                      both    = {};
            RECT                      cell    = {};
            POINT                     field   = {};
            POINT                     at      = {};
            bool                      isFound = false;



            scaler.SetDpi (96);
            Build (window, theme, text);

            //  The disassembly alone, filling the window down to the status
            //  bar, under the zoom field at its right end.
            for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
            {
                if (!pane.starts_with (DebuggerLayout::kCode) && window.IsPaneShown (pane))
                {
                    Assert::IsTrue (window.EditPaneLayout().AutoHide (pane, DxuiDockSide::Left), pane.c_str());
                }
            }

            window.GetDockSite()->Relayout();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (kManyRows), 0);

            list = window.GetCodeList (0);
            list->Paint (painter, text, theme);

            field = GetCenter (window.GetStatusBar()->GetFieldRect (window.kStatusZoom));

            Send (window, DxuiMouseEventKind::Down, field);
            Assert::IsTrue (window.IsZoomPopupOpen());
            zoom = window.GetZoomPopupRect();
            Send (window, DxuiMouseEventKind::Down, field);
            Assert::IsFalse (window.IsZoomPopupOpen());

            //  A row whose tip lies under where the popup opens.
            for (int row = 0; row < kManyRows && !isFound; row++)
            {
                if (list->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell))
                {
                    at      = GetOperandPoint (window, row);
                    isFound = TryGetTip (window, at, text, layout) && IntersectRect (&both, &zoom, &layout.rect) != FALSE;
                }
            }

            Assert::IsTrue (isFound, L"no row's tip lies under the zoom popup");

            Send (window, DxuiMouseEventKind::Down, field);
            Assert::IsTrue  (window.IsZoomPopupOpen());
            Assert::IsFalse (TryGetTip (window, at, text, layout), L"the tip shows over the zoom popup");
        }


        static POINT  GetCenter (const RECT & rect)
        {
            return POINT { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
        }


        //  A row whose operand the pane cuts off, broken by spaces, then one
        //  that fits, then another cut off; or `rows` rows, every one cut off.
        static std::shared_ptr<const DebuggerViewSnapshot>  MakeSnapshot (int rows = 0)
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            std::string                     longOperand;
            DebuggerViewSnapshot::CodeLine  lda;
            DebuggerViewSnapshot::CodeLine  rts;
            DebuggerViewSnapshot::CodeLine  sta;



            for (int i = 0; i < kLongOperandWords; i++)
            {
                longOperand += "A=00 X=01 ";
            }

            lda.address                   = 0x0300;
            lda.bytes                     = "A5 10";
            lda.instruction               = "LDA $10";
            lda.annotationParts.registers = longOperand;
            lda.effect                    = "A=41";
            lda.isCurrent                 = true;
            rts.address                   = 0x0302;
            rts.bytes                     = "60";
            rts.instruction               = "RTS";
            rts.annotationParts.registers = "S=FD";
            sta.address                   = 0x0303;
            sta.bytes                     = "85 11";
            sta.instruction               = "STA $11";
            sta.annotationParts.registers = longOperand;

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda, rts, sta };
            snapshot->codeOpen[0]  = true;

            if (rows > 0)
            {
                snapshot->codeViews[0].clear();

                for (int i = 0; i < rows; i++)
                {
                    sta.address = (Word) (0x0300 + i * 2);
                    snapshot->codeViews[0].push_back (sta);
                }
            }

            snapshot->code = snapshot->codeViews[0];

            return snapshot;
        }


        static constexpr int  kWindowWidth      = 1400;
        static constexpr int  kWindowHeight     = 900;
        static constexpr int  kNarrowerPx       = 300;
        static constexpr int  kLongOperandWords = 20;
        static constexpr int  kManyRows         = 80;
        static constexpr int  kDragPx           = 20;
        static constexpr int  kOverGapPx        = 10;
        static constexpr int  kOverSizePx       = 200;
        static constexpr int  kSplitIntoTextPx  = 60;
        static constexpr int  kGapPx            = 20;
        static constexpr int  kLastRow          = 2;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BranchArrowTipTests
    //
    //  The tip over the PC's branch arrow has whether the branch will be
    //  taken; the tip over the dotted arrow has the branch the last step
    //  took to the PC.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BranchArrowTipTests)
    {
    public:

        static constexpr int  kWindowWidth  = 1400;
        static constexpr int  kWindowHeight = 900;

        //  $0300 DEY, $0302 BNE $0300 at the PC, which won't branch, $0304
        //  DEX, $0306 JMP $0302, the step that led to the PC.
        static std::shared_ptr<const DebuggerViewSnapshot>  MakeSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  dey;
            DebuggerViewSnapshot::CodeLine  bne;
            DebuggerViewSnapshot::CodeLine  dex;
            DebuggerViewSnapshot::CodeLine  jmp;



            dey.address       = 0x0300;
            dey.bytes         = "88";
            dey.instruction   = "DEY";
            bne.address       = 0x0302;
            bne.bytes         = "D0 FC";
            bne.instruction   = "BNE $0300";
            bne.isCurrent     = true;
            bne.target        = 0x0300;
            bne.isTargetTaken = false;
            dex.address       = 0x0304;
            dex.bytes         = "CA";
            dex.instruction   = "DEX";
            jmp.address       = 0x0306;
            jmp.bytes         = "4C 02 03";
            jmp.instruction   = "JMP $0302";
            jmp.target        = 0x0302;

            snapshot->pc           = 0x0302;
            snapshot->isPaused     = true;
            snapshot->branchedFrom = 0x0306;
            snapshot->codeViews[0] = { dey, bne, dex, jmp };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];
            return snapshot;
        }

        //  A point on the arrows' upright, left of the mnemonics, halfway
        //  down from one row to the next.
        static POINT  GetUprightPoint (TipWindow & window, int fromRow)
        {
            DxuiListView  * list   = window.GetCodeList (0);
            RECT            bounds = list->GetBounds();
            RECT            from   = {};
            RECT            to     = {};



            Assert::IsTrue (list->GetCellTextRectPx (fromRow,     DisassemblyCells::kInstructionColumn, from));
            Assert::IsTrue (list->GetCellTextRectPx (fromRow + 1, DisassemblyCells::kInstructionColumn, to));

            return POINT { bounds.left + from.left - (LONG) (BranchArrow::Input().marginPx + BranchArrow::Input().stubPx),
                           bounds.top + (from.top + from.bottom + to.top + to.bottom) / 4 };
        }


        TEST_METHOD (EachArrowsTipHasWhatItsBranchDoes)
        {
            CassoTheme            theme;
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            MockDxuiPainter       painter;
            DxuiDpiScaler         scaler;
            DxuiTip               tip;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);
            window.GetCodeList (0)->Paint (painter, text, theme);

            Assert::IsTrue (static_cast<const DxuiPanel &> (window).TryGetTip (GetUprightPoint (window, 0), tip), L"the PC's arrow, from its row up to the target's");
            Assert::AreEqual (std::wstring (L"This branch won't be taken"), tip.text);

            Assert::IsTrue   (static_cast<const DxuiPanel &> (window).TryGetTip (GetUprightPoint (window, 2), tip), L"the dotted arrow, from the step's row up to the PC's");
            Assert::AreEqual (std::wstring (L"This jump was taken"), tip.text);
        }
    };
}
