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

    m_isCatchingUp.store (m_useStandIns, std::memory_order_release);

    m_thumbs.SetCapacity (capacity);
    m_bases.SetCapacity  (capacity);
    m_shown.assign ((size_t) m_count, nullptr);

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetCellImage
//
//  A thumbnail missing at this cell size is scaled from its point's base
//  copy and kept. A cell whose point is not drawn yet keeps the picture it
//  last showed, so it never goes blank between one point and the next. With
//  none, once a layout has changed over pictures already drawn, it shows the
//  nearest point drawn, scaled once and kept as shown, so a resize never
//  empties the strip while the new points are drawn.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image HistoryThumbnails::GetCellImage (int index)
{
    std::lock_guard<std::mutex>  held (m_lock);
    Image                        image;
    uint64_t                     position = 0;
    uint64_t                     nearest  = 0;
    bool                         isNear   = false;



    if (index < 0 || (size_t) index >= m_cells.size())
    {
        return nullptr;
    }

    if (m_shown.size() < m_cells.size())
    {
        m_shown.resize (m_cells.size());
    }

    position = m_cells[(size_t) index].position;
    image    = m_thumbs.Find (position);

    if (image == nullptr)
    {
        image = ScaleBase (position);

        if (image != nullptr)
        {
            m_thumbs.Put (position, image);
        }
    }

    if (image == nullptr && m_shown[(size_t) index] == nullptr && m_useStandIns)
    {
        isNear = m_bases.TryFindNearest (position, nearest);
        image  = isNear ? ScaleBase (nearest) : nullptr;
    }

    if (image == nullptr)
    {
        return m_shown[(size_t) index];
    }

    m_shown[(size_t) index] = image;
    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetPreviewImage
//
//  A picture not drawn yet is asked for, ahead of every thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image HistoryThumbnails::GetPreviewImage (int index)
{
    std::lock_guard<std::mutex>  held (m_lock);
    Image                        image;
    uint64_t                     position = 0;



    if (index < 0 || (size_t) index >= m_cells.size())
    {
        return nullptr;
    }

    position = m_cells[(size_t) index].position;
    image    = m_previews.Find (position);

    if (image == nullptr)
    {
        m_wantedPreview = position;
    }

    return image;
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
//  HistoryThumbnails::Service
//
//  Lays the points out again, and while the strip shows and nothing is being
//  drawn, at most kRendersPerSecond times a second, unpacks the keyframe the
//  next wanted picture comes from and hands it to the worker. Unpacking is
//  the only part on this thread.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HistoryThumbnails::Service (KeyframeStore & keyframes)
{
    HRESULT                            hr       = S_OK;
    std::vector<HistoryThumbnailCell>  cells;
    int                                count    = 0;
    uint64_t                           position = 0;
    bool                               isFound  = false;
    bool                               isIdle   = false;
    LARGE_INTEGER                      start    = {};
    LARGE_INTEGER                      end      = {};
    LARGE_INTEGER                      freq     = {};



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

    isIdle = IsVisible() && !m_isInFlight.load (std::memory_order_acquire) && IsRenderDue();
    BAIL_OUT_IF (!isIdle, S_OK);

    isFound = TryPickWanted (position);

    if (!isFound)
    {
        m_isCatchingUp.store (false, std::memory_order_release);
    }

    BAIL_OUT_IF (!isFound, S_OK);

    QueryPerformanceFrequency (&freq);
    QueryPerformanceCounter   (&start);

    hr = keyframes.RestoreAtPosition (position, m_job.state, isFound);
    CHR (hr);

    QueryPerformanceCounter (&end);

    if (!isFound)
    {
        ForgetPoint (position);
        BAIL_OUT_IF (true, S_OK);
    }

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

    m_job.position = position;
    m_job.unpackMs = (double) (end.QuadPart - start.QuadPart) * 1000.0 / (double) freq.QuadPart;

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
        m_wantedPreview.reset();

        m_useStandIns = false;
    }

    m_step = 0;

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::PlanCells
//
//  The last cell is the live end, the newest keyframe. The others are the
//  grid points below it, one step apart and ending at the last multiple of
//  the step before the live end, each the keyframe at or before it; a point
//  older than history shows the oldest keyframe. Because the grid is fixed
//  to absolute positions, a point keeps its keyframe until that keyframe is
//  dropped.
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
    top    = (newest > 0) ? (newest - 1) / ioStep * ioStep : 0;

    outCells.resize ((size_t) count);

    for (i = 0; i < count - 1; i++)
    {
        back = (uint64_t) (count - 2 - i) * ioStep;

        if (back > top || !TryFindAtOrBefore (keyframes, top - back, index))
        {
            index = 0;
        }

        outCells[(size_t) i].position = keyframes.GetInfo (index).position;
    }

    outCells.back().position = newest;
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
//  Worker thread. A snapshot that cannot be drawn still files a picture, an
//  empty one, so it is not asked for again and again; the strip shows its
//  cell empty. The thumbnail is scaled from the base copy, as every later
//  cell size will be; one drawn for a cell size since replaced is dropped,
//  and the base copy kept.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::Draw (Job & job)
{
    HRESULT                         hr    = S_OK;
    std::shared_ptr<DxuiIconImage>  full  = std::make_shared<DxuiIconImage>();
    std::shared_ptr<DxuiIconImage>  thumb = std::make_shared<DxuiIconImage>();
    Image                           base;
    LARGE_INTEGER                   start = {};
    LARGE_INTEGER                   end   = {};
    LARGE_INTEGER                   freq  = {};



    QueryPerformanceFrequency (&freq);
    QueryPerformanceCounter   (&start);

    hr = m_renderer.Render (job.state, full->bgraPremul, full->width, full->height);

    if (FAILED (hr))
    {
        full->width  = 0;
        full->height = 0;
        full->bgraPremul.clear();
    }

    base = MakeBase (full);

    Shrink (*base, (base->width > 0) ? job.thumbPx.cx : 0, (base->height > 0) ? job.thumbPx.cy : 0, *thumb);

    QueryPerformanceCounter (&end);

    {
        std::lock_guard<std::mutex>  held (m_lock);

        if (job.layoutId == m_layoutId)
        {
            m_thumbs.Put (job.position, thumb);
        }

        m_bases.Put (job.position, base);

        m_previews.Put (job.position, full);

        if (m_wantedPreview == job.position)
        {
            m_wantedPreview.reset();
        }
    }

    m_lastRenderMs.store (job.unpackMs + (double) (end.QuadPart - start.QuadPart) * 1000.0 / (double) freq.QuadPart, std::memory_order_relaxed);
    m_renderCount.fetch_add (1, std::memory_order_relaxed);
    m_isInFlight.store (false, std::memory_order_release);

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TryPickWanted
//
//  The preview the pointer rests on first, then the live end if it has never
//  had a picture, then the points newest first, where history scrolling
//  brings new ones in, and then the live end's latest. The live end moves on
//  with every keyframe and keeps its last picture meanwhile, so redrawing it
//  first every time would leave the points waiting for as long as the
//  machine runs.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryPickWanted (uint64_t & outPosition)
{
    std::lock_guard<std::mutex>  held (m_lock);
    bool                         hasCellSize = m_cellPx.cx > 0 && m_cellPx.cy > 0;
    uint64_t                     live        = 0;
    bool                         isLiveShown = false;
    bool                         hasLivePast = false;
    size_t                       i           = 0;



    outPosition = 0;

    if (m_wantedPreview.has_value() && !m_previews.Contains (*m_wantedPreview))
    {
        outPosition = *m_wantedPreview;
        return true;
    }

    if (!hasCellSize || m_cells.empty())
    {
        return false;
    }

    live        = m_cells.back().position;
    isLiveShown = HasPicture (live);
    hasLivePast = m_shown.size() == m_cells.size() && m_shown.back() != nullptr;

    if (!isLiveShown && !hasLivePast)
    {
        outPosition = live;
        return true;
    }

    for (i = m_cells.size() - 1; i-- > 0; )
    {
        if (!HasPicture (m_cells[i].position))
        {
            outPosition = m_cells[i].position;
            return true;
        }
    }

    if (!isLiveShown)
    {
        outPosition = live;
        return true;
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
//  At most kRendersPerSecond while history scrolls. After a new layout over
//  pictures already drawn, until every point has its own, at the faster
//  kCatchUpRendersPerSecond, still one in flight at a time, so a resize
//  settles in a fraction of a second rather than several.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::IsRenderDue()
{
    constexpr uint64_t  kMsPerSecond = 1000;



    uint64_t  now  = GetNowMs();
    uint64_t  rate = m_isCatchingUp.load (std::memory_order_acquire) ? kCatchUpRendersPerSecond : kRendersPerSecond;



    return !m_hasSubmitted || now - m_lastSubmitMs >= kMsPerSecond / rate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryThumbnails::GetNowMs() const
{
    return m_clock ? m_clock() : GetTickCount64();
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
//  HistoryThumbnails::GetModeText
//
////////////////////////////////////////////////////////////////////////////////

PCWSTR HistoryThumbnails::GetModeText (bool isBehindLive)
{
    return isBehindLive ? L"Replay" : L"Live";
}




