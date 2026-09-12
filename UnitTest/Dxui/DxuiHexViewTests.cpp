#include "Pch.h"

#include "Widgets/DxuiHexView.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CountingHexSource
//
//  A run of bytes whose value is its own offset, counting every byte the view
//  asks for so a test can prove the view reads only what it draws.
//
////////////////////////////////////////////////////////////////////////////////

class CountingHexSource : public IDxuiHexSource
{
public:
    explicit CountingHexSource (uint64_t count) : m_count (count) {}

    uint64_t  GetByteCount() const override { return m_count; }

    void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const override
    {
        for (size_t idx = 0; idx < out.size(); idx++)
        {
            out[idx] = (uint8_t) ((offset + idx) & 0xFF);
        }

        m_bytesRead += out.size();
    }

    uint64_t  GetBytesRead() const { return m_bytesRead; }

private:
    uint64_t          m_count     = 0;
    mutable uint64_t  m_bytesRead = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewTests
//
//  Which byte a point selects, where each byte is drawn in either column, what
//  a regrouping does to a selection, and where the keys take the caret. The
//  cell is 8 by 16 DIPs and the view is 800 by 320, so a row is 20 cells tall
//  and the column arithmetic reads directly.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiHexViewTests)
{
public:

    static constexpr int  kCellW = 8;
    static constexpr int  kCellH = 16;

    //  Cells the offset column and the gutter spend before the hex column,
    //  for a source whose last address fits in four digits.
    static constexpr int  kHexStart = 4 + DxuiHexView::kGutterCells;


    static void  LayOut (DxuiHexView & view)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        view.SetCellSizeDip (kCellW, kCellH);
        view.Layout (RECT { 0, 0, 800, 320 }, scaler);
    }


