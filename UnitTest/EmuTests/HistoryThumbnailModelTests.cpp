#include "Pch.h"

#include "HResultAssert.h"
#include "InlineWorkQueue.h"
#include "Debugger/Reverse/HistoryImageCache.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/IHistoryFrameRenderer.h"
#include "Debugger/Reverse/KeyframeStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kThumbStateBytes = 256;
static constexpr uint64_t  s_kThumbInterval   = 1000;
static constexpr uint64_t  s_kThumbStride     = 100;
static constexpr int       s_kFakeWidth       = 8;
static constexpr int       s_kFakeHeight      = 4;
static constexpr SIZE      s_kThumbCell       = { 4, 2 };
static constexpr size_t    s_kNoiseStateBytes = 4096;
static constexpr uint32_t  s_kThumbOpaque     = 0xFF000000u;
static constexpr uint64_t  s_kThumbTurnMs     = 125;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHistoryFrameRenderer
//
//  Draws a snapshot as one flat color taken from its first byte, and counts
//  the snapshots it was handed.
//
////////////////////////////////////////////////////////////////////////////////

class FakeHistoryFrameRenderer : public IHistoryFrameRenderer
{
public:
    HRESULT Render (const std::vector<Byte>  & state,
                    std::vector<uint32_t>    & outBgra,
                    int                      & outWidth,
                    int                      & outHeight) override
    {
        constexpr uint32_t  kOpaque = 0xFF000000u;



        calls++;
        drawn.insert (state[0]);

        if (clock != nullptr)
        {
            *clock += costMs;
        }

        outWidth  = s_kFakeWidth;
        outHeight = s_kFakeHeight;
        outBgra.assign ((size_t) s_kFakeWidth * s_kFakeHeight, kOpaque | state[0]);
        return S_OK;
    }

