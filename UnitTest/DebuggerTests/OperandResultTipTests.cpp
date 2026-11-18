#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "DebuggerTests/OperandTipWindow.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/SourceRows.h"
#include "Ui/Debugger/OperandResultTip.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace OperandResultTipTests
{
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

        static constexpr int       kRowLeft     = 20;
        static constexpr int       kTextLeft    = 100;
        static constexpr int       kCodeLeft    = 60;
        static constexpr int       kRowTop      = 40;
        static constexpr float     kFontPx      = 12.0f;
        static constexpr uint32_t  kFill        = 0xFF2A2D2E;
        static constexpr uint32_t  kInk         = 0xFFDCDCDC;
        static constexpr uint32_t  kOperand     = 0xFF57A64A;
        static constexpr uint32_t  kResult      = 0xFF4EC9B0;


        //  Room for `chars` characters of text from the operand's start, to
        //  the view's edge.
        static int  GetEdgeFor (int chars)
        {
            return kTextLeft + chars * TipMetrics::kGlyphPx;
        }


        //  A row from kRowLeft to the view's right edge, on one screen.
        static OperandResultTip::Placement  MakePlacement (int visibleRight)
        {
            OperandResultTip::Placement  placement;



            placement.row         = RECT { kRowLeft, kRowTop, visibleRight, kRowTop + TipMetrics::kRowHeight };
            placement.visibleLeft = kRowLeft;
            placement.codeLeftPx  = (float) kCodeLeft;
            placement.padLeftPx   = TipMetrics::kPadPx;
            placement.padRightPx  = TipMetrics::kPadPx;
            placement.fontPx      = kFontPx;
            placement.face        = DxuiTheme::kMonoFace;
            placement.fill        = kFill;
            placement.workArea    = TipMetrics::kScreen;
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
            size_t                     at      = cell.text.find (SourceRows::kpszResultPrefix);



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


        static std::vector<RecordedPaintCall>  GetFills (const MockDxuiPainter & painter)
        {
            std::vector<RecordedPaintCall>  fills;



            std::ranges::copy_if (painter.Calls(), std::back_inserter (fills), [] (const RecordedPaintCall & call) { return call.kind == RecordedPaintKind::FillRect; });
            return fills;
        }


        TEST_METHOD (AppearsOnlyWhereTheViewCutsTextOff)
        {
            OperandResultTip::Placement  placement = MakeRow ("$0010=41", "A=41", TipMetrics::kScreenRight);
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

            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 $1234=41", "A=41", TipMetrics::kScreenRight);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            text.SetCannedMetrics (placement.segments[1].text, SIZE { kTextPx, TipMetrics::kRowHeight });

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
            Assert::AreEqual (kTextLeft - TipMetrics::kPadPx, layout.firstLeft, L"the row's line from the operand's padding");
            Assert::AreEqual (kFill, layout.fill, L"the row's highlight");

            OperandResultTip::Paint (layout, painter, text);
            fills = GetFills (painter);

            Assert::AreEqual ((size_t) 2, fills.size());
            Assert::AreEqual ((float) (kTextLeft - TipMetrics::kPadPx - kRowLeft), fills[0].x, L"the row's line from its redrawn text");
            Assert::AreEqual ((float) TipMetrics::kRowHeight,                       fills[0].height);
            Assert::AreEqual ((float) (layout.rect.right - kTextLeft + TipMetrics::kPadPx), fills[0].width, L"to the tip's right edge");
            Assert::AreEqual (0.0f,                                     fills[1].x, L"the line below from the row's start");
            Assert::AreEqual ((float) TipMetrics::kRowHeight,                       fills[1].y);
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
            OperandResultTip::Placement  placement = MakePlacement (kCodeLeft + 2 * TipMetrics::kGlyphPx);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (std::wstring (10, L' '), kCodeLeft, placement));
            placement.segments.push_back (MakeSegment (L"BNE w_inner", kCodeLeft + 11 * TipMetrics::kGlyphPx, placement));

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (std::wstring (L"BNE w_inner"), TipMetrics::GetLineText (layout, 0));
        }


        //  The row's line is its text as the view draws it, run on past
        //  the view's edge to the end of the part the view cuts off.
        TEST_METHOD (TheRowsOwnLineRunsOnPastTheViewsEdge)
        {
            OperandResultTip::Placement  placement = MakePlacement (kCodeLeft + 4 * TipMetrics::kGlyphPx);
            MockDxuiTextRenderer         text;
            MockDxuiPainter              painter;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (L"0300", kRowLeft + TipMetrics::kPadPx, placement));
            placement.segments.push_back (MakeSegment (L"BNE w_inner", kCodeLeft, placement));

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (1, layout.lineCount, L"one line");
            Assert::AreEqual (std::wstring (L"BNE w_inner"), TipMetrics::GetLineText (layout, 0), L"from the part the view cuts off, the address left to the view");
            Assert::AreEqual ((float) kCodeLeft,             TipMetrics::GetLine (layout, 0)[0].leftPx, L"where the view draws it");
            Assert::AreEqual ((LONG) (kCodeLeft + 11 * TipMetrics::kGlyphPx + TipMetrics::kPadPx), layout.rect.right, L"run on past the view to its end");
            Assert::AreEqual ((LONG) (kCodeLeft - TipMetrics::kPadPx), layout.rect.left, L"a tip of the row's line alone starts at its redrawn text");

            OperandResultTip::Paint (layout, painter, text);

            Assert::AreEqual ((float) kCodeLeft, (float) layout.rect.left + text.Calls()[0].x, L"the text lands on the view's");
            Assert::AreEqual ((float) kRowTop,   (float) layout.rect.top  + text.Calls()[0].y);
            Assert::AreEqual ((float) TipMetrics::kRowHeight, text.Calls()[0].height, L"a row high, as the row is");
        }


        //  A part drawn whole that the part ahead of it, run on, would run
        //  into, follows that part, a padding's width apart, as columns are.
        TEST_METHOD (APartRunIntoByTheOneAheadFollowsIt)
        {
            OperandResultTip::Placement         placement = MakePlacement (TipMetrics::kScreenRight);
            MockDxuiTextRenderer                text;
            OperandResultTip::Layout            layout;
            std::vector<OperandResultTip::Run>  runs;



            placement.segments.push_back (MakeSegment (L"a_long_label", kCodeLeft, placement));
            placement.segments.push_back (MakeSegment (L"RTS", kTextLeft, placement));
            placement.segments[0].clipRightPx = (float) (kTextLeft - TipMetrics::kPadPx);

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout), L"a column narrower than its label cuts it off");

            runs = TipMetrics::GetLine (layout, 0);

            Assert::AreEqual ((size_t) 2, runs.size());
            Assert::AreEqual ((float) kCodeLeft, runs[0].leftPx);
            Assert::AreEqual ((float) (kCodeLeft + 12 * TipMetrics::kGlyphPx + 2 * TipMetrics::kPadPx), runs[1].leftPx, L"pushed along past the label");
        }


        //  A part pushed along to the screen's edge by a cut-off part ahead of
        //  it wraps there, as the last part does, and the part after it
        //  follows it. A part whose place is past the edge is cut there. None
        //  of this takes the tip down.
        TEST_METHOD (APartPushedToTheScreensEdgeWrapsThere)
        {
            constexpr int                       kEdgePx   = 210;
            constexpr int                       kAfterPx  = 180;

            OperandResultTip::Placement         placement = MakePlacement (TipMetrics::kScreenRight);
            MockDxuiTextRenderer                text;
            OperandResultTip::Layout            layout;
            float                               pushedPx  = (float) (kCodeLeft + 12 * TipMetrics::kGlyphPx + 2 * TipMetrics::kPadPx);



            placement.segments.push_back (MakeSegment (L"a_long_label", kCodeLeft, placement));
            placement.segments.push_back (MakeSegment (L"LDA (PTR),Y", kTextLeft, placement));
            placement.segments.push_back (MakeSegment (L"; go", kAfterPx, placement));
            placement.segments[0].clipRightPx = (float) (kTextLeft - TipMetrics::kPadPx);
            placement.workArea.right          = kEdgePx + TipMetrics::kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout), L"a part pushed off the screen takes the tip down");
            Assert::AreEqual (3, layout.lineCount);
            Assert::AreEqual ((size_t) 2, TipMetrics::GetLine (layout, 0).size());
            Assert::AreEqual (std::wstring (L"a_long_label"), TipMetrics::GetLine (layout, 0)[0].text);
            Assert::AreEqual (std::wstring (L"LDA"),          TipMetrics::GetLine (layout, 0)[1].text, L"broken at its space at the screen's edge");
            Assert::AreEqual (pushedPx,                       TipMetrics::GetLine (layout, 0)[1].leftPx);
            Assert::AreEqual (std::wstring (L"(PTR),Y"),      TipMetrics::GetLineText (layout, 1));
            Assert::AreEqual (pushedPx,                       TipMetrics::GetLine (layout, 1)[0].leftPx, L"continued at its left");
            Assert::AreEqual (std::wstring (L"; go"),         TipMetrics::GetLineText (layout, 2), L"the next part on the next line");
            Assert::AreEqual ((float) kAfterPx,               TipMetrics::GetLine (layout, 2)[0].leftPx, L"at its left");
            Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"on the screen");

            //  A part that starts past the screen's edge is cut there.
            placement.segments[2].leftPx = (float) (kEdgePx + TipMetrics::kPadPx);

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout), L"a part past the screen's edge takes the tip down");
            Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"on the screen");
            Assert::AreEqual (std::wstring (L"(PTR),Y"),      TipMetrics::GetLine (layout, 1)[0].text);
        }


        //  A narrow pane cuts the branch off at its edge, and its operand and
        //  result lie out of view. The branch runs on along the row's line,
        //  and the operand, with its symbol, and the result are each on a
        //  line below, under the mnemonic.
        TEST_METHOD (AnOperandOutOfViewLiesBelowUnderTheCode)
        {
            OperandResultTip::Placement  placement = MakePlacement (kCodeLeft + 4 * TipMetrics::kGlyphPx);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.segments.push_back (MakeSegment (L"BNE w_inner", kCodeLeft, placement));
            placement.segments.push_back (MakeOperand ("Z=0", "PC=$60BA; 3 cycles", placement));
            placement.operand = 1;
            placement.symbol  = L"w_inner=$60BA";

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (3, layout.lineCount);
            Assert::AreEqual (std::wstring (L"BNE w_inner"),       TipMetrics::GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"Z=0 w_inner=$60BA"), TipMetrics::GetLineText (layout, 1), L"the symbol another of the operand's values");
            Assert::AreEqual (std::wstring (L"Result: PC=$60BA; 3 cycles"), TipMetrics::GetLineText (layout, 2));
            Assert::AreEqual ((float) kCodeLeft, TipMetrics::GetLine (layout, 1)[0].leftPx, L"the operand under the mnemonic");
            Assert::AreEqual ((float) kCodeLeft, TipMetrics::GetLine (layout, 2)[0].leftPx, L"the result under the mnemonic");
            Assert::AreEqual (kOperand, TipMetrics::GetLine (layout, 1)[0].colors.back(), L"the symbol in the operand's color");
            Assert::IsTrue   (layout.hasSymbol);

            //  With nothing ahead of it cut off, the tip starts below the row.
            placement.segments[0].text = L"BNE";

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::IsTrue   (TipMetrics::GetLine (layout, 0).empty(), L"nothing on the row's line");
            Assert::AreEqual ((LONG) (kRowTop + TipMetrics::kRowHeight), layout.rect.top, L"from the line below the row");
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
            Assert::AreEqual (std::wstring (L"A=00 X=01 $1234=41"), TipMetrics::GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"Result: A=41"),       TipMetrics::GetLineText (layout, 1));
            Assert::AreEqual ((float) kTextLeft, TipMetrics::GetLine (layout, 0)[0].leftPx, L"the operand where the view draws it");
            Assert::AreEqual ((float) kTextLeft, TipMetrics::GetLine (layout, 1)[0].leftPx, L"the result under the operand's start");
            Assert::AreEqual (kOperand, TipMetrics::GetLine (layout, 0)[0].colors.front(), L"the operand's color");
            Assert::AreEqual (kResult,  TipMetrics::GetLine (layout, 1)[0].colors.front(), L"the result's color");

            //  The symbol joins the operand on its line.
            placement.symbol = L"w_inner=$60BA";

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (std::wstring (L"A=00 X=01 $1234=41 w_inner=$60BA"), TipMetrics::GetLineText (layout, 0));
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



            placement.workArea.right = GetEdgeFor (kRoomChars) + TipMetrics::kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (3, layout.lineCount, L"two lines of operand, one of result");
            Assert::AreEqual (std::wstring (L"A=00 X=01 Y=02 $1234=41"), TipMetrics::GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"C=1 Z=0 N=1 V=0"),         TipMetrics::GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"Result: A=41"),            TipMetrics::GetLineText (layout, 2));

            for (const OperandResultTip::Run & run : layout.runs)
            {
                Assert::IsTrue   ((int) run.text.size() <= kRoomChars, run.text.c_str());
                Assert::AreEqual ((float) kTextLeft, run.leftPx, L"every line of a part starts at the same left");
            }

            Assert::AreEqual (TextEncoding::NarrowToWide (operand), TipMetrics::GetLineText (layout, 0) + L" " + TipMetrics::GetLineText (layout, 1), L"a break drops part of the operand");
        }


        TEST_METHOD (AWordWiderThanALineBreaksWhereTheLineEnds)
        {
            constexpr int                kRoomChars = 24;

            OperandResultTip::Placement  placement = MakeRow ("Turn_on_the_80_column_display_now", "", GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.workArea.right = GetEdgeFor (kRoomChars) + TipMetrics::kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (2, layout.lineCount);
            Assert::AreEqual (std::wstring (L"Turn_on_the_80_column_di"), TipMetrics::GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"splay_now"),                TipMetrics::GetLineText (layout, 1));
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
            Assert::AreEqual (std::wstring (L"A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0"), TipMetrics::GetLineText (layout, 0));
            Assert::AreEqual ((LONG) (GetEdgeFor (39) + TipMetrics::kPadPx), layout.rect.right, L"the tip runs on past the view to the end of the operand");
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
            placement.segments[1].resultAt  = (int) cell.find (SourceRows::kpszResultPrefix);
            placement.segments[1].commentAt = (int) cell.find (L';');
            placement.operand               = 1;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (3, layout.lineCount);
            Assert::AreEqual (std::wstring (L"; spin until the counter wraps"), TipMetrics::GetLineText (layout, 0), L"the comment on the row's line");
            Assert::AreEqual ((float) kTextLeft,                       TipMetrics::GetLine (layout, 0)[0].leftPx, L"in the operand's place");
            Assert::AreEqual (std::wstring (L"Z=0"),                   TipMetrics::GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"Result: PC=$60BA"),      TipMetrics::GetLineText (layout, 2));
            Assert::AreEqual ((float) kCodeLeft, TipMetrics::GetLine (layout, 1)[0].leftPx, L"the operand under the code");
            Assert::AreEqual ((float) kCodeLeft, TipMetrics::GetLine (layout, 2)[0].leftPx, L"the result under the code");

            //  Wider than the screen's room, the comment wraps under its
            //  first character, and the operand and result follow it.
            placement.workArea.right = GetEdgeFor (20) + TipMetrics::kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (std::wstring (L"; spin until the"), TipMetrics::GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"counter wraps"),    TipMetrics::GetLineText (layout, 1));
            Assert::AreEqual ((float) (kTextLeft + 2 * TipMetrics::kGlyphPx), TipMetrics::GetLine (layout, 1)[0].leftPx, L"under the comment's text");
            Assert::AreEqual (std::wstring (L"Z=0"),              TipMetrics::GetLineText (layout, 2));
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



            placement.segments.push_back (MakeSegment (L"hint_text", kRowLeft + TipMetrics::kPadPx, placement));
            placement.segments.push_back (MakeSegment (line, kTextLeft, placement));
            placement.segments[1].isWholeLine = true;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (1, layout.lineCount, L"the whole line on one line");
            Assert::AreEqual (line, TipMetrics::GetLineText (layout, 0), L"the line the view cuts off; its label is the view's");
            Assert::AreEqual ((LONG) (GetEdgeFor ((int) line.size()) + TipMetrics::kPadPx), layout.rect.right, L"run on to the end of the line");
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
            placement.workArea.right          = GetEdgeFor (kRoomChars) + TipMetrics::kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (3, layout.lineCount);
            Assert::AreEqual (std::wstring (L"lda #$41 ; set the accumulator to the"), TipMetrics::GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"letter A before the loop"),              TipMetrics::GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"starts"),                                TipMetrics::GetLineText (layout, 2));
            Assert::AreEqual ((float) (kTextLeft + kCommentChar * TipMetrics::kGlyphPx), TipMetrics::GetLine (layout, 1)[0].leftPx, L"under the comment's first character");
            Assert::AreEqual ((float) (kTextLeft + kCommentChar * TipMetrics::kGlyphPx), TipMetrics::GetLine (layout, 2)[0].leftPx);
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
            placement.workArea.right          = GetEdgeFor (kRoomChars) + TipMetrics::kPadPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::IsTrue   (layout.lineCount > 1, L"too wide for the screen's room");
            Assert::AreEqual (std::wstring (L"  dc.b \"ONE;TWO THREE FOUR"), TipMetrics::GetLineText (layout, 0), L"broken at a space");

            for (int line = 1; line < layout.lineCount; line++)
            {
                Assert::AreEqual ((float) (kTextLeft + (kCodeChar + OperandResultTip::kCodeIndentChars) * TipMetrics::kGlyphPx), TipMetrics::GetLine (layout, line)[0].leftPx);
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
            Assert::IsTrue   (TipMetrics::GetLine (layout, 0).empty());
            Assert::AreEqual (std::wstring (L"A=00 X=01"),    TipMetrics::GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"Result: A=41"), TipMetrics::GetLineText (layout, 2));
            Assert::AreEqual ((float) (kViewLeft + TipMetrics::kPadPx), TipMetrics::GetLine (layout, 1)[0].leftPx, L"from the view's text");
        }


        TEST_METHOD (AtTheScreensRightEdgeTheTipGrowsDown)
        {
            constexpr int                kRoomChars = 12;

            OperandResultTip::Placement  placement = MakeRow ("A=00 X=01 Y=02 $1234=41 C=1 Z=0", "A=41", GetEdgeFor (30));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  The view runs past the screen's edge.
            placement.workArea.right = GetEdgeFor (kRoomChars) + TipMetrics::kPadPx;

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
            placement.workArea.bottom = kRowTop + 2 * TipMetrics::kRowHeight;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (placement, text, layout));
            Assert::AreEqual (2, layout.lineCount);
            Assert::IsTrue   (layout.rect.bottom <= placement.workArea.bottom, L"the tip runs off the bottom of the screen");

            //  With room for a single row, nothing fits.
            placement.workArea.bottom = kRowTop + TipMetrics::kRowHeight;

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
                placement.workArea.right = GetEdgeFor (31) + TipMetrics::kPadPx;

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
            Assert::AreEqual ((float) TipMetrics::kRowHeight, text.Calls()[1].y - text.Calls()[0].y, L"the result a row below");
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
