#include "Pch.h"

#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/IHistoryFrameRenderer.h"
#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::HistoryThumbnails
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::HistoryThumbnails (IHistoryFrameRenderer & renderer) :
    m_renderer (renderer)
{
    m_job.owner = this;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::~HistoryThumbnails
//
//  The picture in flight draws into this object, so it finishes first.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::~HistoryThumbnails()
{
    WaitForWork();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::SetWorkQueue
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetWorkQueue (IWorkQueue * queue)
{
    WaitForWork();

    m_queue = queue;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::WaitForWork
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::WaitForWork()
{
    if (m_queue != nullptr)
    {
        m_queue->WaitAll();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetCells
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::GetCells (std::vector<HistoryThumbnailCell> & outCells) const
{
    std::lock_guard<std::mutex>  held (m_lock);



    outCells = m_cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::SetCellLayout
//
//  A new cell size outdates every thumbnail, though not the base copies they
//  are scaled from; a new count moves the points, so what each cell last
//  showed belongs to another. The caches hold twice the cells shown, so
//  scrolling history keeps the ones just passed.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetCellLayout (
    int   count,
    SIZE  cellPx)
{
    std::lock_guard<std::mutex>  held (m_lock);
    bool                         isNewSize  = cellPx.cx != m_cellPx.cx || cellPx.cy != m_cellPx.cy;
    bool                         isNewCount = count != m_count;
    size_t                       capacity   = 0;



    if (!isNewSize && !isNewCount)
    {
        return;
    }

    if (isNewSize)
    {
        m_cellPx = cellPx;
        m_layoutId++;
        m_thumbs.Clear();
    }

    m_count       = (std::max) (count, 0);
    capacity      = (std::max) (kMinThumbCount, (size_t) m_count * 2);
    m_useStandIns = m_useStandIns || m_bases.GetCount() > 0;

    m_thumbs.SetCapacity (capacity);
    m_bases.SetCapacity  (capacity);
    m_shown.assign ((size_t) m_count, ShownPicture {});

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetCellImage
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image HistoryThumbnails::GetCellImage (int index)
{
    std::lock_guard<std::mutex>  held (m_lock);



    if (index < 0 || (size_t) index >= m_cells.size())
    {
        return nullptr;
    }

    return ResolveShown ((size_t) index).image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::ResolveShown
//
//  Under the lock: the picture cell `index` shows and the snapshot it is
//  of. A thumbnail missing at this cell size is scaled from its point's base
//  copy and kept. A cell whose point is not drawn yet keeps the picture it
//  last showed, so it never goes blank between one point and the next. With
//  none, once a layout has changed over pictures already drawn, it shows the
//  nearest point drawn, scaled once and kept as shown, so a resize never
//  empties the strip while the new points are drawn.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::ShownPicture HistoryThumbnails::ResolveShown (size_t index)
{
    const HistoryThumbnailCell  & cell    = m_cells[index];
    Image                         image;
    uint64_t                      nearest = 0;
    bool                          isNear  = false;
    PointMap::const_iterator      found;



    if (m_shown.size() < m_cells.size())
    {
        m_shown.resize (m_cells.size());
    }

    image = m_thumbs.Find (cell.position);

    if (image == nullptr)
    {
        image = ScaleBase (cell.position);

        if (image != nullptr)
        {
            m_thumbs.Put (cell.position, image);
        }
    }

    if (image != nullptr)
    {
        m_shown[index] = ShownPicture { image, cell };
        return m_shown[index];
    }

    if (m_shown[index].image != nullptr || !m_useStandIns)
    {
        return m_shown[index];
    }

    isNear = m_bases.TryFindNearest (cell.position, nearest);
    found  = isNear ? m_points.find (nearest) : m_points.end();

    if (found != m_points.end())
    {
        m_shown[index] = ShownPicture { ScaleBase (nearest), found->second };
    }

    return m_shown[index];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetPreviewImage
//
//  The full-size picture of the snapshot the cell shows, so the preview and
//  the cell's labels agree with its picture; while the cell still shows an
//  older snapshot, that one's when there is one to hand. The cell's own
//  point not drawn at full size yet is asked for, ahead of every thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image HistoryThumbnails::GetPreviewImage (int index)
{
    std::lock_guard<std::mutex>  held (m_lock);
    ShownPicture                 shown;
    uint64_t                     position = 0;



    if (index < 0 || (size_t) index >= m_cells.size())
    {
        return nullptr;
    }

    shown    = ResolveShown ((size_t) index);
    position = m_cells[(size_t) index].position;

    if (!m_previews.Contains (position))
    {
        m_wantedPreview = position;
    }

    return m_previews.Find ((shown.image != nullptr) ? shown.point.position : position);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::OnCellClicked
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::OnCellClicked (int index)
{
    HistoryThumbnailCell  cell;
    bool                  isCell = false;



    {
        std::lock_guard<std::mutex>  held (m_lock);

        isCell = index >= 0 && (size_t) index < m_cells.size();

        if (isCell)
        {
            cell = m_cells[(size_t) index];
        }
    }

    if (isCell && m_onSeek)
    {
        m_onSeek (cell);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetTrailingTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryThumbnails::GetTrailingTip()
{
    return IsBehindLive() ? L"Return to live" : L"Running live";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetLeadingTip
//
//  A click on the start time seeks to the first cell, the oldest keyframe
//  held.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryThumbnails::GetLeadingTip()
{
    return L"Go to the start of history";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::OnTrailingLabelClicked
//
//  Replaying, Live goes live, as a seek to the live end does; live, it has
//  nowhere to go.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::OnTrailingLabelClicked()
{
    HistoryThumbnailCell  live;



    if (!IsBehindLive() || !m_onSeek)
    {
        return;
    }

    live.isLive = true;

    m_onSeek (live);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::Service
//
//  Lays the points out again, and while the strip shows and nothing is being
//  drawn, copies the keyframe the next wanted picture comes from out of the
//  store, still packed, and hands it to the worker to unpack and draw: a
//  full-size picture for the pointer at once, a thumbnail once IsRenderDue.
//  Copying is the only part on this thread, and it never waits for a
//  keyframe being packed; one still in flight is asked for again next turn.
//  Copying first collects the keyframes in flight, which can drop the
//  oldest; a point dropped that way is forgotten, the points are laid out
//  again over what history holds now, and the next wanted picture is tried
//  in the same turn, so the first cell moves to the new oldest keyframe
//  without waiting a turn for every drop. The worker's unpacker is read
//  here only while nothing is in flight.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HistoryThumbnails::Service (KeyframeStore & keyframes)
{
    constexpr int  kAttempts = 2;



    HRESULT       hr        = S_OK;
    uint64_t      position  = 0;
    size_t        index     = 0;
    KeyframeCopy  copy      = KeyframeCopy::Gone;
    bool          isPicked  = false;
    bool          isIdle    = false;
    bool          isPreview = false;
    bool          isDue     = false;
    bool          isHeld    = false;
    int           attempt   = 0;



    LayOutCells (keyframes);

    isIdle = IsVisible() && !m_isInFlight.load (std::memory_order_acquire);
    BAIL_OUT_IF (!isIdle, S_OK);

    m_pendingPick.reset();

    for (attempt = 0; attempt < kAttempts && copy != KeyframeCopy::Copied; attempt++)
    {
        isPicked = TryPickWanted (position, isPreview);

        if (!isPicked)
        {
            break;
        }

        //  Only the thumbnails keep the pace; a full-size picture for the
        //  pointer goes now.
        isDue = isPreview || !m_hasSubmitted || IsRenderDue (GetNowMs(), m_lastSubmitMs, GetLastRenderMs());

        if (!isDue)
        {
            break;
        }

        hr = keyframes.CopyPacked (position, m_unpacker, m_job.packed, copy);
        CHR (hr);

        if (copy == KeyframeCopy::Pending)
        {
            m_pendingPick = position;
        }
        else if (copy == KeyframeCopy::Gone)
        {
            ForgetPoint (position);
            LayOutCells (keyframes);
        }
    }

    BAIL_OUT_IF (copy != KeyframeCopy::Copied, S_OK);

    //  The snapshot the picture is of, so a cell showing it gives its time.
    isHeld = TryFindAtOrBefore (keyframes, position, index);
    CBRA (isHeld);

    m_job.point.position = keyframes.GetInfo (index).position;
    m_job.point.cycle    = keyframes.GetInfo (index).cycle;
    m_job.point.wallTime = keyframes.GetInfo (index).wallTime;

    if (m_queue == nullptr)
    {
        hr = m_ownQueue.Create (1);
        CHRA (hr);

        m_queue = &m_ownQueue;
    }

    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_job.thumbPx  = m_cellPx;
        m_job.layoutId = m_layoutId;
    }

    m_job.position  = position;
    m_job.isPreview = isPreview;

    m_isInFlight.store (true, std::memory_order_release);
    m_lastSubmitMs = GetNowMs();
    m_hasSubmitted = true;

    hr = m_queue->Submit (&RunJob, &m_job);

    if (FAILED (hr))
    {
        m_isInFlight.store (false, std::memory_order_release);
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::LayOutCells
//
//  The points over the keyframes held, published only when they changed,
//  and the clock times of the keyframes, for the time under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::LayOutCells (const KeyframeStore & keyframes)
{
    std::vector<HistoryThumbnailCell>  cells;
    int                                count = 0;



    m_wallTimes.Sync (keyframes);

    {
        std::lock_guard<std::mutex>  held (m_lock);

        count = m_count;
    }

    PlanCells (keyframes, count, m_step, cells);

    {
        std::lock_guard<std::mutex>  held (m_lock);

        if (cells.size() != m_cells.size() || !std::equal (cells.begin(), cells.end(), m_cells.begin(),
                                                           [] (const HistoryThumbnailCell & a, const HistoryThumbnailCell & b)
                                                           { return a.position == b.position && a.isLive == b.isLive; }))
        {
            m_cells = std::move (cells);
            Bump();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::Clear
//
//  For a history begun again: every point and picture belongs to the old
//  one.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::Clear()
{
    WaitForWork();

    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_cells.clear();
        m_shown.clear();
        m_thumbs.Clear();
        m_bases.Clear();
        m_previews.Clear();
        m_points.clear();
        m_wantedPreview.reset();

        m_useStandIns = false;
    }

    m_wallTimes.Clear();

    m_step = 0;

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::PlanCells
//
//  The first cell is the oldest keyframe held and the last the live end,
//  the newest, so the strip always runs from where history begins to where
//  it ends. The cells between are the grid points below the live end, one
//  step apart and ending at the last multiple of the step before it, each
//  the keyframe at or before it; a point older than history shows the
//  oldest keyframe. Because the grid is fixed to absolute positions, a point
//  keeps its keyframe until that keyframe is dropped. A step whose grid
//  reaches back past the second keyframe is chosen afresh.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::PlanCells (
    const KeyframeStore                & keyframes,
    int                                  count,
    uint64_t                           & ioStep,
    std::vector<HistoryThumbnailCell>  & outCells)
{
    size_t    held   = keyframes.GetCount();
    uint64_t  oldest = 0;
    uint64_t  newest = 0;
    uint64_t  top    = 0;
    uint64_t  back   = 0;
    size_t    index  = 0;
    int       i      = 0;



    outCells.clear();

    if (held == 0 || count <= 0)
    {
        return;
    }

    oldest = keyframes.GetInfo (0).position;
    newest = keyframes.GetInfo (held - 1).position;
    ioStep = ChooseStep (newest - oldest, count, ioStep);
    top    = GetGridTop (newest, ioStep);

    //  A step kept from a narrower strip or a longer history can reach back
    //  past the oldest keyframe, which would put the first grid points on
    //  the oldest keyframe beside the first cell; the step is chosen afresh
    //  instead, so every cell has a point of its own.
    if (!DoesGridFit (keyframes, count, ioStep, top))
    {
        ioStep = ChooseStep (newest - oldest, count, 0);
        top    = GetGridTop (newest, ioStep);
    }

    outCells.resize ((size_t) count);

    for (i = 0; i < count - 1; i++)
    {
        back = (uint64_t) (count - 2 - i) * ioStep;

        if (i == 0 || back > top || !TryFindAtOrBefore (keyframes, top - back, index))
        {
            index = 0;
        }

        outCells[(size_t) i].position = keyframes.GetInfo (index).position;
        outCells[(size_t) i].cycle    = keyframes.GetInfo (index).cycle;
        outCells[(size_t) i].wallTime = keyframes.GetInfo (index).wallTime;
    }

    outCells.back().position = newest;
    outCells.back().cycle    = keyframes.GetInfo (held - 1).cycle;
    outCells.back().wallTime = keyframes.GetInfo (held - 1).wallTime;
    outCells.back().isLive   = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::ChooseStep
//
//  The step that spreads `count` cells over `span`: the grid cells cover
//  count - 1 steps. The step in use is kept while the span stays within a
//  quarter of the one it was chosen for, so the small swings of a full
//  history -- a group dropped at the old end, keyframes added at the new --
//  never move the grid; past that it is chosen afresh.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryThumbnails::ChooseStep (
    uint64_t  span,
    int       count,
    uint64_t  step)
{
    constexpr uint64_t  kLow   = 3;
    constexpr uint64_t  kHigh  = 5;
    constexpr uint64_t  kParts = 4;



    uint64_t  ideal = (std::max) ((uint64_t) 1, span / (uint64_t) (std::max) (count - 1, 1));



    if (step == 0 || ideal * kParts < step * kLow || ideal * kParts >= step * kHigh)
    {
        return ideal;
    }

    return step;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetGridTop
//
//  The last multiple of the step before the live end, where the grid's
//  newest point lies.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryThumbnails::GetGridTop (
    uint64_t  newest,
    uint64_t  step)
{
    return (newest > 0) ? (newest - 1) / step * step : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::DoesGridFit
//
//  Whether the oldest grid point, the second cell's, lies at or past the
//  second keyframe held, so it is not the oldest keyframe the first cell
//  already shows. With fewer than three cells there are no grid points, and
//  with one keyframe every cell shows it.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::DoesGridFit (
    const KeyframeStore  & keyframes,
    int                    count,
    uint64_t               step,
    uint64_t               top)
{
    constexpr int     kOffGrid  = 2;    // the first cell and the live end are not on the grid
    constexpr size_t  kSecond   = 1;



    size_t    held = keyframes.GetCount();
    uint64_t  back = 0;



    if (count <= kOffGrid || held <= kSecond)
    {
        return true;
    }

    back = (uint64_t) (count - kOffGrid - 1) * step;

    return back <= top && top - back >= keyframes.GetInfo (kSecond).position;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TryFindAtOrBefore
//
//  The newest keyframe taken at or before `position`; positions rise with
//  the index.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryFindAtOrBefore (
    const KeyframeStore  & keyframes,
    uint64_t               position,
    size_t               & outIndex)
{
    size_t  low  = 0;
    size_t  high = keyframes.GetCount();
    size_t  mid  = 0;



    outIndex = 0;

    if (high == 0 || keyframes.GetInfo (0).position > position)
    {
        return false;
    }

    while (high - low > 1)
    {
        mid = low + (high - low) / 2;

        if (keyframes.GetInfo (mid).position <= position)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }

    outIndex = low;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::Shrink
//
//  A box filter: every destination pixel averages the source pixels whose
//  centers fall in its share of the picture, channel by channel.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::Shrink (
    const DxuiIconImage  & source,
    int                    width,
    int                    height,
    DxuiIconImage        & outImage)
{
    constexpr int  kChannels = 4;



    int       dx             = 0;
    int       dy             = 0;
    int       sx             = 0;
    int       sy             = 0;
    int       x0             = 0;
    int       x1             = 0;
    int       y0             = 0;
    int       y1             = 0;
    int       c              = 0;
    uint64_t  sum[kChannels] = {};
    uint64_t  n              = 0;
    uint32_t  pixel          = 0;
    uint32_t  out            = 0;



    outImage.width  = (std::max) (width, 0);
    outImage.height = (std::max) (height, 0);
    outImage.bgraPremul.assign ((size_t) outImage.width * (size_t) outImage.height, 0);

    if (source.width <= 0 || source.height <= 0)
    {
        return;
    }

    for (dy = 0; dy < outImage.height; dy++)
    {
        y0 = dy * source.height / outImage.height;
        y1 = (std::max) (y0 + 1, (dy + 1) * source.height / outImage.height);

        for (dx = 0; dx < outImage.width; dx++)
        {
            x0 = dx * source.width / outImage.width;
            x1 = (std::max) (x0 + 1, (dx + 1) * source.width / outImage.width);
            n   = 0;
            out = 0;

            std::fill (std::begin (sum), std::end (sum), 0);

            for (sy = y0; sy < y1; sy++)
            {
                for (sx = x0; sx < x1; sx++)
                {
                    pixel = source.bgraPremul[(size_t) sy * (size_t) source.width + (size_t) sx];

                    for (c = 0; c < kChannels; c++)
                    {
                        sum[c] += (pixel >> (CHAR_BIT * c)) & 0xFFu;
                    }

                    n++;
                }
            }

            for (c = 0; c < kChannels; c++)
            {
                out |= (uint32_t) ((sum[c] + n / 2) / n) << (CHAR_BIT * c);
            }

            outImage.bgraPremul[(size_t) dy * (size_t) outImage.width + (size_t) dx] = out;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::RunJob
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::RunJob (void * context)
{
    Job  * job = static_cast<Job *> (context);



    job->owner->Draw (*job);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::Draw
//
//  Worker thread. The keyframe is unpacked here, then drawn. A snapshot that
//  cannot be unpacked or drawn still files a picture, an empty one, so it is
//  not asked for again and again; the strip shows its cell empty. The
//  thumbnail is scaled from the base copy, as every later cell size will be;
//  one drawn for a cell size since replaced is dropped, and the base copy
//  kept. A full-size picture drawn only for a thumbnail is kept in room to
//  spare, never in place of one the pointer wanted. What the whole took
//  paces the next thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::Draw (Job & job)
{
    HRESULT                         hr    = S_OK;
    std::shared_ptr<DxuiIconImage>  full  = std::make_shared<DxuiIconImage>();
    std::shared_ptr<DxuiIconImage>  thumb = std::make_shared<DxuiIconImage>();
    Image                           base;
    double                          start = GetNowMs();



    hr = m_unpacker.Unpack (job.packed, job.state);

    if (SUCCEEDED (hr))
    {
        hr = m_renderer.Render (job.state, full->bgraPremul, full->width, full->height);
    }

    if (FAILED (hr))
    {
        full->width  = 0;
        full->height = 0;
        full->bgraPremul.clear();
    }

    base = MakeBase (full);

    Shrink (*base, (base->width > 0) ? job.thumbPx.cx : 0, (base->height > 0) ? job.thumbPx.cy : 0, *thumb);

    {
        std::lock_guard<std::mutex>  held (m_lock);

        if (job.layoutId == m_layoutId)
        {
            m_thumbs.Put (job.position, thumb);
        }

        m_bases.Put (job.position, base);
        m_points[job.position] = job.point;
        PrunePoints();

        if (job.isPreview)
        {
            m_previews.Put (job.position, full);
        }
        else
        {
            m_previews.PutSpare (job.position, full);
        }

        if (m_wantedPreview == job.position)
        {
            m_wantedPreview.reset();
        }
    }

    m_lastRenderMs.store (GetNowMs() - start, std::memory_order_relaxed);
    m_renderCount.fetch_add (1, std::memory_order_relaxed);
    m_isInFlight.store (false, std::memory_order_release);

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TryPickWanted
//
//  The full-size pictures around the pointer first (outIsPreview), then the
//  live end if it has never had a picture, then the first cell, which moves
//  to the new oldest keyframe each time the oldest are dropped and must show
//  where history begins, then the points newest first, where history
//  scrolling brings new ones in, and then the live end's latest. The live
//  end moves on with every keyframe and keeps its last picture meanwhile, so
//  redrawing it first every time would leave the points waiting for as long
//  as the machine runs.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryPickWanted (
    uint64_t  & outPosition,
    bool      & outIsPreview)
{
    std::lock_guard<std::mutex>  held (m_lock);
    bool                         hasCellSize = m_cellPx.cx > 0 && m_cellPx.cy > 0;
    uint64_t                     live        = 0;
    bool                         isLiveShown = false;
    bool                         hasLivePast = false;
    size_t                       i           = 0;



    outPosition  = 0;
    outIsPreview = TryPickFull (outPosition);

    if (outIsPreview)
    {
        return true;
    }

    if (!hasCellSize || m_cells.empty())
    {
        return false;
    }

    live        = m_cells.back().position;
    isLiveShown = HasPicture (live);
    hasLivePast = m_shown.size() == m_cells.size() && m_shown.back().image != nullptr;

    if (!isLiveShown && !hasLivePast && m_pendingPick != live)
    {
        outPosition = live;
        return true;
    }

    if (IsWanted (m_cells.front().position))
    {
        outPosition = m_cells.front().position;
        return true;
    }

    for (i = m_cells.size() - 1; i-- > 0; )
    {
        if (IsWanted (m_cells[i].position))
        {
            outPosition = m_cells[i].position;
            return true;
        }
    }

    if (!isLiveShown && m_pendingPick != live)
    {
        outPosition = live;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TryPickFull
//
//  Under the lock: the next full-size picture the pointer wants. The one
//  asked for first, then the cell under the pointer and the cells nearest
//  it out to kPrefetchReach either side, so a move to a neighbor finds its
//  picture ready.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryPickFull (uint64_t & outPosition)
{
    int  count    = (int) m_cells.size();
    int  distance = 0;



    if (m_wantedPreview.has_value() && !m_previews.Contains (*m_wantedPreview) && m_pendingPick != m_wantedPreview)
    {
        outPosition = *m_wantedPreview;
        return true;
    }

    if (m_hoveredCell < 0 || m_hoveredCell >= count)
    {
        return false;
    }

    for (distance = 0; distance <= (int) kPrefetchReach; distance++)
    {
        for (int index : { m_hoveredCell + distance, m_hoveredCell - distance })
        {
            if (index < 0 || index >= count || m_previews.Contains (m_cells[(size_t) index].position) || m_pendingPick == m_cells[(size_t) index].position)
            {
                continue;
            }

            outPosition = m_cells[(size_t) index].position;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::ForgetPoint
//
//  The keyframe at position is gone from history. A preview asked for there
//  is dropped, or it would be picked again every turn; the cells are laid
//  out again over what history still holds on the next turn.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::ForgetPoint (uint64_t position)
{
    std::lock_guard<std::mutex>  held (m_lock);



    if (m_wantedPreview == position)
    {
        m_wantedPreview.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::IsRenderDue
//
//  The pace follows the cost: a picture that took lastWorkMs is followed no
//  sooner than kWorkShare times that after it went, so the worker draws at
//  most one part in kWorkShare of the time. A fast computer is held only by
//  the one picture a turn Service hands over; a slow one, or a Debug build,
//  draws as fast as that share allows rather than at a rate chosen for
//  either.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::IsRenderDue (
    double  nowMs,
    double  lastSubmitMs,
    double  lastWorkMs)
{
    return nowMs - lastSubmitMs >= lastWorkMs * kWorkShare;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetNowMs
//
//  Milliseconds, finer than a tick: a picture takes well under one.
//
////////////////////////////////////////////////////////////////////////////////

double HistoryThumbnails::GetNowMs() const
{
    constexpr double  kMsPerSecond = 1000.0;



    LARGE_INTEGER  now  = {};
    LARGE_INTEGER  freq = {};



    if (m_clock)
    {
        return m_clock();
    }

    QueryPerformanceFrequency (&freq);
    QueryPerformanceCounter   (&now);

    return (double) now.QuadPart * kMsPerSecond / (double) freq.QuadPart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::HasPicture
//
//  Under the lock. A point with a base copy needs no drawing: any cell size
//  is scaled from it.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::HasPicture (uint64_t position) const
{
    return m_thumbs.Contains (position) || m_bases.Contains (position);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::IsWanted
//
//  Under the lock: a point with no picture yet, unless its keyframe is still
//  being packed this turn, when the next one wanted goes instead.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::IsWanted (uint64_t position) const
{
    return !HasPicture (position) && m_pendingPick != position;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::ScaleBase
//
//  Under the lock: the base copy of the point at the cell size, or null when
//  the point has none.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image HistoryThumbnails::ScaleBase (uint64_t position)
{
    Image                           base  = m_bases.Find (position);
    std::shared_ptr<DxuiIconImage>  thumb;
    bool                            isSet = base != nullptr && base->width > 0 && base->height > 0;



    if (base == nullptr)
    {
        return nullptr;
    }

    thumb = std::make_shared<DxuiIconImage>();

    Shrink (*base, isSet ? (int) m_cellPx.cx : 0, isSet ? (int) m_cellPx.cy : 0, *thumb);

    return thumb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::PrunePoints
//
//  Under the lock: the snapshots of base copies no longer held are
//  forgotten, as nothing can stand in with their pictures any more.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::PrunePoints()
{
    std::erase_if (m_points, [this] (const PointMap::value_type & entry) { return !m_bases.Contains (entry.first); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::MakeBase
//
//  A whole factor keeps the box filter's boxes the same size everywhere.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image HistoryThumbnails::MakeBase (const Image & full)
{
    std::shared_ptr<DxuiIconImage>  base;
    int                             factor = 0;



    if (full->width <= kBaseMaxWidth)
    {
        return full;
    }

    factor = (full->width + kBaseMaxWidth - 1) / kBaseMaxWidth;
    base   = std::make_shared<DxuiIconImage>();

    Shrink (*full, full->width / factor, (std::max) (1, full->height / factor), *base);

    return base;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::SetPlayhead
//
//  Machine thread, every turn: where the machine stands, and whether that
//  is behind live.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetPlayhead (
    uint64_t  position,
    bool      isBehindLive)
{
    m_playhead.store     (position,     std::memory_order_release);
    m_isBehindLive.store (isBehindLive, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::SetPlayheadTime
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetPlayheadTime (
    uint64_t  cycle,
    uint64_t  wallTime,
    uint64_t  endCycle)
{
    m_cycle.store    (cycle,    std::memory_order_release);
    m_wallTime.store (wallTime, std::memory_order_release);
    m_endCycle.store (endCycle, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::SetBegin
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetBegin (
    uint64_t  cycle,
    uint64_t  wallTime)
{
    m_beginCycle.store (cycle,    std::memory_order_release);
    m_beginWall.store  (wallTime, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::NoteSeekLanded
//
//  Machine thread, after SetPlayheadTime has given where the seek landed, so
//  the UI thread, seeing the landing, reads that cycle and not the one
//  before it.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::NoteSeekLanded (uint64_t cycle)
{
    m_landedCycle.store (cycle, std::memory_order_release);
    m_landings.fetch_add (1, std::memory_order_acq_rel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TryGetPlayhead
//
//  Behind live, the line stands where the machine does among the points,
//  labeled by the labeler. Live, there is none and the live end is marked.
//  A line dragged and let go stands where it was dropped, labeled for that
//  point, until the seek there lands, so it never springs back to where
//  the machine stood before; the hold is checked first, so a landing seen
//  comes with the cycle it landed on.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryGetPlayhead (
    float         & outOffset,
    std::wstring  & outTop,
    std::wstring  & outBottom)
{
    bool      isHeld = IsHoldingDrop();
    uint64_t  cycle  = isHeld ? m_heldCycle : m_cycle.load (std::memory_order_acquire);
    uint64_t  wall   = isHeld ? m_wallTimes.GetWallTimeAt (cycle) : m_wallTime.load (std::memory_order_acquire);



    if (!IsBehindLive())
    {
        return false;
    }

    {
        std::lock_guard<std::mutex>  held (m_lock);

        outOffset = GetPlayheadOffset (m_cells, cycle, m_endCycle.load (std::memory_order_acquire));
    }

    if (outOffset < 0.0f)
    {
        return false;
    }

    outTop.clear();
    outBottom.clear();

    if (m_labeler)
    {
        m_labeler (cycle, wall, outTop, outBottom);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::IsHoldingDrop
//
//  UI thread: whether the playhead line still stands where it was dragged.
//  The hold ends once a seek to that cycle has run since the line last
//  moved; a seek elsewhere landing late, as one made while the line was
//  still moving, does not end it. A seek that never runs, as one dropped
//  with the machine it was for, ends it after kSeekPatienceMs.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::IsHoldingDrop()
{
    uint64_t  landings  = 0;
    bool      hasLanded = false;
    bool      isOverdue = false;



    if (!m_isHeld)
    {
        return false;
    }

    landings  = m_landings.load (std::memory_order_acquire);
    hasLanded = landings != m_heldLandings && m_landedCycle.load (std::memory_order_acquire) == m_heldCycle;
    isOverdue = GetNowMs() - m_heldAtMs >= kSeekPatienceMs;

    m_isHeld = !hasLanded && !isOverdue;

    return m_isHeld;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::OnPlayheadDragged
//
//  The line is held where it was dragged, from this move on, until a seek
//  there lands.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::OnPlayheadDragged (
    float  offset,
    bool   isFinal)
{
    uint64_t  cycle = 0;



    {
        std::lock_guard<std::mutex>  held (m_lock);

        cycle = GetCycleAtOffset (m_cells, offset, m_endCycle.load (std::memory_order_acquire));
    }

    m_isHeld       = true;
    m_heldCycle    = cycle;
    m_heldLandings = m_landings.load (std::memory_order_acquire);
    m_heldAtMs     = GetNowMs();

    if (m_onScrub)
    {
        m_onScrub (cycle, isFinal);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetLeadingLabel
//
//  Where history begins, by the labeler's top line, which moves on as the
//  oldest history is dropped.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryThumbnails::GetLeadingLabel()
{
    std::wstring  top;
    std::wstring  bottom;



    if (m_labeler)
    {
        m_labeler (m_beginCycle.load (std::memory_order_acquire), m_beginWall.load (std::memory_order_acquire), top, bottom);
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetMarkedCell
//
////////////////////////////////////////////////////////////////////////////////

int HistoryThumbnails::GetMarkedCell()
{
    std::lock_guard<std::mutex>  held (m_lock);



    return FindPlayheadCell (m_cells, m_playhead.load (std::memory_order_acquire), IsBehindLive());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::FindPlayheadCell
//
//  Live, the live end. Behind live, the last cell whose point is at or
//  before where the machine stands, which is the oldest when it stands
//  before them all.
//
////////////////////////////////////////////////////////////////////////////////

int HistoryThumbnails::FindPlayheadCell (
    const std::vector<HistoryThumbnailCell>  & cells,
    uint64_t                                   position,
    bool                                       isBehindLive)
{
    int  marked = 0;
    int  i      = 0;



    if (cells.empty())
    {
        return -1;
    }

    if (!isBehindLive)
    {
        return (int) cells.size() - 1;
    }

    for (i = 0; i < (int) cells.size(); i++)
    {
        if (cells[(size_t) i].position <= position)
        {
            marked = i;
        }
    }

    return marked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetPlayheadOffset
//
////////////////////////////////////////////////////////////////////////////////

float HistoryThumbnails::GetPlayheadOffset (
    const std::vector<HistoryThumbnailCell>  & cells,
    uint64_t                                   cycle,
    uint64_t                                   endCycle)
{
    size_t    count = cells.size();
    size_t    i     = 0;
    uint64_t  from  = 0;
    uint64_t  to    = 0;



    if (count == 0)
    {
        return -1.0f;
    }

    if (cycle <= cells[0].cycle)
    {
        return 0.0f;
    }

    while (i + 1 < count && cells[i + 1].cycle <= cycle)
    {
        i++;
    }

    from = cells[i].cycle;
    to   = (i + 1 < count) ? cells[i + 1].cycle : endCycle;

    if (to <= from)
    {
        return (float) i;
    }

    return (float) i + (float) (std::min (cycle, to) - from) / (float) (to - from);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetCycleAtOffset
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryThumbnails::GetCycleAtOffset (
    const std::vector<HistoryThumbnailCell>  & cells,
    float                                      offset,
    uint64_t                                   endCycle)
{
    size_t    count = cells.size();
    size_t    i     = 0;
    float     part  = 0.0f;
    uint64_t  from  = 0;
    uint64_t  to    = 0;



    if (count == 0)
    {
        return 0;
    }

    if (offset <= 0.0f)
    {
        return cells[0].cycle;
    }

    if (offset >= (float) count)
    {
        return std::max (endCycle, cells[count - 1].cycle);
    }

    i    = (size_t) offset;
    part = offset - (float) i;
    from = cells[i].cycle;
    to   = (i + 1 < count) ? cells[i + 1].cycle : endCycle;

    if (to <= from)
    {
        return from;
    }

    return from + (uint64_t) std::llround ((double) part * (double) (to - from));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetModeText
//
////////////////////////////////////////////////////////////////////////////////

PCWSTR HistoryThumbnails::GetModeText (bool isBehindLive)
{
    return isBehindLive ? L"Replay" : L"Live";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::SetHoveredCell
//
//  The cells around the one under the pointer are drawn at full size next,
//  so a move to one of them shows it sharp at once.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetHoveredCell (int index)
{
    std::lock_guard<std::mutex>  held (m_lock);



    m_hoveredCell = index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TryGetCellLabels
//
//  The labeler's lines for the time the snapshot the cell shows was taken,
//  as the playhead line's are for where the machine stands. That is the
//  cell's own keyframe once it is drawn; until then, the snapshot of the
//  picture the cell still shows, so the time matches the picture.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryGetCellLabels (
    int             index,
    std::wstring  & outTop,
    std::wstring  & outBottom)
{
    HistoryThumbnailCell  cell;
    ShownPicture          shown;
    bool                  isCell = false;



    {
        std::lock_guard<std::mutex>  held (m_lock);

        isCell = index >= 0 && (size_t) index < m_cells.size();

        if (isCell)
        {
            shown = ResolveShown ((size_t) index);
            cell  = (shown.image != nullptr) ? shown.point : m_cells[(size_t) index];
        }
    }

    outTop.clear();
    outBottom.clear();

    if (!isCell || !m_labeler)
    {
        return false;
    }

    m_labeler (cell.cycle, cell.wallTime, outTop, outBottom);

    return !outTop.empty() || !outBottom.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TryGetLabelsAt
//
//  The labeler's lines for the point under the pointer: its own cycle, on
//  the scale the cells and the playhead line use, and the host's clock at
//  that cycle, so they read as the playhead's will once the machine stands
//  there. The picture under the pointer is still the cell's snapshot; only
//  the time is the pointer's.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryGetLabelsAt (
    int             index,
    float           offset,
    std::wstring  & outTop,
    std::wstring  & outBottom)
{
    HistoryThumbnailCell  point;
    bool                  isPoint = false;



    UNREFERENCED_PARAMETER (index);

    isPoint = TryGetPointAt (offset, point);

    outTop.clear();
    outBottom.clear();

    if (!isPoint || !m_labeler)
    {
        return false;
    }

    m_labeler (point.cycle, m_wallTimes.GetWallTimeAt (point.cycle), outTop, outBottom);

    return !outTop.empty() || !outBottom.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::OnStripClicked
//
//  A seek to the cycle under the pointer; the trailing end, where history
//  ends, is the live end.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::OnStripClicked (
    int    index,
    float  offset)
{
    HistoryThumbnailCell  point;
    bool                  isPoint = false;



    UNREFERENCED_PARAMETER (index);

    isPoint = TryGetPointAt (offset, point);

    if (isPoint && m_onSeek)
    {
        m_onSeek (point);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TryGetPointAt
//
//  The cycle at an offset along the strip, and whether it is where history
//  ends. False with no cells.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryGetPointAt (
    float                   offset,
    HistoryThumbnailCell  & outPoint)
{
    std::lock_guard<std::mutex>  held (m_lock);
    uint64_t                     end  = m_endCycle.load (std::memory_order_acquire);



    outPoint = HistoryThumbnailCell();

    if (m_cells.empty())
    {
        return false;
    }

    outPoint.cycle  = GetCycleAtOffset (m_cells, offset, end);
    outPoint.isLive = outPoint.cycle >= std::max (end, m_cells.back().cycle);

    return true;
}





