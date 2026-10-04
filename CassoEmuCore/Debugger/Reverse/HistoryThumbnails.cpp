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
//  A new cell size outdates every thumbnail; the cache holds twice the cells
//  shown, so scrolling history keeps the ones just passed.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetCellLayout (
    int   count,
    SIZE  cellPx)
{
    std::lock_guard<std::mutex>  held (m_lock);
    bool                         isNewSize  = cellPx.cx != m_cellPx.cx || cellPx.cy != m_cellPx.cy;
    bool                         isNewCount = count != m_count;



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

    m_count = (std::max) (count, 0);
    m_thumbs.SetCapacity ((std::max) (kMinThumbCount, (size_t) m_count * 2));

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

    return m_thumbs.Find (m_cells[(size_t) index].position);
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

    PlanCells (keyframes, count, cells);

    {
        std::lock_guard<std::mutex>  held (m_lock);

        KeepPlan (m_cells, cells, cells);

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
        m_thumbs.Clear();
        m_previews.Clear();
        m_wantedPreview.reset();
    }

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::PlanCells
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::PlanCells (
    const KeyframeStore                & keyframes,
    int                                  count,
    std::vector<HistoryThumbnailCell>  & outCells)
{
    size_t    held   = keyframes.GetCount();
    uint64_t  oldest = 0;
    uint64_t  newest = 0;
    size_t    index  = 0;
    int       i      = 0;



    outCells.clear();

    if (held == 0 || count <= 0)
    {
        return;
    }

    oldest = keyframes.GetInfo (0).position;
    newest = keyframes.GetInfo (held - 1).position;

    outCells.resize ((size_t) count);

    for (i = 0; i < count; i++)
    {
        if (!TryFindAtOrBefore (keyframes, GetTarget (oldest, newest, i, count), index))
        {
            index = 0;
        }

        outCells[(size_t) i].position = keyframes.GetInfo (index).position;
        outCells[(size_t) i].isLive   = i == count - 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::KeepPlan
//
//  While the machine runs the live end moves every frame, and planning
//  afresh each time would move every point with it, so no picture would be
//  drawn before its point had moved on. The points stay where they are,
//  the live end following the newest keyframe, until it has moved a whole
//  step past where it was planned or the plan no longer fits the history
//  held: another cell count, the first point dropped from history, or the
//  live end moved back, as a seek that truncates the future does.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::KeepPlan (
    const std::vector<HistoryThumbnailCell>  & previous,
    const std::vector<HistoryThumbnailCell>  & fresh,
    std::vector<HistoryThumbnailCell>        & outCells)
{
    size_t                             count  = previous.size();
    uint64_t                           step   = 0;
    bool                               isKept = false;
    std::vector<HistoryThumbnailCell>  kept;



    if (count >= 2 && fresh.size() == count)
    {
        step   = (previous.back().position - previous.front().position) / (count - 1);
        isKept = previous.front().position >= fresh.front().position &&
                 fresh.back().position     >= previous.back().position &&
                 fresh.back().position     <  previous.back().position + (std::max) (step, (uint64_t) 1);
    }

    if (!isKept)
    {
        if (&outCells != &fresh)
        {
            outCells = fresh;
        }

        return;
    }

    kept                 = previous;
    kept.back().position = fresh.back().position;
    outCells             = std::move (kept);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetTarget
//
//  Point `index` of `count`, evenly spaced from oldest to newest, both ends
//  included. A single point is the newest.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryThumbnails::GetTarget (
    uint64_t  oldest,
    uint64_t  newest,
    int       index,
    int       count)
{
    uint64_t  span = (newest > oldest) ? newest - oldest : 0;



    if (count <= 1)
    {
        return newest;
    }

    return oldest + (uint64_t) ((double) span * (double) index / (double) (count - 1));
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
//  cell empty. A thumbnail drawn for a cell size since replaced is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::Draw (Job & job)
{
    HRESULT                         hr    = S_OK;
    std::shared_ptr<DxuiIconImage>  full  = std::make_shared<DxuiIconImage>();
    std::shared_ptr<DxuiIconImage>  thumb = std::make_shared<DxuiIconImage>();
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

    Shrink (*full, (full->width > 0) ? job.thumbPx.cx : 0, (full->height > 0) ? job.thumbPx.cy : 0, *thumb);

    QueryPerformanceCounter (&end);

    {
        std::lock_guard<std::mutex>  held (m_lock);

        if (job.layoutId == m_layoutId)
        {
            m_thumbs.Put (job.position, thumb);
        }

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
//  The preview the pointer rests on first, then the live end, then the
//  points oldest first.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::TryPickWanted (uint64_t & outPosition)
{
    std::lock_guard<std::mutex>  held (m_lock);
    bool                         hasCellSize = m_cellPx.cx > 0 && m_cellPx.cy > 0;



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

    if (!m_thumbs.Contains (m_cells.back().position))
    {
        outPosition = m_cells.back().position;
        return true;
    }

    for (const HistoryThumbnailCell & cell : m_cells)
    {
        if (!m_thumbs.Contains (cell.position))
        {
            outPosition = cell.position;
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
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::IsRenderDue()
{
    constexpr uint64_t  kMsPerSecond = 1000;



    uint64_t  now = GetNowMs();



    return !m_hasSubmitted || now - m_lastSubmitMs >= kMsPerSecond / kRendersPerSecond;
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