    int              calls  = 0;
    std::set<Byte>   drawn;              // the first byte of every snapshot drawn
    uint64_t       * clock  = nullptr;   // moved on by costMs for every picture, when given
    uint64_t         costMs = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailModelTests
//
//  The history strip's model: which keyframes its cells show, oldest at the
//  left and the newest at the right; the pictures drawn for them on a worker,
//  only while the strip shows and no faster than the bound; the cache that
//  keeps them; and the seek a click asks for.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryThumbnailModelTests)
{
public:

    TEST_METHOD (TheStepSpreadsTheCellsAndHoldsThroughSmallSwings)
    {
        Assert::AreEqual<uint64_t> (300, HistoryThumbnails::ChooseStep (900, 4, 0),   L"three steps over 900");
        Assert::AreEqual<uint64_t> (300, HistoryThumbnails::ChooseStep (800, 4, 300), L"a span a little shorter keeps the step");
        Assert::AreEqual<uint64_t> (300, HistoryThumbnails::ChooseStep (1100, 4, 300), L"a little longer keeps it too");
        Assert::AreEqual<uint64_t> (400, HistoryThumbnails::ChooseStep (1200, 4, 300), L"a quarter longer chooses afresh");
        Assert::AreEqual<uint64_t> (200, HistoryThumbnails::ChooseStep (600, 4, 300),  L"a third shorter chooses afresh");
        Assert::AreEqual<uint64_t> (1,   HistoryThumbnails::ChooseStep (0, 4, 0),      L"never zero");
    }


    TEST_METHOD (CellsSnapToTheKeyframeAtOrBeforeEachGridPoint)
    {
        constexpr int  kCount = 4;

        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells;
        uint64_t                           step = 0;



        Fill (store, 10);

        HistoryThumbnails::PlanCells (store, kCount, step, cells);

        Assert::AreEqual<size_t>   (kCount, cells.size(), L"one point per cell");
        Assert::AreEqual<uint64_t> (300,    step,              L"three steps over 0 to 900");
        Assert::AreEqual<uint64_t> (0,      cells[0].position, L"the oldest keyframe at the left");
        Assert::AreEqual<uint64_t> (300,    cells[1].position, L"a step on");
        Assert::AreEqual<uint64_t> (600,    cells[2].position, L"two steps on");
        Assert::AreEqual<uint64_t> (900,    cells[3].position, L"the newest keyframe at the right");
        Assert::IsFalse (cells[2].isLive, L"only the last stands for live");
        Assert::IsTrue  (cells[3].isLive, L"the last stands for live");

        step = 0;
        HistoryThumbnails::PlanCells (store, 3, step, cells);

        Assert::AreEqual<uint64_t> (400, cells[1].position, L"the grid point is 450, so the keyframe before it");
    }


    TEST_METHOD (GridPointsMoveAsWholeCellsAsTheLiveEndMoves)
    {
        KeyframeStore                      before;
        KeyframeStore                      after;
        std::vector<HistoryThumbnailCell>  cells;
        uint64_t                           step = 0;



        Fill (before, 10);
        Fill (after,  11);

        HistoryThumbnails::PlanCells (before, 4, step, cells);
        Assert::AreEqual<uint64_t> (600, cells[2].position, L"the last grid point before");

        HistoryThumbnails::PlanCells (after, 4, step, cells);

        Assert::AreEqual<uint64_t> (300,  step,              L"the step holds");
        Assert::AreEqual<uint64_t> (0,    cells[0].position, L"the first cell stays on the oldest keyframe");
        Assert::AreEqual<uint64_t> (600,  cells[1].position, L"the grid points moved one cell left, each its keyframe");
        Assert::AreEqual<uint64_t> (900,  cells[2].position, L"and a new one came in at the right");
        Assert::AreEqual<uint64_t> (1000, cells[3].position, L"the live end follows the newest keyframe");
    }


    //  However history grows, and as its oldest is dropped, the first cell
    //  is the oldest keyframe held and the last the newest, both always
    //  there; only the cells between them follow the grid.
    TEST_METHOD (TheFirstCellIsAlwaysTheOldestAndTheLastTheLiveEnd)
    {
        constexpr int     kCount  = 6;
        constexpr size_t  kAdds   = 60;
        constexpr size_t  kWarmUp = kCount;

        KeyframeStore                      store;
        KeyframeSettings                   settings;
        std::vector<HistoryThumbnailCell>  cells;
        std::vector<Byte>                  state (s_kThumbStateBytes);
        uint64_t                           step  = 0;
        size_t                             i     = 0;
        size_t                             grown = 0;
        HRESULT                            hr    = S_OK;



        settings.intervalCycles = s_kThumbInterval;
        store.Configure (settings);

        for (i = 0; i < kAdds; i++)
        {
            hr = store.Add (i * s_kThumbStride, i * s_kThumbInterval, state);
            AssertSucceeded (hr, L"Add");

            if (i < kWarmUp)
            {
                continue;
            }

            HistoryThumbnails::PlanCells (store, kCount, step, cells);

            Assert::AreEqual<size_t>   (kCount, cells.size(), L"every cell has a point");
            Assert::AreEqual<uint64_t> (store.GetInfo (0).position, cells.front().position, L"the first cell is the oldest keyframe");
            Assert::AreEqual<uint64_t> (store.GetInfo (store.GetCount() - 1).position, cells.back().position, L"the last is the newest");
            Assert::IsTrue (cells.back().isLive, L"and stands for live");

            grown += (cells[1].position > cells.front().position) ? 1 : 0;
        }

        Assert::IsTrue (grown > 0, L"the grid points moved on past the oldest while it stayed");
    }


    //  The time a cell's labels give is its keyframe's: the cycle it was
    //  taken at and the host's clock then.
    TEST_METHOD (ACellsLabelsGiveItsKeyframesTime)
    {
        constexpr uint64_t  kWall = 0x01DC0000AABBCCDDull;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now     = 0;
        uint64_t                  cycle   = 0;
        uint64_t                  wall    = 0;
        std::wstring              top;
        std::wstring              bottom;
        bool                      isShown = false;
        HRESULT                   hr      = S_OK;



        store.SetWallClock ([] () -> uint64_t { return kWall; });
        Prepare (thumbnails, queue, store, now);

        thumbnails.SetLabeler ([&] (uint64_t c, uint64_t w, std::wstring & outTop, std::wstring & outBottom)
        {
            cycle     = c;
            wall      = w;
            outTop    = L"top";
            outBottom = L"bottom";
        });

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays out 0, 400 and 800");

        isShown = thumbnails.TryGetCellLabels (1, top, bottom);

        Assert::IsTrue   (isShown, L"a cell has labels");
        Assert::AreEqual<uint64_t> (4 * s_kThumbInterval, cycle, L"its keyframe's cycle");
        Assert::AreEqual<uint64_t> (kWall, wall,                  L"and its keyframe's clock time");
        Assert::AreEqual (std::wstring (L"top"),    top,    L"the labeler's top line");
        Assert::AreEqual (std::wstring (L"bottom"), bottom, L"and bottom line");

        Assert::IsFalse (thumbnails.TryGetCellLabels (3, top, bottom), L"no cell, no labels");
    }


    //  A click anywhere along the strip seeks to the cycle under the
    //  pointer, on the scale the cells and the playhead line use: each
    //  cell's point by its cycle, a straight line between one point and the
    //  next, the leading end the oldest history and the trailing end where
    //  history ends, which is live.
    TEST_METHOD (AClickSeeksToTheCycleUnderThePointer)
    {
        FakeHistoryFrameRenderer           renderer;
        InlineWorkQueue                    queue;
        HistoryThumbnails                  thumbnails (renderer);
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  asked;
        uint64_t                           now = 0;
        HRESULT                            hr  = S_OK;



        Prepare (thumbnails, queue, store, now);

        thumbnails.SetOnSeek ([&asked] (const HistoryThumbnailCell & cell) { asked.push_back (cell); });

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays out 0, 4000 and 8000 cycles");

        thumbnails.SetPlayhead     (3 * s_kThumbStride, true);
        thumbnails.SetPlayheadTime (3 * s_kThumbInterval, 0, 9 * s_kThumbInterval);

        thumbnails.OnStripClicked (0, 0.0f);
        thumbnails.OnStripClicked (0, 0.25f);
        thumbnails.OnStripClicked (1, 1.5f);
        thumbnails.OnStripClicked (2, 2.5f);
        thumbnails.OnStripClicked (2, 3.0f);

        Assert::AreEqual<size_t>   (5,    asked.size(),    L"every click seeks");
        Assert::AreEqual<uint64_t> (0,    asked[0].cycle,  L"the leading end is the oldest history");
        Assert::AreEqual<uint64_t> (1000, asked[1].cycle,  L"a quarter of the way to the second point");
        Assert::AreEqual<uint64_t> (6000, asked[2].cycle,  L"halfway between the second and third");
        Assert::AreEqual<uint64_t> (8500, asked[3].cycle,  L"halfway from the newest keyframe to where history ends");
        Assert::AreEqual<uint64_t> (9000, asked[4].cycle,  L"the trailing end is where history ends");
        Assert::IsFalse (asked[3].isLive, L"inside history is not live");
        Assert::IsTrue  (asked[4].isLive, L"the trailing end is live");
    }


    //  A playhead line let go stays where it was dropped, labeled for that
    //  point, until the seek there lands: not where the machine stood
    //  before, nor where a seek made while it moved lands late. A seek that
    //  ran before the drop, to the same cycle, does not count as its landing.
    //  Once landed, the line follows the machine again.
    TEST_METHOD (ADroppedPlayheadStaysWhereItWasDroppedUntilTheSeekLands)
    {
        constexpr float     kDropped   = 1.5f;      // halfway between 4000 and 8000 cycles
        constexpr uint64_t  kDropCycle = 6000;
        constexpr uint64_t  kLanded    = 6003;      // the first instruction at or after it
        constexpr uint64_t  kRanOn     = 7000;
        constexpr float     kTolerance = 0.001f;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        std::vector<uint64_t>     scrubs;
        uint64_t                  now     = 0;
        uint64_t                  labeled = 0;
        float                     offset  = 0.0f;
        std::wstring              top;
        std::wstring              bottom;
        bool                      isOn    = false;
        HRESULT                   hr      = S_OK;



        Prepare (thumbnails, queue, store, now);

        thumbnails.SetOnScrub ([&scrubs] (uint64_t cycle, bool isFinal) { (void) isFinal; scrubs.push_back (cycle); });
        thumbnails.SetLabeler ([&labeled] (uint64_t c, uint64_t w, std::wstring & outTop, std::wstring & outBottom)
        {
            (void) w;
            labeled   = c;
            outTop    = L"top";
            outBottom = L"bottom";
        });

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays out 0, 4000 and 8000 cycles");

        thumbnails.SetPlayhead     (3 * s_kThumbStride, true);
        thumbnails.SetPlayheadTime (3 * s_kThumbInterval, 0, 9 * s_kThumbInterval);

        //  An earlier drop at the same point, landed, and the machine ran on.
        thumbnails.OnPlayheadDragged (kDropped, true);
        thumbnails.SetPlayheadTime   (kLanded, 0, 9 * s_kThumbInterval);
        thumbnails.NoteSeekLanded    (kDropCycle);
        thumbnails.SetPlayheadTime   (kRanOn, 0, 9 * s_kThumbInterval);

        isOn = thumbnails.TryGetPlayhead (offset, top, bottom);
        Assert::IsTrue (isOn, L"the line shows replaying");
        Assert::AreEqual (1.75f, offset, kTolerance, L"the line follows the machine once the earlier seek landed");

        //  Dragged away and dropped at the same point again.
        thumbnails.OnPlayheadDragged (0.5f,     false);
        thumbnails.OnPlayheadDragged (kDropped, true);
        Assert::AreEqual<uint64_t> (kDropCycle, scrubs.back(), L"the drop seeks to the point under it");

        thumbnails.TryGetPlayhead (offset, top, bottom);
        Assert::AreEqual (kDropped, offset, kTolerance, L"the dropped line stays where it was dropped, not where the machine stands");
        Assert::AreEqual<uint64_t> (kDropCycle, labeled, L"labeled for the point it was dropped at");

        //  The seek made while the line moved lands late.
        thumbnails.SetPlayheadTime (2 * s_kThumbInterval, 0, 9 * s_kThumbInterval);
        thumbnails.NoteSeekLanded  (2 * s_kThumbInterval);

        thumbnails.TryGetPlayhead (offset, top, bottom);
        Assert::AreEqual (kDropped, offset, kTolerance, L"a seek elsewhere landing late does not move the line");
        Assert::AreEqual<uint64_t> (kDropCycle, labeled, L"nor its labels");

        //  The drop's own seek lands.
        thumbnails.SetPlayheadTime (kLanded, 0, 9 * s_kThumbInterval);
        thumbnails.NoteSeekLanded  (kDropCycle);

        thumbnails.TryGetPlayhead (offset, top, bottom);
        Assert::AreEqual<uint64_t> (kLanded, labeled, L"once landed, the labels are where the machine stands");

        thumbnails.SetPlayheadTime (kRanOn, 0, 9 * s_kThumbInterval);
        thumbnails.TryGetPlayhead  (offset, top, bottom);
        Assert::AreEqual (1.75f, offset, kTolerance, L"and the line follows the machine as it runs on");

        //  A seek that never lands lets the line go after a while.
        thumbnails.OnPlayheadDragged (kDropped, true);
        now += (uint64_t) HistoryThumbnails::kSeekPatienceMs;

        thumbnails.TryGetPlayhead (offset, top, bottom);
        Assert::AreEqual (1.75f, offset, kTolerance, L"a seek that never lands stops holding the line");
    }


    //  The labels for a point along the strip give its own cycle and the
    //  host's clock there, counted on from the newest keyframe at or before
    //  it, as the playhead line's are once the machine stands there. An hour
    //  the machine spent stopped between two cells shows at the keyframe
    //  after it, not at the next cell.
    TEST_METHOD (TheLabelsUnderThePointerGiveItsExactTime)
    {
        constexpr uint64_t  kHourTicks = 36000000000ull;
        constexpr size_t    kAfterStop = 5;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        KeyframeSettings          settings;
        std::vector<Byte>         state (s_kThumbStateBytes);
        uint64_t                  now     = 0;
        uint64_t                  cycle   = 0;
        uint64_t                  wall    = 0;
        std::wstring              top;
        std::wstring              bottom;
        bool                      isShown = false;
        HRESULT                   hr      = S_OK;
        size_t                    i       = 0;



        settings.intervalCycles = s_kThumbInterval;
        store.Configure    (settings);
        store.SetWallClock ([] () -> uint64_t { return s_wall; });

        for (i = 0; i < 9; i++)
        {
            s_wall   = kBaseWall + i * kWallPerKeyframe + ((i >= kAfterStop) ? kHourTicks : 0);
            state[0] = static_cast<Byte> (i + 1);

            hr = store.Add (i * s_kThumbStride, i * s_kThumbInterval, state);
            AssertSucceeded (hr, L"Add");
        }

        thumbnails.SetWorkQueue  (&queue);
        thumbnails.SetClock      ([&now] { return (double) now; });
        thumbnails.SetCellLayout (3, s_kThumbCell);

        thumbnails.SetLabeler ([&] (uint64_t c, uint64_t w, std::wstring & outTop, std::wstring & outBottom)
        {
            cycle     = c;
            wall      = w;
            outTop    = L"top";
            outBottom = L"bottom";
        });

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays out 0, 4000 and 8000 cycles");

        thumbnails.SetPlayhead     (3 * s_kThumbStride, true);
        thumbnails.SetPlayheadTime (3 * s_kThumbInterval, 0, 9 * s_kThumbInterval);

        isShown = thumbnails.TryGetLabelsAt (1, 1.25f, top, bottom);

        Assert::IsTrue (isShown, L"a point has labels");
        Assert::AreEqual<uint64_t> (5000, cycle, L"its own cycle, a quarter of the way past the second cell");
        Assert::AreEqual<uint64_t> (store.GetInfo (kAfterStop).wallTime, wall, L"the clock at the keyframe there, after the hour stopped");

        thumbnails.TryGetLabelsAt (1, 1.3f, top, bottom);

        Assert::AreEqual<uint64_t> (5200, cycle, L"a pixel on is a cycle of its own");
        Assert::AreEqual<uint64_t> (HistoryStatus::GetWallTimeAt (store.GetInfo (kAfterStop).wallTime, 5000, 5200), wall, L"counted on from that keyframe");

        thumbnails.TryGetLabelsAt (0, 0.5f, top, bottom);

        Assert::AreEqual<uint64_t> (2000, cycle, L"halfway along the first cell");
        Assert::AreEqual<uint64_t> (store.GetInfo (2).wallTime, wall, L"the clock at the keyframe there");
    }


    //  While history is short and growing, the grid moves under a cell
    //  faster than its new point is drawn, so for a while the cell shows the
    //  picture of a snapshot that is no longer its point. Its labels give the
    //  time of the snapshot its picture shows, and its preview shows that
    //  same snapshot, painted before the worker draws and after.
    TEST_METHOD (AHoveredCellsTimeAndPreviewMatchItsPictureWhileHistoryGrows)
    {
        constexpr size_t  kRounds   = 80;
        constexpr size_t  kPerRound = 3;
        constexpr int     kCount    = 8;
        constexpr int     kHovered  = 1;
        constexpr int     kSlower   = 2;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        KeyframeSettings          settings;
        std::vector<Byte>         state (s_kThumbStateBytes);
        uint64_t                  now     = 0;
        uint64_t                  cycle   = 0;
        size_t                    round   = 0;
        size_t                    added   = 0;
        size_t                    i       = 0;
        size_t                    checked = 0;
        size_t                    stale   = 0;
        HRESULT                   hr      = S_OK;



        settings.intervalCycles = s_kThumbInterval;
        store.Configure (settings);

        thumbnails.SetWorkQueue   (&queue);
        thumbnails.SetClock       ([&now] { return (double) now; });
        thumbnails.SetCellLayout  (kCount, s_kThumbCell);
        thumbnails.SetVisible     (true);
        thumbnails.SetHoveredCell (kHovered);

        thumbnails.SetLabeler ([&cycle] (uint64_t c, uint64_t w, std::wstring & outTop, std::wstring & outBottom)
        {
            (void) w;
            cycle     = c;
            outTop    = L"top";
            outBottom = L"bottom";
        });

        for (round = 0; round < kRounds; round++)
        {
            //  Keyframes come faster than pictures are drawn.
            for (i = 0; i < kPerRound; i++, added++)
            {
                state[0] = static_cast<Byte> (added + 1);

                hr = store.Add (added * s_kThumbStride, added * s_kThumbInterval, state);
                AssertSucceeded (hr, L"Add");
            }

            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service");

            CheckShownTime (thumbnails, kHovered, cycle, checked, stale);

            queue.TryRunNext();

            CheckShownTime (thumbnails, kHovered, cycle, checked, stale);

            now += s_kThumbTurnMs / kSlower;
        }

        Assert::IsTrue (checked > 0, L"the hovered cell showed pictures");
        Assert::IsTrue (stale > 0,   L"and the grid moved under it before its new point was drawn");
    }


    //  The start time before the pictures goes to where history begins: the
    //  same seek a click on the first cell asks for.
    TEST_METHOD (TheStartTimeGoesToTheOldestHistoryAndBothEndsHaveTips)
    {
        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        HistoryThumbnailCell      asked;
        uint64_t                  now   = 0;
        int                       seeks = 0;
        HRESULT                   hr    = S_OK;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetOnSeek ([&] (const HistoryThumbnailCell & cell) { asked = cell; seeks++; });

        thumbnails.OnLeadingLabelClicked();
        Assert::AreEqual (0, seeks, L"no history laid out, nowhere to go");

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays out 0, 400 and 800");

        thumbnails.OnLeadingLabelClicked();

        Assert::AreEqual (1, seeks, L"one seek");
        Assert::AreEqual<uint64_t> (0, asked.position, L"to the oldest keyframe held");
        Assert::IsFalse (asked.isLive, L"a point in the past");

        Assert::AreEqual (std::wstring (L"Go to the start of history"), thumbnails.GetLeadingTip(), L"the start time's tip");

        Assert::AreEqual (std::wstring (L"Running live"), thumbnails.GetTrailingTip(), L"Live's tip while live");
        Assert::IsFalse  (thumbnails.IsTrailingLabelClickable(), L"Live is no button while live");

        thumbnails.SetPlayhead (400, true);
        Assert::AreEqual (std::wstring (L"Return to live"), thumbnails.GetTrailingTip(), L"Live's tip while replaying");
        Assert::IsTrue   (thumbnails.IsTrailingLabelClickable(), L"Live is a button while replaying");
    }


    //  The cell under the pointer is drawn as soon as nothing else is being
    //  drawn, not after the wait that paces the thumbnails.
    TEST_METHOD (TheHoveredPreviewIsDrawnWithoutWaitingItsTurn)
    {
        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now = 0;
        HRESULT                   hr  = S_OK;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetVisible (true);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service");
        Assert::IsTrue (queue.TryRunNext(), L"the live end is drawn");

        thumbnails.SetHoveredCell (0);
        Assert::IsNull (thumbnails.GetPreviewImage (0).get(), L"the hovered cell is not drawn yet");

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service at once");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"the hovered cell goes to the worker right away");
        Assert::IsTrue (queue.TryRunNext(), L"drawn");

        Assert::IsNotNull (thumbnails.GetPreviewImage (0).get(), L"its full-size picture is ready");
    }


    //  With the pointer resting on a cell, the cells either side are drawn
    //  at full size next, so moving to one shows it sharp at once.
    TEST_METHOD (TheHoveredCellsNeighborsAreDrawnNext)
    {
        constexpr int  kCount   = 7;
        constexpr int  kHovered = 3;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now   = 0;
        int                       round = 0;
        HRESULT                   hr    = S_OK;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetCellLayout (kCount, s_kThumbCell);
        thumbnails.SetVisible (true);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays the points out");
        queue.TryRunNext();

        thumbnails.SetHoveredCell (kHovered);
        thumbnails.GetPreviewImage (kHovered);

        for (round = 0; round < 1 + 2 * (int) HistoryThumbnails::kPrefetchReach; round++)
        {
            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service with no time passing");
            queue.TryRunNext();
        }

        for (int index : { kHovered - 2, kHovered - 1, kHovered, kHovered + 1, kHovered + 2 })
        {
            Assert::IsNotNull (thumbnails.GetPreviewImage (index).get(), L"the hovered cell and the two either side are sharp");
        }
    }

    TEST_METHOD (NothingIsPlannedWithoutHistoryOrRoom)
    {
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells (1);
        uint64_t                           step = 0;



        HistoryThumbnails::PlanCells (store, 4, step, cells);
        Assert::IsTrue (cells.empty(), L"no keyframes, no cells");

        Fill (store, 3);

        HistoryThumbnails::PlanCells (store, 0, step, cells);
        Assert::IsTrue (cells.empty(), L"no room, no cells");
    }


    TEST_METHOD (TheCacheDropsTheLeastRecentlyUsed)
    {
        HistoryImageCache  cache (2);



        cache.Put (1, std::make_shared<DxuiIconImage>());
        cache.Put (2, std::make_shared<DxuiIconImage>());

        Assert::IsNotNull (cache.Find (1).get(), L"finding 1 uses it");

        cache.Put (3, std::make_shared<DxuiIconImage>());

        Assert::IsTrue  (cache.Contains (1), L"used most recently, kept");
        Assert::IsFalse (cache.Contains (2), L"used longest ago, dropped");
        Assert::IsTrue  (cache.Contains (3), L"just added");
        Assert::AreEqual<size_t> (2, cache.GetCount(), L"never over its capacity");

        cache.SetCapacity (1);

        Assert::IsFalse (cache.Contains (1), L"a smaller capacity drops the oldest");
        Assert::IsTrue  (cache.Contains (3), L"and keeps the newest");
    }


    //  A spare picture, one drawn for a thumbnail rather than asked for at
    //  full size, is kept only in room to spare: it never pushes out a
    //  picture in use, and it is the first to go when one needs the room.
    TEST_METHOD (ASparePictureNeverPushesOutOneInUse)
    {
        HistoryImageCache  cache (3);



        cache.Put      (1, std::make_shared<DxuiIconImage>());
        cache.Put      (2, std::make_shared<DxuiIconImage>());
        cache.PutSpare (3, std::make_shared<DxuiIconImage>());

        Assert::IsTrue (cache.Contains (3), L"kept while there is room");

        cache.PutSpare (4, std::make_shared<DxuiIconImage>());

        Assert::IsFalse (cache.Contains (4), L"not kept once the cache is full");
        Assert::IsTrue  (cache.Contains (1), L"the pictures in use stay");
        Assert::IsTrue  (cache.Contains (2), L"the pictures in use stay");

        cache.Put (5, std::make_shared<DxuiIconImage>());

        Assert::IsFalse (cache.Contains (3), L"the spare goes first when a picture in use needs room");
        Assert::IsTrue  (cache.Contains (1), L"ahead of the pictures in use");
    }


    TEST_METHOD (ShrinkingAveragesTheCoveredPixels)
    {
        DxuiIconImage  source;
        DxuiIconImage  shrunk;



        source.width      = 2;
        source.height     = 1;
        source.bgraPremul = { 0xFF000000u, 0xFF0000FEu };

        HistoryThumbnails::Shrink (source, 1, 1, shrunk);

        Assert::AreEqual (1, shrunk.width,  L"width");
        Assert::AreEqual (1, shrunk.height, L"height");
        Assert::AreEqual<uint32_t> (0xFF00007Fu, shrunk.bgraPremul[0], L"the two pixels averaged");
    }



    TEST_METHOD (NothingIsDrawnWhileTheStripIsHidden)
    {
        FakeHistoryFrameRenderer           renderer;
        InlineWorkQueue                    queue;
        HistoryThumbnails                  thumbnails (renderer);
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells;
        uint64_t                           now = 0;
        HRESULT                            hr  = S_OK;



        Prepare (thumbnails, queue, store, now);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service");

        Assert::AreEqual<size_t> (0, queue.GetPendingCount(), L"hidden: nothing handed to the worker");
        Assert::AreEqual (0, renderer.calls, L"hidden: nothing drawn");

        thumbnails.GetCells (cells);
        Assert::AreEqual<size_t> (3, cells.size(), L"the points are laid out all the same");
    }


    //  One picture in flight at a time, unpacked and drawn on the worker,
    //  and the next thumbnail paced by what the last one cost: kWorkShare
    //  times its cost after it went, so the worker is never busy more than
    //  that share of the time.
    TEST_METHOD (DrawingIsBoundedAndOnTheWorker)
    {
        constexpr uint64_t  kCostMs = 10;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now     = 0;
        uint64_t                  spacing = kCostMs * HistoryThumbnails::kWorkShare;
        HRESULT                   hr      = S_OK;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetVisible (true);

        renderer.clock  = &now;
        renderer.costMs = kCostMs;

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service");

        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"one picture handed to the worker");
        Assert::AreEqual (0, renderer.calls, L"not drawn on the machine's thread");

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service while one is in flight");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"never two in flight");

        Assert::IsTrue (queue.TryRunNext(), L"the worker draws it");
        Assert::AreEqual (1, renderer.calls, L"drawn once");
        Assert::IsNotNull (thumbnails.GetCellImage (2).get(), L"the live end is drawn first");
        Assert::IsNull    (thumbnails.GetCellImage (1).get(), L"the points wait their turn");

        Assert::AreEqual<double> ((double) kCostMs, thumbnails.GetLastRenderMs(), L"what the picture cost");

        now += spacing - kCostMs - 1;

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service too soon");
        Assert::AreEqual<size_t> (0, queue.GetPendingCount(), L"too soon after the last: nothing more");

        now++;

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service once due");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"due again: the next picture");
        Assert::IsTrue (queue.TryRunNext(), L"drawn");

        Assert::IsNotNull (thumbnails.GetCellImage (0).get(), L"then the first cell, where history begins");
        Assert::IsNull    (thumbnails.GetCellImage (1).get(), L"ahead of the grid points");
        Assert::AreEqual ((int) s_kThumbCell.cx, thumbnails.GetCellImage (0)->width,  L"a thumbnail is the cell's width");
        Assert::AreEqual ((int) s_kThumbCell.cy, thumbnails.GetCellImage (0)->height, L"and the cell's height");
    }