    static DxuiMouseEvent  MakeMouse (DxuiMouseEventKind kind, int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };
        return ev;
    }


    static DxuiKeyEvent  MakeKey (WPARAM vk, bool shift = false, bool ctrl = false)
    {
        DxuiKeyEvent  ev;

        ev.kind  = DxuiKeyEventKind::Down;
        ev.vk    = vk;
        ev.shift = shift;
        ev.ctrl  = ctrl;
        return ev;
    }


    TEST_METHOD (RowCount_CountsAPartialLastRow)
    {
        CountingHexSource  source (33);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);

        Assert::AreEqual (uint64_t (3), view.GetRowCount(),
            L"Thirty-three bytes are two full rows and one byte");
        Assert::AreEqual (20, view.GetRowCap(),
            L"320 DIPs of height at 16 DIPs a row shows twenty rows");
    }


    TEST_METHOD (ByteRect_HexColumnSpacesEveryByteAtGroupingOne)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;
        RECT               first  = {};
        RECT               sixth  = {};


        view.SetSource (&source);
        LayOut (view);

        first = view.GetByteRect (0, DxuiHexView::Column::Hex);
        sixth = view.GetByteRect (5, DxuiHexView::Column::Hex);

        Assert::AreEqual ((LONG) (kHexStart * kCellW), first.left,
            L"The first byte follows the offset column and its gutter");
        Assert::AreEqual ((LONG) (2 * kCellW), first.right - first.left,
            L"A byte is two hex digits wide");
        Assert::AreEqual ((LONG) ((kHexStart + 15) * kCellW), sixth.left,
            L"At a grouping of one every byte costs three cells");
    }


    TEST_METHOD (ByteRect_GroupingOfFourClosesTheGapsInsideAGroup)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;
        RECT               sixth = {};


        view.SetSource (&source);
        LayOut (view);

        Assert::IsTrue (view.SetGrouping (4),
            L"Four divides a sixteen-byte row");

        sixth = view.GetByteRect (5, DxuiHexView::Column::Hex);

        Assert::AreEqual ((LONG) ((kHexStart + 11) * kCellW), sixth.left,
            L"Byte five is ten digits and one group space from the left");
    }


    TEST_METHOD (SetGrouping_RefusesWhatTheRowDoesNotDivide)
    {
        DxuiHexView  view;


        view.SetBytesPerRow (12);

        Assert::IsFalse (view.SetGrouping (8),
            L"Eight would leave a half group at the end of a twelve-byte row");
        Assert::AreEqual (1, view.GetGrouping(),
            L"A refused grouping leaves the one in force");
    }


    TEST_METHOD (ByteRect_TextColumnIsOneCellABytePastTheHexColumn)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;
        RECT               rect  = {};
        int                start = kHexStart + (16 * 2) + 15 + DxuiHexView::kGutterCells;


        view.SetSource (&source);
        LayOut (view);

        rect = view.GetByteRect (3, DxuiHexView::Column::Text);

        Assert::AreEqual ((LONG) ((start + 3) * kCellW), rect.left,
            L"The text column is one cell a byte");
        Assert::AreEqual ((LONG) kCellW, rect.right - rect.left,
            L"A character is one cell wide");
    }


    TEST_METHOD (HitTest_TheSpaceAfterAByteBelongsToThatByte)
    {
        CountingHexSource        source (256);
        DxuiHexView              view;
        DxuiHexView::HitResult   onDigit;
        DxuiHexView::HitResult   onSpace;


        view.SetSource (&source);
        LayOut (view);

        onDigit = view.HitTestPoint (POINT { kHexStart * kCellW, 0 });
        onSpace = view.HitTestPoint (POINT { (kHexStart + 2) * kCellW, 0 });

        Assert::IsTrue (onDigit.hit && onSpace.hit,
            L"Both points are inside the hex column");
        Assert::AreEqual (uint64_t (0), onDigit.offset,
            L"The first two cells are byte zero");
        Assert::AreEqual (uint64_t (0), onSpace.offset,
            L"The space after byte zero is still byte zero, so a drag never stalls");
    }


    TEST_METHOD (HitTest_TextColumnReportsItsOwnColumn)
    {
        CountingHexSource       source (256);
        DxuiHexView             view;
        DxuiHexView::HitResult  hit;
        int                     start = kHexStart + (16 * 2) + 15 + DxuiHexView::kGutterCells;


        view.SetSource (&source);
        LayOut (view);

        hit = view.HitTestPoint (POINT { (start + 7) * kCellW, kCellH });

        Assert::IsTrue (hit.hit, L"The point is inside the text column");
        Assert::AreEqual (uint64_t (16 + 7), hit.offset,
            L"The second row's eighth character is byte twenty-three");
        Assert::IsTrue (hit.column == DxuiHexView::Column::Text,
            L"A point in the text column reports the text column");
    }


    TEST_METHOD (HitTest_PastTheLastByteOfAShortRowLandsOnTheLastByte)
    {
        CountingHexSource       source (20);
        DxuiHexView             view;
        DxuiHexView::HitResult  hit;


        view.SetSource (&source);
        LayOut (view);

        hit = view.HitTestPoint (POINT { (kHexStart + 30) * kCellW, kCellH });

        Assert::IsTrue (hit.hit, L"The row is there even where its bytes are not");
        Assert::AreEqual (uint64_t (19), hit.offset,
            L"Dragging past the end of the file stops on its last byte");
    }


    TEST_METHOD (Selection_LightsTheSameRunInBothColumns)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);

        view.SelectByte (4, DxuiHexView::Column::Hex);
        view.ExtendSelectionTo (20);

        Assert::AreEqual (uint64_t (17), view.GetSelectionCount(),
            L"Four through twenty inclusive is seventeen bytes");
        Assert::IsTrue (view.IsByteSelected (16),
            L"A run crossing a row boundary keeps going on the next row");
        Assert::IsFalse (view.IsByteSelected (21),
            L"The byte past the moving end is not in the run");

        Assert::IsTrue (view.GetByteRect (16, DxuiHexView::Column::Hex).right > 0,
            L"The run has a place in the hex column");
        Assert::IsTrue (view.GetByteRect (16, DxuiHexView::Column::Text).right > 0,
            L"and the same bytes have a place in the text column");
    }


    TEST_METHOD (Selection_SurvivesARegrouping)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;
        RECT               before = {};
        RECT               after  = {};


        view.SetSource (&source);
        LayOut (view);

        view.SelectByte (4, DxuiHexView::Column::Hex);
        view.ExtendSelectionTo (9);

        before = view.GetByteRect (9, DxuiHexView::Column::Hex);

        Assert::IsTrue (view.SetGrouping (8), L"Eight divides a sixteen-byte row");

        after = view.GetByteRect (9, DxuiHexView::Column::Hex);

        Assert::AreEqual (uint64_t (6), view.GetSelectionCount(),
            L"Regrouping moves the digits, not the selection");
        Assert::IsTrue (after.left < before.left,
            L"A wider group spends fewer spaces, so the byte moves left");
    }


    TEST_METHOD (Selection_ExtendingBackwardsKeepsTheAnchor)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);

        view.SelectByte (10, DxuiHexView::Column::Hex);
        view.ExtendSelectionTo (2);

        Assert::AreEqual (uint64_t (2),  view.GetSelectionFirst(), L"The run runs from where it ends");
        Assert::AreEqual (uint64_t (10), view.GetSelectionLast(),  L"to where it began");
        Assert::AreEqual (uint64_t (10), view.GetSelectionAnchor(),
            L"The anchor stays where the run was started");
    }


    TEST_METHOD (Mouse_DragSelectsFromPressToRelease)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;
        int                textStart = kHexStart + (16 * 2) + 15 + DxuiHexView::kGutterCells;


        view.SetSource (&source);
        LayOut (view);

        Assert::IsTrue (view.OnMouse (MakeMouse (DxuiMouseEventKind::Down, kHexStart * kCellW, 0)),
            L"A press in the hex column is the view's");
        Assert::IsTrue (view.IsDragging(), L"and starts a drag");

        view.OnMouse (MakeMouse (DxuiMouseEventKind::Move, (textStart + 5) * kCellW, kCellH));
        view.OnMouse (MakeMouse (DxuiMouseEventKind::Up, (textStart + 5) * kCellW, kCellH));

        Assert::IsFalse (view.IsDragging(), L"Releasing ends the drag");
        Assert::AreEqual (uint64_t (0),  view.GetSelectionFirst(), L"The run starts where the press landed");
        Assert::AreEqual (uint64_t (21), view.GetSelectionLast(),
            L"and ends on the byte under the release, even in the other column");
        Assert::IsTrue (view.GetSelectionColumn() == DxuiHexView::Column::Hex,
            L"The column is the one the run was started in");
    }


    TEST_METHOD (Mouse_ShiftClickExtendsInsteadOfStartingOver)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;
        DxuiMouseEvent     shiftClick = MakeMouse (DxuiMouseEventKind::Down, (kHexStart + 9) * kCellW, 0);


        view.SetSource (&source);
        LayOut (view);

        view.OnMouse (MakeMouse (DxuiMouseEventKind::Down, kHexStart * kCellW, 0));
        view.OnMouse (MakeMouse (DxuiMouseEventKind::Up, kHexStart * kCellW, 0));

        shiftClick.shift = true;
        view.OnMouse (shiftClick);

        Assert::AreEqual (uint64_t (4), view.GetSelectionCount(),
            L"Shift with a click takes the run out to the byte clicked");
    }


    TEST_METHOD (Keys_ArrowsWalkAByteAndARow)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);
        view.SelectByte (0, DxuiHexView::Column::Hex);

        view.OnKey (MakeKey (VK_RIGHT));
        Assert::AreEqual (uint64_t (1), view.GetCaret(), L"Right is one byte on");

        view.OnKey (MakeKey (VK_DOWN));
        Assert::AreEqual (uint64_t (17), view.GetCaret(), L"Down is a row on");

        view.OnKey (MakeKey (VK_UP));
        Assert::AreEqual (uint64_t (1), view.GetCaret(), L"and up is a row back");

        Assert::AreEqual (uint64_t (1), view.GetSelectionCount(),
            L"Walking without Shift carries a one-byte selection along");
    }


    TEST_METHOD (Keys_ShiftEndTakesTheRunToTheEndOfTheRow)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);
        view.SelectByte (20, DxuiHexView::Column::Hex);

        view.OnKey (MakeKey (VK_END, true));

        Assert::AreEqual (uint64_t (20), view.GetSelectionFirst(), L"The run starts where the caret was");
        Assert::AreEqual (uint64_t (31), view.GetSelectionLast(),  L"and reaches the last byte of that row");

        view.OnKey (MakeKey (VK_END, true, true));

        Assert::AreEqual (uint64_t (255), view.GetSelectionLast(),
            L"Ctrl with End reaches the last byte of the whole source");
    }


    TEST_METHOD (Keys_CtrlATakesEverything)
    {
        CountingHexSource  source (100);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);

        view.OnKey (MakeKey ('A', false, true));

        Assert::AreEqual (uint64_t (100), view.GetSelectionCount(),
            L"Ctrl+A is the whole source");
    }


    TEST_METHOD (Keys_ArrowsStopAtBothEnds)
    {
        CountingHexSource  source (40);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);

        view.SelectByte (0, DxuiHexView::Column::Hex);
        view.OnKey (MakeKey (VK_LEFT));
        Assert::AreEqual (uint64_t (0), view.GetCaret(), L"The caret does not walk off the front");

        view.SelectByte (39, DxuiHexView::Column::Hex);
        view.OnKey (MakeKey (VK_RIGHT));
        Assert::AreEqual (uint64_t (39), view.GetCaret(), L"nor off the end");

        view.OnKey (MakeKey (VK_DOWN));
        Assert::AreEqual (uint64_t (39), view.GetCaret(),
            L"and down from the last row stays put rather than leaving the source");
    }


    TEST_METHOD (Scrolling_EnsureByteVisibleMovesTheLeastItCan)
    {
        CountingHexSource  source (16 * 100);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);

        Assert::AreEqual (uint64_t (80), view.GetMaxTopRow(),
            L"A hundred rows in a twenty-row view leaves eighty to scroll");

        view.EnsureByteVisible (20 * 16);

        Assert::AreEqual (uint64_t (1), view.GetTopRow(),
            L"The row below the last visible one costs one row of scrolling");

        view.EnsureByteVisible (0);

        Assert::AreEqual (uint64_t (0), view.GetTopRow(),
            L"and going back to the front scrolls back to it");
    }


    TEST_METHOD (Scrolling_WheelMovesThreeRowsAndStopsAtTheEnd)
    {
        CountingHexSource  source (16 * 100);
        DxuiHexView        view;
        DxuiMouseEvent     wheel = MakeMouse (DxuiMouseEventKind::Wheel, 0, 0);


        view.SetSource (&source);
        LayOut (view);

        wheel.wheelDelta = -1.0f;
        view.OnMouse (wheel);

        Assert::AreEqual (uint64_t (3), view.GetTopRow(),
            L"A notch down is three rows down");

        wheel.wheelDelta = -100.0f;
        view.OnMouse (wheel);

        Assert::AreEqual (view.GetMaxTopRow(), view.GetTopRow(),
            L"and the wheel stops where the last row does");
    }


    TEST_METHOD (Offsets_EightDigitsOnceTheLastAddressOutgrowsFour)
    {
        CountingHexSource  shortSource (256);
        CountingHexSource  longSource (0x20000);
        DxuiHexView        view;
        RECT               narrow = {};
        RECT               wide   = {};


        view.SetSource (&shortSource);
        LayOut (view);
        narrow = view.GetRowOffsetRect (0);

        view.SetSource (&longSource);
        LayOut (view);
        wide = view.GetRowOffsetRect (0);

        Assert::AreEqual ((LONG) (4 * kCellW), narrow.right - narrow.left,
            L"A 256-byte file labels its rows in four digits");
        Assert::AreEqual ((LONG) (8 * kCellW), wide.right - wide.left,
            L"and a source past sixteen bits labels them in eight");
    }


    TEST_METHOD (Origin_ShiftsWhatTheOffsetColumnHasToHold)
    {
        CountingHexSource  source (0x100);
        DxuiHexView        view;
        RECT               rect = {};


        view.SetSource (&source);
        view.SetOriginAddress (0xFFF0);
        LayOut (view);

        rect = view.GetRowOffsetRect (0);

        Assert::AreEqual ((LONG) (8 * kCellW), rect.right - rect.left,
            L"256 bytes from $FFF0 run past sixteen bits, so the column widens");
    }


    TEST_METHOD (Source_ChangingItDropsTheOldSelection)
    {
        CountingHexSource  first  (256);
        CountingHexSource  second (16);
        DxuiHexView        view;


        view.SetSource (&first);
        LayOut (view);
        view.SelectByte (200, DxuiHexView::Column::Hex);

        view.SetSource (&second);

        Assert::IsFalse (view.HasSelection(),
            L"A selection of bytes that are gone is not a selection");
        Assert::AreEqual (uint64_t (0), view.GetTopRow(),
            L"and the view is back at the top of the new bytes");
    }


    TEST_METHOD (Paint_ReadsOnlyTheRowsItDraws)
    {
        CountingHexSource     source (64 * 1024);
        DxuiHexView           view;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;


        view.SetSource (&source);
        LayOut (view);
        view.Paint (painter, text, theme);

        Assert::AreEqual (uint64_t (20 * 16), source.GetBytesRead(),
            L"A screenful is twenty rows of sixteen bytes, whatever the source holds");

        view.SetTopRow (1000);
        view.Paint (painter, text, theme);

        Assert::AreEqual (uint64_t (2 * 20 * 16), source.GetBytesRead(),
            L"and scrolling deep into 64 KB costs another screenful, not a scan");
    }


    TEST_METHOD (Paint_LightsTheSelectionInBothColumns)
    {
        CountingHexSource     source (256);
        DxuiHexView           view;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int                   fills = 0;


        view.SetSource (&source);
        LayOut (view);
        view.SelectByte (3, DxuiHexView::Column::Hex);
        view.ExtendSelectionTo (5);
        view.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if ((call.kind == RecordedTextKind::FillRect) && (call.argb == theme.SelectionBackground()))
            {
                fills++;
            }
        }

        Assert::AreEqual (6, fills,
            L"Three selected bytes are lit twice each, once under each column");
    }


    TEST_METHOD (Text_AppleHighBitDecodesBothHalvesTheSameWay)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);

        view.SetTextEncoding (DxuiHexView::TextEncoding::Ascii);
        Assert::AreEqual (std::wstring (L"AB"), view.GetTextFor (0x41, 2),
            L"Plain text reads as itself");
        Assert::AreEqual (std::wstring (L".."), view.GetTextFor (0xC1, 2),
            L"and a byte with the high bit set is not a character it can show");

        view.SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
        Assert::AreEqual (std::wstring (L"AB"), view.GetTextFor (0xC1, 2),
            L"Apple text is the same letters with the high bit set");
        Assert::AreEqual (std::wstring (L"AB"), view.GetTextFor (0x41, 2),
            L"and the inverse half of the set decodes to the same letters");
    }


    TEST_METHOD (Hex_CopiesTheDigitsSpacedByTheGroupingInForce)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;


        view.SetSource (&source);
        LayOut (view);

        Assert::AreEqual (std::wstring (L"10 11 12 13"), view.GetHexFor (0x10, 4),
            L"At a grouping of one every byte is spaced");

        Assert::IsTrue (view.SetGrouping (2), L"Two divides a sixteen-byte row");

        Assert::AreEqual (std::wstring (L"1011 1213"), view.GetHexFor (0x10, 4),
            L"and at two the digits pair up");
    }


    TEST_METHOD (Selection_ReportsEveryChange)
    {
        CountingHexSource  source (256);
        DxuiHexView        view;
        int                changes = 0;


        view.SetSource (&source);
        LayOut (view);
        view.SetOnSelectionChanged ([&changes] () { changes++; });

        view.SelectByte (1, DxuiHexView::Column::Hex);
        view.ExtendSelectionTo (5);
        view.ClearSelection();
        view.ClearSelection();

        Assert::AreEqual (3, changes,
            L"Start, extend and loss are each reported, and clearing nothing is not a change");
    }
};
