#include "Pch.h"

#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::SetMarks
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetMarks (std::vector<HistoryMark> marks)
{
    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_marks = std::move (marks);
    }

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetMarks
//
//  Each mark at its cycle's offset along the cells, with its glyph and a tip
//  of its text and the labeler's top line there, the host's clock when the
//  machine has one. A mark outside the cells is left out.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::GetMarks (std::vector<DxuiStripMark> & outMarks)
{
    std::vector<HistoryMark>  marks;
    DxuiStripMark             strip;
    std::wstring              top;
    std::wstring              bottom;
    uint64_t                  endCycle = m_endCycle.load (std::memory_order_acquire);



    outMarks.clear();

    {
        std::lock_guard<std::mutex>  held (m_lock);

        for (const HistoryMark & mark : m_marks)
        {
            strip.offset = GetPlayheadOffset (m_cells, mark.cycle, endCycle);

            if (strip.offset < 0.0f)
            {
                continue;
            }

            strip.glyph   = GetMarkGlyph (mark.kind);
            strip.count   = mark.count;
            strip.id      = mark.position;
            strip.hasRows = mark.kind == HistoryMarkKind::Fork;
            strip.tip     = GetMarkText (mark.kind);

            outMarks.push_back (strip);
            marks.push_back    (mark);
        }
    }

    for (size_t i = 0; i < marks.size() && m_labeler; i++)
    {
        m_labeler (marks[i].cycle, m_wallTimes.GetWallTimeAt (marks[i].cycle), top, bottom);

        if (!top.empty())
        {
            outMarks[i].tip += L" at " + top;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetMarkGlyph
//
////////////////////////////////////////////////////////////////////////////////

wchar_t HistoryThumbnails::GetMarkGlyph (HistoryMarkKind kind)
{
    PCWSTR  glyph = s_kpszMdl2Refresh;



    switch (kind)
    {
        case HistoryMarkKind::Gap:           glyph = s_kpszMdl2Remove;   break;
        case HistoryMarkKind::Reset:         glyph = s_kpszMdl2Refresh;  break;
        case HistoryMarkKind::PowerCycle:    glyph = s_kpszMdl2Power;    break;
        case HistoryMarkKind::DiskChange:    glyph = s_kpszMdl2Save;     break;
        case HistoryMarkKind::WriteProtect:  glyph = s_kpszMdl2Lock;     break;
        case HistoryMarkKind::StateLoad:     glyph = s_kpszMdl2OpenFile; break;
        case HistoryMarkKind::Fork:          glyph = s_kpszMdl2Shuffle;  break;
    }

    return glyph[0];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetMarkText
//
////////////////////////////////////////////////////////////////////////////////

PCWSTR HistoryThumbnails::GetMarkText (HistoryMarkKind kind)
{
    PCWSTR  text = L"Reset";



    switch (kind)
    {
        case HistoryMarkKind::Gap:           text = L"Recording paused";         break;
        case HistoryMarkKind::Reset:         text = L"Reset";                    break;
        case HistoryMarkKind::PowerCycle:    text = L"Power cycle";              break;
        case HistoryMarkKind::DiskChange:    text = L"Disk changed";             break;
        case HistoryMarkKind::WriteProtect:  text = L"Write protection changed"; break;
        case HistoryMarkKind::StateLoad:     text = L"State loaded";             break;
        case HistoryMarkKind::Fork:          text = L"Other branches";           break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::SetBranches
//
//  Machine thread. The pictures of branches no longer in the store go.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::SetBranches (std::vector<KeyframeBranch> branches)
{
    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_branches = std::move (branches);

        std::erase_if (m_branchPictures, [this] (const auto & each) { return !HasBranch (each.first); });
        std::erase_if (m_wantedBranches, [this] (uint32_t id)       { return !HasBranch (id); });
    }

    Bump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::HasBranch
//
//  Under the lock.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnails::HasBranch (uint32_t id) const
{
    return std::any_of (m_branches.begin(), m_branches.end(), [id] (const KeyframeBranch & each) { return each.id == id; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetForkRows
//
//  A row for each branch that forks from the active path at the mark's
//  position, labeled by the row labeler. A row with no picture yet adds its
//  branch to the worker's list.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnails::GetForkRows (
    uint64_t                         markId,
    std::vector<DxuiStripForkRow>  & outRows)
{
    std::vector<KeyframeBranch>  branches;
    DxuiStripForkRow             row;



    outRows.clear();

    {
        std::lock_guard<std::mutex>  held (m_lock);

        for (const KeyframeBranch & branch : m_branches)
        {
            auto  picture = m_branchPictures.find (branch.id);



            if (branch.parent != 0 || branch.forkPosition != markId)
            {
                continue;
            }

            row.id      = branch.id;
            row.picture = (picture != m_branchPictures.end()) ? picture->second : nullptr;

            if (row.picture == nullptr)
            {
                m_wantedBranches.insert (branch.id);
            }

            outRows.push_back (row);
            branches.push_back (branch);
        }
    }

    for (size_t i = 0; i < outRows.size() && m_rowLabeler; i++)
    {
        m_rowLabeler (branches[i], outRows[i].ranFor, outRows[i].leftAt);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::GetBranchPicture
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails::Image HistoryThumbnails::GetBranchPicture (uint32_t branchId)
{
    std::lock_guard<std::mutex>  held (m_lock);
    auto                         picture = m_branchPictures.find (branchId);



    return (picture != m_branchPictures.end()) ? picture->second : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnails::TrySubmitBranch
//
//  Machine thread: the live end of a branch on the list goes to the worker,
//  at full size. A branch no longer in the store leaves the list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HistoryThumbnails::TrySubmitBranch (
    KeyframeStore  & keyframes,
    bool           & outIsSubmitted)
{
    HRESULT       hr       = S_OK;
    uint32_t      id       = 0;
    bool          isWanted = false;
    KeyframeCopy  copy     = KeyframeCopy::Gone;



    outIsSubmitted = false;

    {
        std::lock_guard<std::mutex>  held (m_lock);

        isWanted = !m_wantedBranches.empty();
        id       = isWanted ? *m_wantedBranches.begin() : 0;
    }

    BAIL_OUT_IF (!isWanted, S_OK);

    hr = keyframes.CopyBranchEnd (id, m_unpacker, m_job.packed, copy);
    CHR (hr);

    if (copy != KeyframeCopy::Copied)
    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_wantedBranches.erase (id);
        BAIL_OUT_IF (true, S_OK);
    }

    m_job.position  = 0;
    m_job.point     = HistoryThumbnailCell();
    m_job.isPreview = true;
    m_job.branchId  = id;

    hr = Submit();
    CHR (hr);

    outIsSubmitted = true;

Error:
    return hr;
}