    //  The machine's thread only copies a keyframe out, still packed; it
    //  never waits for one still being packed, and never unpacks. The live
    //  end, just taken and in flight, is passed over for a point that is
    //  ready, and drawn once its packing is done.
    TEST_METHOD (AKeyframeStillBeingPackedIsNotWaitedFor)
    {
        constexpr size_t  kKeyframes = 9;
        constexpr int     kCount     = 3;
        constexpr int     kLiveCell  = kCount - 1;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           thumbQueue;
        InlineWorkQueue           storeQueue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        KeyframeSettings          settings;
        std::vector<Byte>         state (s_kThumbStateBytes);
        uint64_t                  now   = 0;
        size_t                    i     = 0;
        HRESULT                   hr    = S_OK;



        settings.intervalCycles = s_kThumbInterval;
        store.Configure    (settings);
        store.SetWorkQueue (&storeQueue);

        for (i = 0; i < kKeyframes; i++)
        {
            state[0] = static_cast<Byte> (i + 1);

            hr = store.Add (i * s_kThumbStride, i * s_kThumbInterval, state);
            AssertSucceeded (hr, L"Add");

            if (i + 1 < kKeyframes)
            {
                Assert::IsTrue (storeQueue.TryRunNext(), L"packed");
            }
        }

        thumbnails.SetWorkQueue  (&thumbQueue);
        thumbnails.SetClock      ([&now] { return (double) now; });
        thumbnails.SetCellLayout (kCount, s_kThumbCell);
        thumbnails.SetVisible    (true);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service with the newest keyframe in flight");

        Assert::AreEqual<size_t> (1, storeQueue.GetPendingCount(), L"the keyframe being packed was not waited for");
        Assert::AreEqual<size_t> (1, thumbQueue.GetPendingCount(), L"a point that is ready went instead");
        Assert::AreEqual (0, renderer.calls, L"nothing unpacked or drawn on the machine's thread");

        Assert::IsTrue (thumbQueue.TryRunNext(), L"drawn");
        Assert::IsNotNull (thumbnails.GetCellImage (0).get(),        L"the first cell");
        Assert::IsNull    (thumbnails.GetCellImage (kLiveCell).get(), L"not the live end, still being packed");

        Assert::IsTrue (storeQueue.TryRunNext(), L"its packing finishes");

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service once it is packed");
        Assert::IsTrue (thumbQueue.TryRunNext(), L"drawn");

        Assert::IsNotNull (thumbnails.GetCellImage (kLiveCell).get(), L"now the live end");
        Assert::AreEqual<uint32_t> (s_kThumbOpaque | static_cast<Byte> (kKeyframes), thumbnails.GetCellImage (kLiveCell)->bgraPremul[0], L"from the newest keyframe");
    }


