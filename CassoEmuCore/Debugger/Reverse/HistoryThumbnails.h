#pragma once

#include "Pch.h"

#include "Core/IWorkQueue.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/Reverse/HistoryImageCache.h"

class IHistoryFrameRenderer;
class KeyframeStore;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailCell
//
//  One point a history strip shows: the keyframe it is drawn from, by its
//  position, and whether it stands for the live end.
//
////////////////////////////////////////////////////////////////////////////////

struct HistoryThumbnailCell
{
    uint64_t  position = 0;
    bool      isLive   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails
//
//  Pictures of the screen at evenly spaced points of reverse execution's
//  history, for a strip of them: the oldest point first, the live end last.
//  Each point is the keyframe at or before its even share of the way from
//  the oldest keyframe to the newest, so its picture is that keyframe's
//  screen exactly, and seeking to its position lands on what it shows. The
//  live end is drawn from the newest keyframe.
//
//  Threads. The strip's side (IDxuiImageStripSource, SetVisible) runs on the
//  UI thread. Service runs on the thread that runs the machine, once a
//  frame: it lays the points out over the keyframes, and while the strip is
//  visible unpacks at most one keyframe a turn, and at most kRendersPerSecond
//  a second, which a worker then draws through an IHistoryFrameRenderer.
//  Nothing is drawn while the strip is hidden or while one is being drawn.
//
//  Each picture is drawn at full size; the strip keeps a copy at its cell
//  size for every point it has drawn, the most recently used first out, and
//  the full-size ones for the last few points the pointer rested on.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryThumbnails : public IDxuiImageStripSource
{
public:
    using SeekFn  = std::function<void (const HistoryThumbnailCell & cell)>;
    using ClockFn = std::function<uint64_t()>;

    static constexpr int     kRendersPerSecond = 8;
    static constexpr size_t  kPreviewCount     = 8;
    static constexpr size_t  kMinThumbCount    = 64;

                       HistoryThumbnails  (IHistoryFrameRenderer & renderer);
                       ~HistoryThumbnails () override;

    HistoryThumbnails             (const HistoryThumbnails &) = delete;
    HistoryThumbnails & operator= (const HistoryThumbnails &) = delete;

    //  Production runs the drawing on a pool thread of its own; a test hands
    //  in a queue it runs itself, and a clock it moves itself.
    void               SetWorkQueue (IWorkQueue * queue);
    void               SetClock     (ClockFn clock)  { m_clock = std::move (clock); }
    void               SetOnSeek    (SeekFn onSeek)  { m_onSeek = std::move (onSeek); }

    //  UI thread.
    void               SetVisible   (bool visible)   { m_isVisible.store (visible, std::memory_order_release); }
    bool               IsVisible    () const         { return m_isVisible.load (std::memory_order_acquire); }
    uint64_t           GetVersion   () const         { return m_version.load (std::memory_order_acquire); }
    void               GetCells     (std::vector<HistoryThumbnailCell> & outCells) const;

    void               SetCellLayout   (int count, SIZE cellPx) override;
    Image              GetCellImage    (int index) override;
    Image              GetPreviewImage (int index) override;
    void               OnCellClicked   (int index) override;

    //  Machine thread.
    HRESULT            Service      (KeyframeStore & keyframes);
    void               Clear        ();
    void               WaitForWork  ();

    //  The time one picture took to draw, unpacking included, and how many
    //  have been drawn.
    double             GetLastRenderMs () const      { return m_lastRenderMs.load (std::memory_order_relaxed); }
    uint64_t           GetRenderCount  () const      { return m_renderCount.load (std::memory_order_relaxed); }

    //  The points for `count` cells over the keyframes held, oldest first.
    static void        PlanCells     (const KeyframeStore & keyframes, int count, std::vector<HistoryThumbnailCell> & outCells);
    static uint64_t    GetTarget     (uint64_t oldest, uint64_t newest, int index, int count);
    static void        KeepPlan      (const std::vector<HistoryThumbnailCell> & previous,
                                      const std::vector<HistoryThumbnailCell> & fresh,
                                      std::vector<HistoryThumbnailCell>       & outCells);
    static bool        TryFindAtOrBefore (const KeyframeStore & keyframes, uint64_t position, size_t & outIndex);

    //  A picture shrunk to `width` by `height`, each pixel the average of
    //  the ones it covers.
    static void        Shrink        (const DxuiIconImage & source, int width, int height, DxuiIconImage & outImage);

private:
    //  The one picture in flight.
    struct Job
    {
        HistoryThumbnails      * owner     = nullptr;
        std::vector<Byte>        state;
        uint64_t                 position  = 0;
        SIZE                     thumbPx   = {};
        uint64_t                 layoutId  = 0;
        double                   unpackMs  = 0.0;
    };

    static void        RunJob        (void * context);
    void               Draw          (Job & job);
    bool               TryPickWanted (uint64_t & outPosition);
    bool               IsRenderDue   ();
    uint64_t           GetNowMs      () const;
    void               Bump          ()              { m_version.fetch_add (1, std::memory_order_acq_rel); }

    IHistoryFrameRenderer           & m_renderer;
    ThreadPoolWorkQueue               m_ownQueue;
    IWorkQueue                      * m_queue        = nullptr;
    ClockFn                           m_clock;
    SeekFn                            m_onSeek;

    std::atomic<bool>                 m_isVisible    = false;
    std::atomic<uint64_t>             m_version      = 0;
    std::atomic<bool>                 m_isInFlight   = false;
    std::atomic<double>               m_lastRenderMs = 0.0;
    std::atomic<uint64_t>             m_renderCount  = 0;

    //  Shared between the threads, under the lock.
    mutable std::mutex                m_lock;
    int                               m_count        = 0;
    SIZE                              m_cellPx       = {};
    uint64_t                          m_layoutId     = 0;     // changes with the cell size, which outdates the thumbnails
    std::vector<HistoryThumbnailCell> m_cells;
    HistoryImageCache                 m_thumbs       { kMinThumbCount };
    HistoryImageCache                 m_previews     { kPreviewCount };
    std::optional<uint64_t>           m_wantedPreview;

    //  Machine thread's own.
    Job                               m_job;
    uint64_t                          m_lastSubmitMs = 0;
    bool                              m_hasSubmitted = false;
};
