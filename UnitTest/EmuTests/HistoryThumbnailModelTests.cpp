#include "Pch.h"

#include "HResultAssert.h"
#include "InlineWorkQueue.h"
#include "Debugger/Reverse/HistoryImageCache.h"
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
        outWidth  = s_kFakeWidth;
        outHeight = s_kFakeHeight;
        outBgra.assign ((size_t) s_kFakeWidth * s_kFakeHeight, kOpaque | state[0]);
        return S_OK;
    }

    int  calls = 0;
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
        Assert::AreEqual<uint64_t> (300,  cells[0].position, L"the oldest point left at the left");
        Assert::AreEqual<uint64_t> (600,  cells[1].position, L"the others moved one cell left, each its keyframe");
        Assert::AreEqual<uint64_t> (900,  cells[2].position, L"and a new one came in at the right");
        Assert::AreEqual<uint64_t> (1000, cells[3].position, L"the live end follows the newest keyframe");
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


    TEST_METHOD (DrawingIsBoundedAndOnTheWorker)
    {
        FakeHistoryFrameRenderer  renderer;
        InlineWorkQueue           queue;
        HistoryThumbnails         thumbnails (renderer);
        KeyframeStore             store;
        uint64_t                  now     = 0;
        uint64_t                  spacing = 1000 / HistoryThumbnails::kRendersPerSecond;
        HRESULT                   hr      = S_OK;



        Prepare (thumbnails, queue, store, now);
        thumbnails.SetVisible (true);

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

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service too soon");
        Assert::AreEqual<size_t> (0, queue.GetPendingCount(), L"too soon after the last: nothing more");

        now += spacing;

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service once due");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"due again: the next picture");
        Assert::IsTrue (queue.TryRunNext(), L"drawn");

        Assert::IsNotNull (thumbnails.GetCellImage (1).get(), L"then the points, newest first");
        Assert::AreEqual ((int) s_kThumbCell.cx, thumbnails.GetCellImage (1)->width,  L"a thumbnail is the cell's width");
        Assert::AreEqual ((int) s_kThumbCell.cy, thumbnails.GetCellImage (1)->height, L"and the cell's height");
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


    //  The keyframes in flight are collected when one is unpacked, and
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

            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service");

            Assert::IsTrue (thumbQueue.TryRunNext(), L"a picture was handed to the worker");

            thumbnails.GetCells (cells);
            image = thumbnails.GetCellImage (kLiveCell);

            Assert::IsNotNull (image.get(), L"the live end is drawn");
            Assert::AreEqual<uint32_t> (s_kThumbOpaque | GetTag (cells[kLiveCell].position), image->bgraPremul[0], L"from the keyframe at the live end");

            now += 1000 / HistoryThumbnails::kRendersPerSecond;
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

        now += 1000 / HistoryThumbnails::kRendersPerSecond;

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service once due");

        Assert::IsTrue (thumbQueue.TryRunNext(), L"the strip moved on to a point history still holds");
        Assert::AreEqual (1, renderer.calls, L"drawn");
    }


    //  Once history is full every new keyframe drops the oldest, and the
    //  keyframes come faster than pictures are drawn. The points must hold
    //  still while history scrolls, so each step brings at most a few new
    //  points at the right, and every point gets its own picture.
    TEST_METHOD (TheStripHoldsStillWhileHistoryScrolls)
    {
        constexpr size_t  kRounds         = 400;
        constexpr size_t  kWarmUpRounds   = 100;
        constexpr int     kCount          = 8;
        constexpr int     kLiveCell       = kCount - 1;
        constexpr size_t  kMaxNewPerRound = 2;

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
        HRESULT                            hr       = S_OK;



        PrepareTight (thumbnails, thumbQueue, store, storeQueue, now);
        thumbnails.SetCellLayout (kCount, s_kThumbCell);
        thumbnails.SetVisible (true);

        for (round = 0; round < kRounds; round++)
        {
            AddInFlight (store, position, seed);

            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service");

            thumbQueue.TryRunNext();
            now += 1000 / HistoryThumbnails::kRendersPerSecond;

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

            for (i = 0; i < kLiveCell; i++)
            {
                image = thumbnails.GetCellImage ((int) i);

                Assert::IsNotNull (image.get(), L"every point is drawn");
                Assert::AreEqual<uint32_t> (s_kThumbOpaque | GetTag (cells[i].position), image->bgraPremul[0], L"with its own picture");
            }

            Assert::IsNotNull (thumbnails.GetCellImage (kLiveCell).get(), L"the live end is drawn");
        }

        Assert::IsTrue (store.GetInfo (0).position > dropped, L"the oldest kept being dropped");
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

            hr = thumbnails.Service (store);
            AssertSucceeded (hr, L"Service");

            if (isDrawn)
            {
                Assert::IsNotNull (thumbnails.GetCellImage (kLiveCell).get(), L"the live cell keeps its picture while the next is drawn");
            }

            thumbQueue.TryRunNext();
            now += 1000 / HistoryThumbnails::kRendersPerSecond;

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


    //  New points after a resize are drawn faster than the bound that holds
    //  while history scrolls, so the strip settles quickly.
    TEST_METHOD (NewPointsAfterAResizeAreDrawnFasterThanTheBound)
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

        now += 1000 / HistoryThumbnails::kCatchUpRendersPerSecond;

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service at the catch-up rate");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"the next new point follows at the faster rate");
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

private:

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
            now += 1000 / HistoryThumbnails::kRendersPerSecond;
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
        thumbnails.SetClock      ([&now] { return now; });
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
        thumbnails.SetClock      ([&now] { return now; });
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


    static Byte GetTag (uint64_t position)
    {
        return static_cast<Byte> (position / s_kThumbStride + 1);
    }
};