    TEST_METHOD (TheCellUnderThePointerIsDrawnFirstAtFullSize)
    {
        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now = 0;
        HRESULT                   hr  = S_OK;



        Prepare (thumbnails, queue, store, now);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service while hidden lays the points out");

        thumbnails.SetVisible (true);

        Assert::IsNull (thumbnails.GetPreviewImage (1).get(), L"not drawn yet, and now asked for");

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service");
        Assert::IsTrue (queue.TryRunNext(), L"drawn");

        Assert::IsNotNull (thumbnails.GetPreviewImage (1).get(), L"the preview asked for comes first");
        Assert::AreEqual (s_kFakeWidth,  thumbnails.GetPreviewImage (1)->width,  L"at the picture's own width");
        Assert::AreEqual (s_kFakeHeight, thumbnails.GetPreviewImage (1)->height, L"and height");
        Assert::IsNull (thumbnails.GetCellImage (2).get(), L"the live end waits behind it");
    }


    TEST_METHOD (AClickSeeksToTheCellsKeyframe)
    {
        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore         store;
        uint64_t              now   = 0;
        HistoryThumbnailCell  asked;
        int                   seeks = 0;
        HRESULT               hr    = S_OK;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetOnSeek ([&] (const HistoryThumbnailCell & cell) { asked = cell; seeks++; });

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service");

        thumbnails.OnCellClicked (1);

        Assert::AreEqual (1, seeks, L"one seek");
        Assert::AreEqual<uint64_t> (400, asked.position, L"to the keyframe the cell shows");
        Assert::IsFalse (asked.isLive, L"a point in the past");

        thumbnails.OnCellClicked (2);
        Assert::IsTrue (asked.isLive, L"the last cell is live");

        thumbnails.OnCellClicked (3);
        Assert::AreEqual (2, seeks, L"no cell, no seek");
    }


