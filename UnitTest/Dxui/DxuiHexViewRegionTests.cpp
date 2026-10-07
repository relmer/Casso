#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RegionHexSource
//
//  Bytes in runs of regions, each run given as its first offset, its end and
//  its region, with a color and a label for every region.
//
////////////////////////////////////////////////////////////////////////////////

class RegionHexSource : public IDxuiHexSource
{
public:
    struct Run
    {
        uint64_t  first  = 0;
        uint64_t  end    = 0;
        uint16_t  region = 0;
    };

    RegionHexSource (uint64_t count, std::vector<Run> runs) : m_count (count), m_runs (std::move (runs)) {}

    uint64_t  GetByteCount() const override { return m_count; }

    void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const override
    {
        for (size_t idx = 0; idx < out.size(); idx++)
        {
            out[idx] = (uint8_t) ((offset + idx) & 0xFF);
        }
    }

    void  ReadRegions (uint64_t offset, std::span<uint16_t> out) const override
    {
        for (size_t idx = 0; idx < out.size(); idx++)
        {
            out[idx] = 0;

            for (const Run & run : m_runs)
            {
                if (offset + idx >= run.first && offset + idx < run.end)
                {
                    out[idx] = run.region;
                }
            }
        }
    }

    bool  TryGetRegionStyle (uint16_t region, uint32_t & outArgb, std::wstring & outLabel) const override
    {
        outArgb  = kRegionArgb;
        outLabel = (region == kLongRegion) ? L"Slot 6 ROM" : L"ROM";
        return region != 0;
    }

    static constexpr uint32_t  kRegionArgb = 0xFF2080F0;
    static constexpr uint16_t  kLongRegion = 7;

private:
    uint64_t          m_count = 0;
    std::vector<Run>  m_runs;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewRegionTests
//
//  The outlines around a source's regions: off unless asked for, space opened
//  only between rows an edge runs between, a label's lane where the label
//  sits, the rows and the hit tests moved down past each lane, and the label
//  moved to the next row when the first is too short for it. Cells are 8 by
//  16 at 96 DPI, sixteen bytes a row, as in DxuiHexViewTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiHexViewRegionTests)
{
public:

    static constexpr int  kCellW    = 8;
    static constexpr int  kCellH    = 16;
    static constexpr int  kEdgeLane = DxuiHexView::kEdgeLaneDip;


    static void  LayOut (DxuiHexView & view)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        view.SetCellSizeDip (kCellW, kCellH);
        view.Layout (RECT { 0, 0, 800, 320 }, scaler);
    }


    TEST_METHOD (OutlinesAreOffUnlessTheHostTurnsThemOn)
    {
        RegionHexSource       source (256, { { 32, 64, 1 } });
        DxuiHexView           view;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;



        view.SetSource (&source);
        LayOut (view);

        Assert::IsFalse   (view.IsShowingRegions());
        Assert::AreEqual  (0,          view.GetLaneAbove (2));
        Assert::AreEqual  (2 * kCellH, view.GetRowTop (2));

        view.Paint (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            Assert::IsTrue (call.kind != RecordedPaintKind::DrawLine, L"an outline was drawn with outlines off");
        }
    }


    TEST_METHOD (ALabelLaneOpensAboveARegionAndAnEdgeLaneBelowIt)
    {
        RegionHexSource  source (256, { { 32, 64, 1 } });
        DxuiHexView      view;



        view.SetSource      (&source);
        view.SetShowRegions (true);
        LayOut (view);

        Assert::AreEqual (0,         view.GetLaneAbove (1), L"space opened where no edge runs");
        Assert::IsTrue   (view.GetLaneAbove (2) > kEdgeLane, L"no room for the label above the region");
        Assert::AreEqual (0,         view.GetLaneAbove (3), L"space opened inside the region");
        Assert::AreEqual (kEdgeLane, view.GetLaneAbove (4), L"no edge lane below the region");
    }


    TEST_METHOD (RowsMoveDownPastEachLane)
    {
        RegionHexSource  source (256, { { 32, 64, 1 } });
        DxuiHexView      view;
        int              label = 0;



        view.SetSource      (&source);
        view.SetShowRegions (true);
        LayOut (view);

        label = view.GetLaneAbove (2);

        Assert::AreEqual (kCellH,                             view.GetRowTop (1));
        Assert::AreEqual ((2 * kCellH) + label,               view.GetRowTop (2));
        Assert::AreEqual ((4 * kCellH) + label + kEdgeLane,   view.GetRowTop (4));
        Assert::AreEqual ((LONG) view.GetRowTop (2),          view.GetByteRect (32, DxuiHexView::Column::Hex).top);
    }


    TEST_METHOD (APointInALaneLandsOnTheRowBelow)
    {
        RegionHexSource         source (256, { { 32, 64, 1 } });
        DxuiHexView             view;
        DxuiHexView::HitResult  hit;
        RECT                    byte = {};



        view.SetSource      (&source);
        view.SetShowRegions (true);
        LayOut (view);

        byte = view.GetByteRect (32, DxuiHexView::Column::Hex);
        hit  = view.HitTestPoint (POINT { byte.left + 1, byte.top + 1 });

        Assert::IsTrue   (hit.hit);
        Assert::AreEqual ((uint64_t) 32, hit.offset);

        hit = view.HitTestPoint (POINT { byte.left + 1, byte.top - 2 });

        Assert::IsTrue   (hit.hit);
        Assert::AreEqual ((uint64_t) 32, hit.offset, L"the label's lane went to the row above");

        hit = view.HitTestPoint (POINT { byte.left + 1, byte.top + (2 * kCellH) + kEdgeLane + 1 });

        Assert::AreEqual ((uint64_t) 64, hit.offset, L"the row below the region's edge lane was missed");
    }


