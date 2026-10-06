#pragma once

#include "Pch.h"

#include "Core/IWorkQueue.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/Reverse/HistoryImageCache.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"

class IHistoryFrameRenderer;
class KeyframeStore;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailCell
//
//  One point a history strip shows: the keyframe it is drawn from, by its
//  position and cycle, the host's clock when it was taken, and whether it
//  stands for the live end.
//
////////////////////////////////////////////////////////////////////////////////

struct HistoryThumbnailCell
{
    uint64_t  position = 0;
    uint64_t  cycle    = 0;
    uint64_t  wallTime = 0;
    bool      isLive   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails
//
//  Pictures of the screen at evenly spaced points of reverse execution's
//  history, for a strip of them: the oldest keyframe held first, the live end
//  last. The points between lie on a grid fixed to absolute positions, every
//  multiple of a step chosen from the span held and the cell count, so while
//  history scrolls -- new keyframes at the live end, the oldest dropped -- a
//  point stays put: cells leave at the left and arrive at the right, and
//  only the new ones need drawing. The step changes only when the span moves
//  well away from what it was chosen for. Each point is the keyframe at or
//  before its grid position, so its picture is that keyframe's screen
//  exactly, and seeking to its position lands on what it shows. The live end
//  is drawn from the newest keyframe.
//
//  Threads. The strip's side (IDxuiImageStripSource, SetVisible) runs on the
//  UI thread. Service runs on the thread that runs the machine, once a
//  frame: it lays the points out over the keyframes, and while the strip is
//  visible copies at most one keyframe a turn out of the store still packed,
//  a few kilobytes, for a worker to unpack and draw through an
//  IHistoryFrameRenderer, so the machine's thread never unpacks or draws.
//  Thumbnails are paced by what they cost: the next goes once the time
//  since the last is kWorkShare times what that one took, so the worker is
//  busy at most a quarter of the time however slow the computer, and on a
//  fast one the strip fills at one picture a frame. Nothing is drawn while
//  the strip is hidden or while one is being drawn.
//
//  Each picture is drawn at full size. The strip keeps, for every point it
//  has drawn, a base copy at a fixed size no wider than kBaseMaxWidth and a
//  copy at its cell size made from that, the most recently used first out.
//  It keeps the full-size pictures of the cells the pointer rested on and of
//  the kPrefetchReach cells either side, and those of other points only in
//  room to spare. The cell under the pointer and then those around it are
//  drawn ahead of every thumbnail and without waiting out the thumbnails'
//  pace, so a preview turns sharp as soon as one picture can be drawn, and
//  at once on a move to a neighbor. A new cell size is met by scaling the
//  base copies again, not by drawing anything again. A cell whose point has
//  no picture yet shows the last picture it showed, so the live end does not
//  blink out each time it moves on. After a resize moves the points, a cell
//  with none shows the nearest point's picture until its own is drawn. Whatever
//  picture a cell shows, its labels and its preview are of that picture's
//  snapshot, so the time shown always matches the screen shown.
//
//  Where the machine stands, live or replaying history, is told to it by
//  the machine thread every turn, so the strip can mark that cell.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryThumbnails : public IDxuiImageStripSource
{
public:
    using SeekFn  = std::function<void (const HistoryThumbnailCell & cell)>;
    using ClockFn = std::function<double()>;
    using LabelFn = std::function<void (uint64_t cycle, uint64_t wallTime, std::wstring & outTop, std::wstring & outBottom)>;
    using ScrubFn = std::function<void (uint64_t cycle, bool isFinal)>;

    static constexpr int     kWorkShare               = 4;
    static constexpr size_t  kPreviewCount            = 16;
    static constexpr size_t  kMinThumbCount           = 64;
    static constexpr int     kBaseMaxWidth            = 192;
    static constexpr size_t  kPrefetchReach           = 2;

                       HistoryThumbnails  (IHistoryFrameRenderer & renderer);
                       ~HistoryThumbnails () override;

    HistoryThumbnails             (const HistoryThumbnails &) = delete;
    HistoryThumbnails & operator= (const HistoryThumbnails &) = delete;

    //  Production runs the drawing on a pool thread of its own; a test hands
    //  in a queue it runs itself, and a clock it moves itself.
    void               SetWorkQueue (IWorkQueue * queue);
    void               SetClock     (ClockFn clock)  { m_clock = std::move (clock); }
    void               SetOnSeek    (SeekFn onSeek)  { m_onSeek = std::move (onSeek); }
    void               SetLabeler   (LabelFn labeler) { m_labeler = std::move (labeler); }
    void               SetOnScrub   (ScrubFn onScrub) { m_onScrub = std::move (onScrub); }

    //  UI thread.
    void               SetVisible   (bool visible)   { m_isVisible.store (visible, std::memory_order_release); }
    bool               IsVisible    () const         { return m_isVisible.load (std::memory_order_acquire); }
    uint64_t           GetVersion   () const         { return m_version.load (std::memory_order_acquire); }
    void               GetCells     (std::vector<HistoryThumbnailCell> & outCells) const;

    void               SetCellLayout   (int count, SIZE cellPx) override;
    Image              GetCellImage    (int index) override;
    Image              GetPreviewImage (int index) override;
    void               OnCellClicked   (int index) override;
    int                GetMarkedCell   () override;
    void               SetHoveredCell  (int index) override;
    bool               TryGetCellLabels (int index, std::wstring & outTop, std::wstring & outBottom) override;
    bool               TryGetPlayhead  (float & outOffset, std::wstring & outTop, std::wstring & outBottom) override;
    void               OnPlayheadDragged (float offset, bool isFinal) override;
    std::wstring       GetLeadingLabel () override;
    std::wstring       GetLeadingTip            () override;
    void               OnLeadingLabelClicked    () override { OnCellClicked (0); }
    std::wstring       GetTrailingLabel         () override { return GetModeText (false); }
    bool               IsTrailingLabelAccented  () override { return !IsBehindLive(); }
    bool               IsTrailingLabelClickable () override { return IsBehindLive(); }
    std::wstring       GetTrailingTip           () override;
    void               OnTrailingLabelClicked   () override;
    bool               IsBehindLive    () const      { return m_isBehindLive.load (std::memory_order_acquire); }

    //  Machine thread.
    void               SetPlayhead  (uint64_t position, bool isBehindLive);

    //  Where the machine stands in cycles and by the host's clock, where
    //  history ends in cycles, and where and when it begins.
    void               SetPlayheadTime (uint64_t cycle, uint64_t wallTime, uint64_t endCycle);
    void               SetBegin        (uint64_t cycle, uint64_t wallTime);
    HRESULT            Service      (KeyframeStore & keyframes);
    void               Clear        ();
    void               WaitForWork  ();

    //  The time one picture took to unpack and draw, and how many have been
    //  drawn.
    double             GetLastRenderMs () const      { return m_lastRenderMs.load (std::memory_order_relaxed); }
    uint64_t           GetRenderCount  () const      { return m_renderCount.load (std::memory_order_relaxed); }

    //  The points for `count` cells over the keyframes held, oldest first, on
    //  the grid of `ioStep`, which is chosen afresh when it no longer suits.
    //  Whether a thumbnail may go to the worker now, at nowMs, after the last
    //  went at lastSubmitMs and took lastWorkMs: so the worker is busy at most
    //  one part in kWorkShare of the time.
    static bool        IsRenderDue   (double nowMs, double lastSubmitMs, double lastWorkMs);

    static void        PlanCells     (const KeyframeStore & keyframes, int count, uint64_t & ioStep, std::vector<HistoryThumbnailCell> & outCells);
    static uint64_t    ChooseStep    (uint64_t span, int count, uint64_t step);
    static bool        TryFindAtOrBefore (const KeyframeStore & keyframes, uint64_t position, size_t & outIndex);

    //  The cell that marks where the machine stands, and what the timeline
    //  calls the state it is in.
    static int         FindPlayheadCell (const std::vector<HistoryThumbnailCell> & cells, uint64_t position, bool isBehindLive);
    static PCWSTR      GetModeText      (bool isBehindLive);

    //  Where the playhead line stands along the strip, in cells from the
    //  leading edge, for the machine at cycle with history ending at endCycle,
    //  or -1 with no cells; the cycle at such an offset; and a cycle moved to
    //  the nearest whole second of emulated time, within first and last.
    static float       GetPlayheadOffset (const std::vector<HistoryThumbnailCell> & cells, uint64_t cycle, uint64_t endCycle);
    static uint64_t    GetCycleAtOffset  (const std::vector<HistoryThumbnailCell> & cells, float offset, uint64_t endCycle);
    static uint64_t    SnapToSecond      (uint64_t cycle, uint64_t cyclesPerSecond, uint64_t first, uint64_t last);

    //  A picture shrunk to `width` by `height`, each pixel the average of
    //  the ones it covers.
    static void        Shrink        (const DxuiIconImage & source, int width, int height, DxuiIconImage & outImage);

    //  The fixed-size copy every cell size is scaled from: the picture itself
    //  when it is no wider than kBaseMaxWidth, else shrunk by a whole factor
    //  to fit.
    static Image       MakeBase      (const Image & full);

private:
    using PointMap = std::unordered_map<uint64_t, HistoryThumbnailCell>;

    //  The picture a cell last showed and the snapshot it is of, which is
    //  not the cell's point while that point is not drawn yet.
    struct ShownPicture
    {
        Image                 image;
        HistoryThumbnailCell  point;
    };

    //  The one picture in flight.
    struct Job
    {
        HistoryThumbnails      * owner     = nullptr;
        PackedKeyframe           packed;
        std::vector<Byte>        state;
        HistoryThumbnailCell     point;
        uint64_t                 position  = 0;
        SIZE                     thumbPx   = {};
        uint64_t                 layoutId  = 0;
        bool                     isPreview = false;   // asked for at full size, not only as a thumbnail
    };

    static void        RunJob        (void * context);
    void               Draw          (Job & job);
    void               LayOutCells   (const KeyframeStore & keyframes);
    bool               TryPickWanted (uint64_t & outPosition, bool & outIsPreview);
    bool               TryPickFull   (uint64_t & outPosition);
    bool               HasPicture    (uint64_t position) const;
    bool               IsWanted      (uint64_t position) const;
    Image              ScaleBase     (uint64_t position);
    ShownPicture       ResolveShown  (size_t index);
    void               PrunePoints   ();
    void               ForgetPoint   (uint64_t position);
    double             GetNowMs      () const;
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
    std::atomic<uint64_t>             m_playhead     = 0;
    std::atomic<bool>                 m_isBehindLive = false;
    std::atomic<uint64_t>             m_cycle        = 0;
    std::atomic<uint64_t>             m_wallTime     = 0;
    std::atomic<uint64_t>             m_endCycle     = 0;
    std::atomic<uint64_t>             m_beginCycle   = 0;
    std::atomic<uint64_t>             m_beginWall    = 0;
    LabelFn                           m_labeler;
    ScrubFn                           m_onScrub;

    //  Shared between the threads, under the lock.
    mutable std::mutex                m_lock;
    int                               m_count        = 0;
    SIZE                              m_cellPx       = {};
    uint64_t                          m_layoutId     = 0;     // changes with the cell size, which outdates the thumbnails
    std::vector<HistoryThumbnailCell> m_cells;
    HistoryImageCache                 m_thumbs       { kMinThumbCount };
    HistoryImageCache                 m_bases        { kMinThumbCount };
    HistoryImageCache                 m_previews     { kPreviewCount };
    std::optional<uint64_t>           m_wantedPreview;
    int                               m_hoveredCell  = -1;
    std::vector<ShownPicture>         m_shown;               // the last picture each cell showed
    PointMap                          m_points;              // the snapshot of every base copy held
    bool                              m_useStandIns  = false; // set once a layout changes over pictures drawn

    //  Machine thread's own.
    Job                               m_job;
    KeyframeUnpacker                  m_unpacker;            // the worker's while a picture is in flight
    double                            m_lastSubmitMs = 0.0;
    std::optional<uint64_t>           m_pendingPick;         // a keyframe still being packed, passed over this turn
    bool                              m_hasSubmitted = false;
    uint64_t                          m_step         = 0;
};