    //  The keyframes packed are collected when one is copied out, and
    //  collecting them can drop the oldest groups, which moves every index.
    //  The picture must still come from the keyframe at its point.
    TEST_METHOD (APictureComesFromItsPointWhenCollectingDropsTheOldest)
    {
        constexpr size_t  kRounds   = 200;
        constexpr int     kLiveCell = 0;

        FakeHistoryFrameRenderer           renderer;
        InlineWorkQueue                    thumbQueue;
        InlineWorkQueue                    storeQueue;
        HistoryThumbnails                  thumbnails (renderer);
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells;
        HistoryThumbnails::Image           image;
        uint64_t                           now      = 0;
        uint64_t                           position = 0;
        uint32_t                           seed     = 1;
        size_t                             round    = 0;
        HRESULT                            hr       = S_OK;



        PrepareTight (thumbnails, thumbQueue, store, storeQueue, now);
        thumbnails.SetCellLayout (1, s_kThumbCell);
        thumbnails.SetVisible (true);

        for (round = 0; round < kRounds; round++)
        {
            AddInFlight (store, position, seed);
            FinishPacking (storeQueue);

            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service");

            Assert::IsTrue (thumbQueue.TryRunNext(), L"a picture was handed to the worker");

            thumbnails.GetCells (cells);
            image = thumbnails.GetCellImage (kLiveCell);

            Assert::IsNotNull (image.get(), L"the live end is drawn");
            Assert::AreEqual<uint32_t> (s_kThumbOpaque | GetTag (cells[kLiveCell].position), image->bgraPremul[0], L"from the keyframe at the live end");

            now += s_kThumbTurnMs;
        }

        Assert::IsTrue (store.GetInfo (0).position > 0, L"and the oldest keyframes were dropped along the way");
    }