    TEST_METHOD (PaintingDrawsTheOutlineAndItsLabel)
    {
        RegionHexSource       source (256, { { 32, 64, 1 } });
        DxuiHexView           view;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int                   lines  = 0;
        bool                  label  = false;
        RECT                  first  = {};



        view.SetSource      (&source);
        view.SetShowRegions (true);
        LayOut (view);
        view.Paint (painter, text, theme);

        first = view.GetByteRect (32, DxuiHexView::Column::Hex);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            lines += (call.kind == RecordedPaintKind::DrawLine) ? 1 : 0;
        }

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == L"ROM")
            {
                label = true;

                Assert::AreEqual (RegionHexSource::kRegionArgb, call.argb);
                Assert::IsTrue   (call.y + call.height <= (float) first.top, L"the label reaches into the region's first row");
                Assert::IsTrue   (call.y >= (float) (first.top - view.GetLaneAbove (2)), L"the label reaches into the row above");
            }
        }

        Assert::IsTrue (lines > 0, L"no outline was drawn");
        Assert::IsTrue (label,     L"no label was drawn");
    }


    TEST_METHOD (ARegionStartingPartwayAlongARowStepsItsOutline)
    {
        RegionHexSource  source (256, { { 40, 80, 1 } });
        DxuiHexView      view;



        view.SetSource      (&source);
        view.SetShowRegions (true);
        LayOut (view);

        Assert::IsTrue   (view.GetLaneAbove (2) > kEdgeLane, L"no label lane over the partial first row");
        Assert::AreEqual (kEdgeLane, view.GetLaneAbove (3), L"no lane for the step under the first row's start");
        Assert::AreEqual (kEdgeLane, view.GetLaneAbove (5), L"no lane below the region");
    }


    TEST_METHOD (ALabelTooLongForTheFirstRowMovesToTheNext)
    {
        RegionHexSource                  source (256, { { 46, 128, RegionHexSource::kLongRegion } });
        DxuiHexView                      view;
        DxuiHexView::LabelSegment        segment;



        view.SetSource      (&source);
        view.SetShowRegions (true);
        LayOut (view);

        Assert::IsTrue   (view.TryGetLabelSegment (2, 14, segment));
        Assert::AreEqual ((uint64_t) 3, segment.row, L"the label stayed on the two values of the first row");
        Assert::AreEqual (0,  segment.first);
        Assert::AreEqual (13, segment.last);
        Assert::AreEqual (kEdgeLane, view.GetLaneAbove (2), L"the first row's top kept a label lane");
        Assert::IsTrue   (view.GetLaneAbove (3) > kEdgeLane, L"no label lane where the label went");
    }


    //  An outline's edge gets a gutter of its own: the offsets keep the usual
    //  gap to the box, rather than the box's edge taking the middle of it.
    TEST_METHOD (OutlinesWidenTheGuttersRatherThanTakeThem)
    {
        RegionHexSource  source (256, { { 32, 64, 1 } });
        DxuiHexView      plain;
        DxuiHexView      boxed;
        RECT             plainHex  = {};
        RECT             boxedHex  = {};
        RECT             boxedText = {};
        RECT             plainText = {};



        plain.SetSource      (&source);
        boxed.SetSource      (&source);
        boxed.SetShowRegions (true);
        LayOut (plain);
        LayOut (boxed);

        plainHex  = plain.GetByteRect (0, DxuiHexView::Column::Hex);
        boxedHex  = boxed.GetByteRect (0, DxuiHexView::Column::Hex);
        plainText = plain.GetByteRect (0, DxuiHexView::Column::Text);
        boxedText = boxed.GetByteRect (0, DxuiHexView::Column::Text);

        Assert::AreEqual ((LONG) (DxuiHexView::kGutterCells * kCellW),       plainHex.left - plain.GetRowOffsetRect (0).right);
        Assert::AreEqual ((LONG) ((DxuiHexView::kGutterCells + 1) * kCellW), boxedHex.left - boxed.GetRowOffsetRect (0).right,
                          L"a box's edge took the middle of the gap");
        Assert::AreEqual ((LONG) kCellW, (boxedText.left - boxedHex.left) - (plainText.left - plainHex.left),
                          L"and the text column's gutter grows too");
    }


    TEST_METHOD (TheLanesLeaveRoomForFewerRows)
    {
        RegionHexSource  plain (4096, {});
        RegionHexSource  boxed (4096, { { 0, 16, 1 }, { 32, 48, 1 }, { 64, 80, 1 } });
        DxuiHexView      without;
        DxuiHexView      with;



        without.SetSource (&plain);
        with.SetSource    (&boxed);
        with.SetShowRegions (true);
        LayOut (without);
        LayOut (with);

        Assert::AreEqual (320 / kCellH, without.GetRowCap());
        Assert::IsTrue   (with.GetRowCap() < without.GetRowCap(), L"the lanes took no rows from the page");
    }
};
