#pragma once

#include "Ui/Debugger/DebuggerViewState.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TracePane
//
//  The instruction trace (FR-047): a list with one row per retained entry,
//  ending at the newest, that can scroll to any of them.
//
//  THE LIST HAS EVERY ROW; THE SNAPSHOT HOLDS A WINDOW OF THEM. The list is
//  virtual, so it asks this pane for the rows on screen only, and those come
//  from the window of entries the CPU thread last read. When the rows on
//  screen leave that window, the pane asks for a read around them; at the
//  end of the list it asks to follow the newest instead. Until a read
//  arrives, rows outside the window are blank.
//
//  The window owns the list as a child control; the pane holds a pointer to
//  it.
//
////////////////////////////////////////////////////////////////////////////////

class TracePane
{
public:
    //  Where to read from: an entry, or the newest when empty.
    using MoveFn = std::function<void (std::optional<uint64_t> first)>;

    TracePane (DxuiListView * list, MoveFn move) : m_list (list), m_move (std::move (move)) {}

    TracePane (const TracePane &)             = delete;
    TracePane & operator= (const TracePane &) = delete;

    DxuiListView *  GetList () const { return m_list; }

    void  Configure    ();
    void  Apply        (const DebuggerViewSnapshot::TraceState & trace);
    void  FollowScroll ();

    //  What to ask for when rows top to top + visible are on screen and the
    //  window holds held entries from first: kFollowEnd at the end of the
    //  list, an entry a little above top when the rows leave the window, and
    //  nothing when the window holds them.
    static constexpr uint64_t  kFollowEnd = UINT64_MAX;

    static std::optional<uint64_t>  GetReadStartFor (uint64_t first, uint64_t held, uint64_t top, int visible, bool isAtEnd);

private:
    //  Rows kept above the ones on screen when a new read is asked for, so a
    //  short scroll back does not ask again.
    static constexpr uint64_t  kLeadRows = 16;

    void  ProvideRow (int row, std::vector<DxuiListView::Cell> & out) const;

    DxuiListView             * m_list      = nullptr;
    MoveFn                     m_move;
    uint64_t                   m_first     = 0;
    uint64_t                   m_total     = 0;
    std::vector<TraceRecord>   m_entries;
    std::optional<uint64_t>    m_requested = kFollowEnd;
};