    TEST_METHOD (APreviewOfADroppedPointIsForgotten)
    {
        constexpr int  kOldestCell = 0;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           thumbQueue;
        InlineWorkQueue           storeQueue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now      = 0;
        uint64_t                  position = 0;
        uint32_t                  seed     = 1;
        HRESULT                   hr       = S_OK;



        PrepareTight (thumbnails, thumbQueue, store, storeQueue, now);

        AddInFlight (store, position, seed);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service while hidden lays the points out");

        Assert::IsNull (thumbnails.GetPreviewImage (kOldestCell).get(), L"the oldest point is asked for");

        while (store.GetInfo (0).position == 0)
        {
            AddInFlight (store, position, seed);
        }

        thumbnails.SetVisible (true);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service with the point dropped");

        now += s_kThumbTurnMs;

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service once due");

        Assert::IsTrue (thumbQueue.TryRunNext(), L"the strip moved on to a point history still holds");
        Assert::AreEqual (1, renderer.calls, L"drawn");
    }


    //  Once history is full every new keyframe drops the oldest, and the
    //  keyframes come faster than pictures are drawn. The grid points must
    //  hold still while history scrolls, so each step brings at most a few
    //  new points at the right, and every point gets its own picture; the
    //  first cell, the oldest keyframe, follows each drop.
    TEST_METHOD (TheStripHoldsStillWhileHistoryScrolls)
    {
        constexpr size_t  kRounds          = 400;
        constexpr size_t  kWarmUpRounds    = 100;
        constexpr int     kCount           = 8;
        constexpr int     kLiveCell        = kCount - 1;
        constexpr size_t  kMaxNewPerRound  = 2;
        constexpr size_t  kMostNumerator   = 10;
        constexpr size_t  kMostDenominator = 9;

        FakeHistoryFrameRenderer           renderer;
        InlineWorkQueue                    thumbQueue;
        InlineWorkQueue                    storeQueue;
        HistoryThumbnails                  thumbnails (renderer);
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells;
        std::set<uint64_t>                 previous;
        std::set<uint64_t>                 current;
        HistoryThumbnails::Image           image;
        uint64_t                           now      = 0;
        uint64_t                           position = 0;
        uint64_t                           dropped  = 0;
        uint32_t                           seed     = 1;
        size_t                             round    = 0;
        size_t                             i        = 0;
        size_t                             fresh    = 0;
        size_t                             ownGrid  = 0;
        size_t                             checked  = 0;
        HRESULT                            hr       = S_OK;



        PrepareTight (thumbnails, thumbQueue, store, storeQueue, now);
        thumbnails.SetCellLayout (kCount, s_kThumbCell);
        thumbnails.SetVisible (true);

        for (round = 0; round < kRounds; round++)
        {
            AddInFlight (store, position, seed);
            FinishPacking (storeQueue);

            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service");

            thumbQueue.TryRunNext();
            now += s_kThumbTurnMs;

            thumbnails.GetCells (cells);
            Assert::AreEqual<size_t> (kCount, cells.size(), L"every cell has a point");

            current.clear();
            fresh = 0;

            for (i = 0; i < kCount; i++)
            {
                image = thumbnails.GetCellImage ((int) i);
            }

            for (i = 0; i < kLiveCell; i++)
            {
                current.insert (cells[i].position);
                fresh += previous.count (cells[i].position) == 0 ? 1 : 0;
            }

            previous = current;

            if (round < kWarmUpRounds)
            {
                dropped = store.GetInfo (0).position;
                continue;
            }

            Assert::IsTrue (fresh <= kMaxNewPerRound, L"a scroll step brings only a few new points");

            //  The first cell moves with every drop of the oldest and is drawn
            //  ahead of the grid points, so it is always the oldest itself.
            image = thumbnails.GetCellImage (0);

            Assert::IsNotNull (image.get(), L"the first cell is drawn");
            Assert::AreEqual<uint32_t> (s_kThumbOpaque | GetTag (cells[0].position), image->bgraPremul[0], L"with the oldest keyframe's own picture");

            //  Here the oldest is dropped every round and only one picture is
            //  drawn a round, so a grid point arriving at the right can wait a
            //  round or two; it still shows a picture meanwhile.
            for (i = 1; i < kLiveCell; i++)
            {
                image = thumbnails.GetCellImage ((int) i);

                Assert::IsNotNull (image.get(), L"every grid point shows a picture");

                ownGrid += (image->bgraPremul[0] == (s_kThumbOpaque | GetTag (cells[i].position))) ? 1 : 0;
                checked++;
            }

            Assert::IsNotNull (thumbnails.GetCellImage (kLiveCell).get(), L"the live end is drawn");
        }

        Assert::IsTrue (store.GetInfo (0).position > dropped, L"the oldest kept being dropped");
        Assert::IsTrue (ownGrid * kMostNumerator > checked * kMostDenominator, L"the grid points nearly always show their own pictures");
    }


    //  The live end moves on with every keyframe; until the new picture is
    //  drawn the cell keeps showing the one before.
    TEST_METHOD (TheLiveCellNeverShowsEmptyOnceDrawn)
    {
        constexpr size_t  kRounds   = 100;
        constexpr int     kCount    = 8;
        constexpr int     kLiveCell = kCount - 1;

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           thumbQueue;
        InlineWorkQueue           storeQueue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now      = 0;
        uint64_t                  position = 0;
        uint32_t                  seed     = 1;
        size_t                    round    = 0;
        bool                      isDrawn  = false;
        HRESULT                   hr       = S_OK;



        PrepareTight (thumbnails, thumbQueue, store, storeQueue, now);
        thumbnails.SetCellLayout (kCount, s_kThumbCell);
        thumbnails.SetVisible (true);

        for (round = 0; round < kRounds; round++)
        {
            AddInFlight (store, position, seed);
            FinishPacking (storeQueue);

            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service");

            if (isDrawn)
            {
                Assert::IsNotNull (thumbnails.GetCellImage (kLiveCell).get(), L"the live cell keeps its picture while the next is drawn");
            }

            thumbQueue.TryRunNext();
            now += s_kThumbTurnMs;

            isDrawn = isDrawn || thumbnails.GetCellImage (kLiveCell) != nullptr;
        }

        Assert::IsTrue (isDrawn, L"the live cell was drawn");
    }


