#include "Pch.h"

#include "HResultAssert.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/Reverse/HistoryMarks.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/IHistoryFrameRenderer.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/KeyframeStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kMarkStateBytes = 256;
static constexpr uint64_t  s_kMarkInterval   = 1000;
static constexpr uint64_t  s_kMarkStride     = 100;
static constexpr SIZE      s_kMarkCell       = { 4, 2 };





////////////////////////////////////////////////////////////////////////////////
//
//  BlankFrameRenderer
//
//  Draws every snapshot as one black pixel; the mark tests draw nothing.
//
////////////////////////////////////////////////////////////////////////////////

class BlankFrameRenderer : public IHistoryFrameRenderer
{
public:
    HRESULT Render (const std::vector<Byte>  & state,
                    std::vector<uint32_t>    & outBgra,
                    int                      & outWidth,
                    int                      & outHeight) override
    {
        (void) state;

        outWidth  = 1;
        outHeight = 1;
        outBgra.assign (1, 0xFF000000u);
        return S_OK;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  MarkRig
//
//  Keyframes every s_kMarkStride positions, s_kMarkInterval cycles apart,
//  and a journal stamped with a position the test moves.
//
////////////////////////////////////////////////////////////////////////////////

class MarkRig
{
public:
    KeyframeStore             store;
    InputJournal              journal;
    HistoryMarks              marks;
    std::vector<HistoryMark>  list;
    uint64_t                  position = 0;



    MarkRig()
    {
        journal.SetOn             (true);
        journal.SetPositionSource (&position);
    }


    void Fill (size_t count, size_t first = 0)
    {
        KeyframeSettings   settings;
        std::vector<Byte>  state (s_kMarkStateBytes);
        HRESULT            hr = S_OK;



        settings.intervalCycles = s_kMarkInterval;
        store.Configure (settings);

        for (size_t i = first; i < first + count; i++)
        {
            hr = store.Add (i * s_kMarkStride, i * s_kMarkInterval, state);
            AssertSucceeded (hr, L"Add");
        }
    }


    void RecordAt (uint64_t at, InputKind kind)
    {
        position = at;
        journal.Record (0, kind, 0, 0, std::string_view());
    }


    bool Update ()
    {
        uint64_t  endPosition = (store.GetCount() > 0) ? store.GetInfo (store.GetCount() - 1).position : 0;
        uint64_t  endCycle    = (store.GetCount() > 0) ? store.GetInfo (store.GetCount() - 1).cycle    : 0;



        return marks.Update (store, journal, endPosition, endCycle, list);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryMarkTests
//
//  The marks on the timeline: one for each reset, power cycle, disk change,
//  write-protect change and state load in the journal, and one for each gap
//  in recording, each at the cycle its position falls at between the
//  keyframes; the journal read once, again after a cut; and the strip's
//  glyph, offset and tip for each.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryMarkTests)
{
public:

    TEST_METHOD (MarkingRecordsMakeMarksAtTheirCycles)
    {
        MarkRig  rig;



        rig.Fill (10);

        rig.RecordAt (120, InputKind::KeyPress);
        rig.RecordAt (150, InputKind::Reset);
        rig.RecordAt (420, InputKind::DiskEject);
        rig.RecordAt (420, InputKind::DiskMount);
        rig.RecordAt (700, InputKind::PowerCycle);
        rig.RecordAt (810, InputKind::StateLoad);

        Assert::IsTrue (rig.Update(), L"the first update lists the marks");

        Assert::AreEqual<size_t>   (4,    rig.list.size(),      L"a keystroke makes none, and a swap makes one");
        Assert::IsTrue             (HistoryMarkKind::Reset      == rig.list[0].kind, L"the reset first");
        Assert::AreEqual<uint64_t> (1500, rig.list[0].cycle,    L"halfway between the keyframes at 100 and 200");
        Assert::IsTrue             (HistoryMarkKind::DiskChange == rig.list[1].kind, L"then the swap");
        Assert::AreEqual<uint64_t> (4200, rig.list[1].cycle,    L"a fifth past the keyframe at 400");
        Assert::IsTrue             (HistoryMarkKind::PowerCycle == rig.list[2].kind, L"then the power cycle");
        Assert::AreEqual<uint64_t> (7000, rig.list[2].cycle,    L"on a keyframe exactly");
        Assert::IsTrue             (HistoryMarkKind::StateLoad  == rig.list[3].kind, L"then the state load");
    }


    TEST_METHOD (AGapMarksTheKeyframeAfterIt)
    {
        MarkRig  rig;



        rig.Fill (5);
        rig.store.MarkNewestGap (350);

        Assert::IsTrue (rig.Update(), L"the gap is listed");

        Assert::AreEqual<size_t>   (1,    rig.list.size(),   L"one mark");
        Assert::IsTrue             (HistoryMarkKind::Gap == rig.list[0].kind, L"a gap");
        Assert::AreEqual<uint64_t> (400,  rig.list[0].position, L"at the keyframe after it");
        Assert::AreEqual<uint64_t> (4000, rig.list[0].cycle,    L"at that keyframe's cycle");
    }


    TEST_METHOD (AnUpdateWithNothingNewReportsNoChange)
    {
        MarkRig  rig;



        rig.Fill (10);
        rig.RecordAt (150, InputKind::Reset);

        Assert::IsTrue  (rig.Update(), L"the first update");
        Assert::IsFalse (rig.Update(), L"nothing new");

        rig.RecordAt (160, InputKind::KeyPress);

        Assert::IsFalse (rig.Update(), L"a keystroke changes nothing");

        rig.RecordAt (170, InputKind::Reset);

        Assert::IsTrue           (rig.Update(), L"a second reset is new");
        Assert::AreEqual<size_t> (2, rig.list.size(), L"both resets");
    }


    TEST_METHOD (ACutAtTheEndReadsTheJournalAgain)
    {
        MarkRig  rig;
        size_t   cutAt = 0;



        rig.Fill (10);
        rig.RecordAt (150, InputKind::Reset);

        cutAt = rig.journal.GetEndIndex();

        rig.RecordAt (650, InputKind::PowerCycle);

        Assert::IsTrue           (rig.Update(), L"the first update");
        Assert::AreEqual<size_t> (2, rig.list.size(), L"both marks");

        rig.journal.Truncate (cutAt);
        rig.RecordAt (660, InputKind::Reset);

        Assert::IsTrue           (rig.Update(), L"the cut is seen");
        Assert::AreEqual<size_t> (2, rig.list.size(), L"the reset before the cut and the one after it");
        Assert::IsTrue           (HistoryMarkKind::Reset == rig.list[1].kind, L"the power cycle cut away");
        Assert::AreEqual<uint64_t> (660, rig.list[1].position, L"the reset made after the cut");
    }


    TEST_METHOD (MarksBeforeTheOldestKeyframeAreLeftOut)
    {
        MarkRig  rig;



        rig.Fill (9, 1);

        rig.RecordAt (50,  InputKind::Reset);
        rig.RecordAt (250, InputKind::Reset);

        Assert::IsTrue             (rig.Update(), L"the update lists the marks");
        Assert::AreEqual<size_t>   (1,   rig.list.size(),      L"only the reset inside history");
        Assert::AreEqual<uint64_t> (250, rig.list[0].position, L"the one at 250");
    }


    TEST_METHOD (TheStripGetsEachMarksGlyphOffsetAndTip)
    {
        MarkRig                             rig;
        BlankFrameRenderer                  renderer;
        HistoryThumbnails                   thumbnails (renderer);
        std::vector<DxuiStripMark>          strip;
        std::vector<HistoryThumbnailCell>   cells;
        HRESULT                             hr = S_OK;



        rig.Fill (10);
        rig.RecordAt (300, InputKind::Reset);

        thumbnails.SetCellLayout   (4, s_kMarkCell);
        thumbnails.SetPlayheadTime (9000, 0, 9000);
        thumbnails.SetLabeler      ([] (uint64_t cycle, uint64_t wallTime, std::wstring & outTop, std::wstring & outBottom)
        {
            (void) wallTime;

            outTop    = std::to_wstring (cycle);
            outBottom.clear();
        });

        hr = thumbnails.Service (rig.store);
        AssertSucceeded (hr, L"Service");

        Assert::IsTrue (rig.Update(), L"the marks");

        thumbnails.SetMarks (rig.list);
        thumbnails.GetMarks (strip);
        thumbnails.GetCells (cells);

        Assert::AreEqual<size_t> (1, strip.size(), L"one mark on the strip");
        Assert::AreEqual (HistoryThumbnails::GetPlayheadOffset (cells, 3000, 9000), strip[0].offset, L"at its cycle's offset");
        Assert::AreEqual (s_kpszMdl2Refresh[0], strip[0].glyph, L"a reset's glyph");
        Assert::AreEqual (std::wstring (L"Reset at 3000"), strip[0].tip, L"its text and the labeler's top line");
        Assert::IsFalse  (strip[0].hasRows, L"no flyout");
    }
};
