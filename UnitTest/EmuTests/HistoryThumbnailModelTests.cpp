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

    TEST_METHOD (TargetsRunEvenlyFromOldestToNewest)
    {
        constexpr uint64_t  kOldest = 1000;
        constexpr uint64_t  kNewest = 5000;
        constexpr int       kCount  = 5;



        Assert::AreEqual<uint64_t> (kOldest, HistoryThumbnails::GetTarget (kOldest, kNewest, 0, kCount), L"the first is the oldest");
        Assert::AreEqual<uint64_t> (2000,    HistoryThumbnails::GetTarget (kOldest, kNewest, 1, kCount), L"evenly spaced");
        Assert::AreEqual<uint64_t> (3000,    HistoryThumbnails::GetTarget (kOldest, kNewest, 2, kCount), L"evenly spaced");
        Assert::AreEqual<uint64_t> (kNewest, HistoryThumbnails::GetTarget (kOldest, kNewest, 4, kCount), L"the last is the newest");
        Assert::AreEqual<uint64_t> (kNewest, HistoryThumbnails::GetTarget (kOldest, kNewest, 0, 1),      L"a single cell is the newest");
    }


    TEST_METHOD (CellsSnapToTheKeyframeAtOrBeforeEachTarget)
    {
        constexpr int                      kCount = 4;
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells;



        Fill (store, 10);

        HistoryThumbnails::PlanCells (store, kCount, cells);

        Assert::AreEqual<size_t>   (kCount, cells.size(), L"one point per cell");
        Assert::AreEqual<uint64_t> (0,      cells[0].position, L"the oldest keyframe at the left");
        Assert::AreEqual<uint64_t> (300,    cells[1].position, L"a third of the way: 300 is a keyframe");
        Assert::AreEqual<uint64_t> (600,    cells[2].position, L"two thirds of the way");
        Assert::AreEqual<uint64_t> (900,    cells[3].position, L"the newest keyframe at the right");
        Assert::IsFalse (cells[2].isLive, L"only the last stands for live");
        Assert::IsTrue  (cells[3].isLive, L"the last stands for live");

        HistoryThumbnails::PlanCells (store, 3, cells);

        Assert::AreEqual<uint64_t> (400, cells[1].position, L"halfway is 450, so the keyframe before it");
    }


    TEST_METHOD (PointsHoldStillWhileTheLiveEndMovesLessThanAStep)
    {
        std::vector<HistoryThumbnailCell>  previous = { { 0, false }, { 400, false }, { 800, true } };
        std::vector<HistoryThumbnailCell>  fresh    = { { 0, false }, { 500, false }, { 1000, true } };
        std::vector<HistoryThumbnailCell>  out;



        fresh.back().position = 1100;
        HistoryThumbnails::KeepPlan (previous, fresh, out);

        Assert::AreEqual<uint64_t> (400,  out[1].position, L"less than a step on: the points stay");
        Assert::AreEqual<uint64_t> (1100, out[2].position, L"and the live end follows the newest keyframe");

        fresh.back().position = 1200;
        HistoryThumbnails::KeepPlan (previous, fresh, out);

        Assert::AreEqual<uint64_t> (500, out[1].position, L"a whole step on: planned afresh");

        fresh = { { 100, false }, { 500, false }, { 900, true } };
        HistoryThumbnails::KeepPlan (previous, fresh, out);

        Assert::AreEqual<uint64_t> (100, out[0].position, L"the first point dropped from history: planned afresh");

        fresh = { { 0, false }, { 300, false }, { 700, true } };
        HistoryThumbnails::KeepPlan (previous, fresh, out);

        Assert::AreEqual<uint64_t> (700, out[2].position, L"the live end moved back: planned afresh");
    }


    TEST_METHOD (NothingIsPlannedWithoutHistoryOrRoom)
    {
        KeyframeStore                      store;
        std::vector<HistoryThumbnailCell>  cells (1);



        HistoryThumbnails::PlanCells (store, 4, cells);
        Assert::IsTrue (cells.empty(), L"no keyframes, no cells");

        Fill (store, 3);

        HistoryThumbnails::PlanCells (store, 0, cells);
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
        Assert::IsNull    (thumbnails.GetCellImage (0).get(), L"the oldest waits its turn");

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service too soon");
        Assert::AreEqual<size_t> (0, queue.GetPendingCount(), L"too soon after the last: nothing more");

        now += spacing;

        hr = thumbnails.Service (store);
        AssertSucceeded (hr, L"Service once due");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"due again: the next picture");
        Assert::IsTrue (queue.TryRunNext(), L"drawn");

        Assert::IsNotNull (thumbnails.GetCellImage (0).get(), L"then the oldest");
        Assert::AreEqual ((int) s_kThumbCell.cx, thumbnails.GetCellImage (0)->width,  L"a thumbnail is the cell's width");
        Assert::AreEqual ((int) s_kThumbCell.cy, thumbnails.GetCellImage (0)->height, L"and the cell's height");
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

private:

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
};