    //  A new cell size is met from the pictures already drawn, scaled to it,
    //  rather than by drawing every point again.
    TEST_METHOD (ANewCellSizeRescalesWithoutDrawingAgain)
    {
        constexpr SIZE  kWider = { 6, 3 };

        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        HistoryThumbnails::Image  image;
        uint64_t                  now   = 0;
        int                       calls = 0;
        int                       i     = 0;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetVisible (true);
        DrawAll (thumbnails, queue, store, now);

        calls = renderer.calls;
        thumbnails.SetCellLayout (3, kWider);

        for (i = 0; i < 3; i++)
        {
            image = thumbnails.GetCellImage (i);

            Assert::IsNotNull (image.get(), L"every cell has a picture right after the resize");
            Assert::AreEqual ((int) kWider.cx, image->width,  L"at the new width");
            Assert::AreEqual ((int) kWider.cy, image->height, L"and the new height");
        }

        Assert::AreEqual (calls, renderer.calls, L"and nothing was drawn again");
    }


    //  A resize that changes the cell count moves the points; one not drawn
    //  yet shows the nearest picture drawn until its own is ready.
    TEST_METHOD (ANewPointShowsTheNearestPictureUntilDrawn)
    {
        FakeHistoryFrameRenderer           renderer;
        InlineWorkQueue                    queue;
        HistoryThumbnails                  thumbnails (renderer);
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells;
        uint64_t                           now = 0;
        int                                i   = 0;
        HRESULT                            hr  = S_OK;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetVisible (true);
        DrawAll (thumbnails, queue, store, now);

        thumbnails.SetCellLayout (5, s_kThumbCell);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays out five points");

        thumbnails.GetCells (cells);
        Assert::AreEqual<size_t> (5, cells.size(), L"five points");

        for (i = 0; i < 5; i++)
        {
            Assert::IsNotNull (thumbnails.GetCellImage (i).get(), L"every cell shows a picture at once");
        }
    }


    //  Resizing the window wider and narrower changes the cell count over a
    //  history that begins at power-on. Every cell keeps a point of its own:
    //  no grid point reaches back onto the oldest keyframe the first cell
    //  shows, so the oldest picture never fills several cells. A cell shows
    //  a stand-in only until its own picture is drawn, and from then on that
    //  picture and its time.
    TEST_METHOD (EveryCellShowsItsOwnPointThroughAResizeSequence)
    {
        constexpr size_t  kKeyframes = 40;
        constexpr int     kRounds    = 64;

        static constexpr int  kCounts[] = { 10, 13, 11, 16, 9, 14, 12, 17 };

        FakeHistoryFrameRenderer           renderer;
        InlineWorkQueue                    queue;
        HistoryThumbnails                  thumbnails (renderer);
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells;
        uint64_t                           now     = 0;
        uint64_t                           labeled = 0;
        HRESULT                            hr      = S_OK;
        int                                round   = 0;
        size_t                             i       = 0;
        size_t                             checked = 0;



        Fill (store, kKeyframes);

        thumbnails.SetWorkQueue (&queue);
        thumbnails.SetClock     ([&now] { return (double) now; });
        thumbnails.SetVisible   (true);

        thumbnails.SetLabeler ([&labeled] (uint64_t c, uint64_t w, std::wstring & outTop, std::wstring & outBottom)
        {
            (void) w;
            labeled   = c;
            outTop    = L"top";
            outBottom = L"bottom";
        });

        for (int count : kCounts)
        {
            thumbnails.SetCellLayout (count, s_kThumbCell);

            for (round = 0; round < kRounds; round++)
            {
                hr = thumbnails.Service (store);
                AssertSucceeded (hr, L"Service");

                queue.TryRunNext();
                now += s_kThumbTurnMs;

                thumbnails.GetCells (cells);
                Assert::AreEqual<size_t> ((size_t) count, cells.size(), L"a point for every cell");

                for (i = 1; i < cells.size(); i++)
                {
                    Assert::IsTrue (cells[i].position > cells[i - 1].position, L"every cell has a point of its own, later than the one before");
                }

                for (i = 0; i < cells.size(); i++)
                {
                    checked += CheckOwnPicture (thumbnails, renderer, cells, i, labeled) ? 1 : 0;
                }
            }

            for (i = 0; i < cells.size(); i++)
            {
                Assert::IsTrue (CheckOwnPicture (thumbnails, renderer, cells, i, labeled), L"once settled every cell shows its own picture");
            }
        }

        Assert::IsTrue (checked > 0, L"cells were checked against their own pictures");
    }


    //  New points after a resize are drawn faster than the bound that holds
    //  while history scrolls, so the strip settles quickly.
    //  With pictures that cost next to nothing, nothing holds the next back
    //  but the one picture a turn: no clock time need pass between them.
    TEST_METHOD (NewPointsAfterAResizeAreDrawnOneATurn)
    {
        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now = 0;
        HRESULT                   hr  = S_OK;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetVisible (true);
        DrawAll (thumbnails, queue, store, now);

        thumbnails.SetCellLayout (5, s_kThumbCell);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays out five points and hands one over");
        Assert::IsTrue (queue.TryRunNext(), L"the first new point is drawn");

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service on the next turn");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"the next new point follows at once");
    }

    TEST_METHOD (TheMarkedCellFollowsThePlayhead)
    {
        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now = 0;
        HRESULT                   hr  = S_OK;



        Prepare (thumbnails, queue, store, now);

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service lays out 0, 400 and 800");

        thumbnails.SetPlayhead (800, false);
        Assert::IsFalse (thumbnails.IsBehindLive(), L"live");
        Assert::AreEqual (2, thumbnails.GetMarkedCell(), L"live: the live end");

        thumbnails.SetPlayhead (450, true);
        Assert::IsTrue (thumbnails.IsBehindLive(), L"replaying");
        Assert::AreEqual (1, thumbnails.GetMarkedCell(), L"behind live: the point at or before");
    }


    //  Live beside the pictures: accented only while live, and a click on it
    //  while replaying seeks to the live end; live, it does nothing.
    TEST_METHOD (LiveIsAccentedWhileLiveAndAClickReplayingGoesLive)
    {
        FakeHistoryFrameRenderer  renderer;
        HistoryThumbnails         thumbnails (renderer);
        HistoryThumbnailCell      asked;
        int                       seeks = 0;



        thumbnails.SetOnSeek ([&] (const HistoryThumbnailCell & cell) { asked = cell; seeks++; });

        thumbnails.SetPlayhead (800, false);
        Assert::AreEqual (std::wstring (L"Live"), thumbnails.GetTrailingLabel(), L"the label");
        Assert::IsTrue   (thumbnails.IsTrailingLabelAccented(),                  L"accented while live");

        thumbnails.OnTrailingLabelClicked();
        Assert::AreEqual (0, seeks, L"live has nowhere to go");

        thumbnails.SetPlayhead (450, true);
        Assert::AreEqual (std::wstring (L"Live"), thumbnails.GetTrailingLabel(), L"the same label replaying");
        Assert::IsFalse  (thumbnails.IsTrailingLabelAccented(),                  L"plain while replaying");

        thumbnails.OnTrailingLabelClicked();
        Assert::AreEqual (1, seeks,        L"replaying, a click seeks");
        Assert::IsTrue   (asked.isLive,    L"to the live end");
    }

private:

    static constexpr uint64_t  kBaseWall        = 0x01DC000000000000ull;
    static constexpr uint64_t  kWallPerKeyframe = 9775;      // 1000 cycles of 100 ns ticks, near enough

    static inline uint64_t  s_wall = 0;


    //  Paints a cell as the strip does, picture first, and checks that the
    //  time its labels give and its preview belong to the snapshot whose
    //  picture it shows; counts a cell showing a snapshot that is no longer
    //  its point as stale.
    static void CheckShownTime (HistoryThumbnails & thumbnails, int index, const uint64_t & labeledCycle, size_t & ioChecked, size_t & ioStale)
    {
        constexpr uint32_t  kTagMask = 0xFFu;

        std::vector<HistoryThumbnailCell>  cells;
        HistoryThumbnails::Image           image   = thumbnails.GetCellImage (index);
        HistoryThumbnails::Image           preview;
        std::wstring                       top;
        std::wstring                       bottom;
        uint64_t                           shown   = 0;
        bool                               isShown = false;



        if (image == nullptr || image->bgraPremul.empty())
        {
            return;
        }

        shown   = (uint64_t) ((image->bgraPremul[0] & kTagMask) - 1) * s_kThumbInterval;
        isShown = thumbnails.TryGetCellLabels (index, top, bottom);

        Assert::IsTrue (isShown, L"a cell with a picture has labels");
        Assert::AreEqual<uint64_t> (shown, labeledCycle, L"the labels give the time of the snapshot the picture shows");

        preview = thumbnails.GetPreviewImage (index);

        if (preview != nullptr)
        {
            Assert::AreEqual<uint32_t> (image->bgraPremul[0], preview->bgraPremul[0], L"the preview shows the snapshot the cell shows");
        }

        thumbnails.GetCells (cells);

        ioStale += (cells[(size_t) index].cycle != shown) ? 1 : 0;
        ioChecked++;
    }


    //  Whether cell `index` shows its own point's picture, which it must once
    //  that picture is drawn; and then its labels give its point's time.
    static bool CheckOwnPicture (HistoryThumbnails & thumbnails, const FakeHistoryFrameRenderer & renderer, const std::vector<HistoryThumbnailCell> & cells, size_t index, const uint64_t & labeledCycle)
    {
        constexpr uint32_t  kTagMask = 0xFFu;

        HistoryThumbnails::Image  image   = thumbnails.GetCellImage ((int) index);
        Byte                      own     = GetTag (cells[index].position);
        bool                      isDrawn = renderer.drawn.contains (own);
        std::wstring              top;
        std::wstring              bottom;



        if (!isDrawn)
        {
            return false;
        }

        Assert::IsNotNull (image.get(), L"a cell whose picture is drawn shows a picture");
        Assert::AreEqual<uint32_t> (own, image->bgraPremul[0] & kTagMask, L"a cell whose own picture is drawn shows it, not a stand-in");

        thumbnails.TryGetCellLabels ((int) index, top, bottom);
        Assert::AreEqual<uint64_t> (cells[index].cycle, labeledCycle, L"and its labels give its own point's time");

        return true;
    }


    //  Runs the strip until every cell's picture is drawn.
    static void DrawAll (HistoryThumbnails & thumbnails, InlineWorkQueue & queue, KeyframeStore & store, uint64_t & now)
    {
        constexpr int  kMaxRounds = 16;

        HRESULT  hr    = S_OK;
        int      round = 0;



        for (round = 0; round < kMaxRounds; round++)
        {
            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service");

            queue.TryRunNext();
            now += s_kThumbTurnMs;
        }

        Assert::IsNotNull (thumbnails.GetCellImage (0).get(), L"the oldest point is drawn");
        Assert::IsNotNull (thumbnails.GetCellImage (1).get(), L"the middle point is drawn");
        Assert::IsNotNull (thumbnails.GetCellImage (2).get(), L"the live end is drawn");
    }



    static void Fill (KeyframeStore & store, size_t count)
    {
        KeyframeSettings   settings;
        std::vector<Byte>  state (s_kThumbStateBytes);
        HRESULT            hr = S_OK;
        size_t             i  = 0;



        settings.intervalCycles = s_kThumbInterval;
        store.Configure (settings);

        for (i = 0; i < count; i++)
        {
            state[0] = static_cast<Byte> (i + 1);

            hr = store.Add (i * s_kThumbStride, i * s_kThumbInterval, state);
            AssertSucceeded (hr, L"Add");
        }
    }


    static void Prepare (HistoryThumbnails & thumbnails, InlineWorkQueue & queue, KeyframeStore & store, uint64_t & now)
    {
        Fill (store, 9);

        thumbnails.SetWorkQueue  (&queue);
        thumbnails.SetClock      ([&now] { return (double) now; });
        thumbnails.SetCellLayout (3, s_kThumbCell);
    }


    //  A budget the arena binds, unlike Fill's, so placing the keyframes in
    //  flight drops the oldest groups.
    static void PrepareTight (HistoryThumbnails & thumbnails, InlineWorkQueue & thumbQueue, KeyframeStore & store, InlineWorkQueue & storeQueue, uint64_t & now)
    {
        constexpr uint32_t  kWholeEvery   = 2;
        constexpr size_t    kBudgetStates = 64;

        KeyframeSettings  settings;



        settings.intervalCycles = s_kThumbInterval;
        settings.wholeEvery     = kWholeEvery;
        settings.budgetBytes    = s_kNoiseStateBytes * kBudgetStates;

        store.Configure    (settings);
        store.SetWorkQueue (&storeQueue);

        thumbnails.SetWorkQueue  (&thumbQueue);
        thumbnails.SetClock      ([&now] { return (double) now; });
        thumbnails.SetCellLayout (3, s_kThumbCell);
    }


    //  Adds a keyframe for every work buffer and leaves them all in flight.
    //  Noise does not pack, so the arena fills; the first byte is the tag
    //  GetTag gives for the keyframe's position.
    static void AddInFlight (KeyframeStore & store, uint64_t & position, uint32_t & seed)
    {
        constexpr uint32_t  kLcgMultiplier = 1664525u;
        constexpr uint32_t  kLcgIncrement  = 1013904223u;
        constexpr int       kLcgHighByte   = 24;

        std::vector<Byte>  state (s_kNoiseStateBytes);
        HRESULT            hr = S_OK;
        size_t             i  = 0;
        size_t             j  = 0;



        for (j = 0; j < KeyframeStore::kBufferCount; j++)
        {
            for (i = 0; i < state.size(); i++)
            {
                seed     = seed * kLcgMultiplier + kLcgIncrement;
                state[i] = static_cast<Byte> (seed >> kLcgHighByte);
            }

            state[0] = GetTag (position);

            hr = store.Add (position, position / s_kThumbStride * s_kThumbInterval, state);
            AssertSucceeded (hr, L"Add");

            position += s_kThumbStride;
        }
    }


    //  The pool packs the keyframes in flight, as it does well within a
    //  frame; collecting them, which can drop the oldest groups, is left to
    //  the next copy out of the store.
    static void FinishPacking (InlineWorkQueue & storeQueue)
    {
        while (storeQueue.TryRunNext())
        {
        }
    }


    static Byte GetTag (uint64_t position)
    {
        return static_cast<Byte> (position / s_kThumbStride + 1);
    }
};
