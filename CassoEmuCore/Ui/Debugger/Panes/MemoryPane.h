#pragma once

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/MemoryEditModel.h"
#include "Widgets/DxuiHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane
//
//  One memory window (FR-034): a hex view over this window's edit model,
//  editable in place, grouped by one, two or four bytes, with its own undo.
//
//  THE VIEW SCROLLS THE WHOLE 64K; THE SNAPSHOT HOLDS A WINDOW OF IT. When the
//  rows on screen leave the bytes the CPU thread last read for this window,
//  the pane asks for a read around them. Until it arrives, rows outside the
//  read show as zero.
//
//  The window owns the view as a child control; the pane holds a pointer to
//  it and the model it reads from.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryPane
{
public:
    using  MoveFn   = std::function<void (int id, Word first)>;
    using  RunFn    = std::function<void (const std::string & line)>;
    using  ActionFn = std::function<void (const DebuggerActionBuilder & build)>;

    MemoryPane (int id, DxuiHexView * view, MoveFn move, ActionFn run, RunFn note);

    //  The view holds a pointer to this pane's model, so a pane never moves.
    MemoryPane (const MemoryPane &)             = delete;
    MemoryPane & operator= (const MemoryPane &) = delete;

    int            GetId    () const { return m_id; }
    DxuiHexView *  GetView  () const { return m_view; }
    int            GetGrouping () const { return m_grouping; }
    int            GetColumns  () const { return m_columns; }

    //  The address of the first byte on screen.
    Word           GetTopAddress () const { return m_model.GetAddressOf (m_view->GetTopRow() * (uint64_t) m_view->GetBytesPerRow()); }

    void  Configure (HWND hwnd);

    //  The ink for a byte that changed since the previous snapshot.
    void  SetChangedColor (uint32_t argb) { m_changedArgb = argb; }

    //  The bytes the snapshot read for this window. The first one places the
    //  view there; later ones leave the view where the user has it.
    void  Apply (const DebuggerViewSnapshot::MemoryWindow & window);

    //  Asks for a read around the rows on screen once they leave the last one.
    void  FollowScroll ();

    //  Scrolls to an address and asks for a read there.
    void  GoTo (Word address);

    //  One, two, four bytes a value, and round again. Returns the new grouping.
    int   CycleGrouping ();

    //  One, two or four bytes a value.
    void  SetGrouping (int bytesPerValue);

    //  Values across a row, or 0 for as many as the pane's width holds.
    void  SetColumns (int valuesPerRow);

    //  Asks again for the bytes the window last read.
    void  Refresh ();

    bool  Undo    () { return m_view->IsEditable() && m_model.Undo(); }
    void  ClearHistory () { m_model.ClearHistory(); }

    //  Where a read should start to hold the rows on screen, or nothing when
    //  the last read already holds them.
    static std::optional<Word>  GetReadStartFor (Word readFirst, uint64_t topOffset, int visibleRows, int bytesPerRow = kBytesPerRow);

    //  The columns a window starts with: sixteen values a row.
    static constexpr int  kDefaultColumns = DebuggerViewState::kMemoryRowBytes;

private:
    static constexpr int  kBytesPerRow = DebuggerViewState::kMemoryRowBytes;

    void  NoteRefusal (uint64_t offset) const;

    int                    m_id          = 0;
    DxuiHexView          * m_view        = nullptr;
    MemoryEditModel        m_model;
    MoveFn                 m_move;
    RunFn                  m_note;
    Word                   m_readFirst   = 0;
    bool                   m_placed      = false;
    int                    m_grouping    = 1;
    int                    m_columns     = kDefaultColumns;
    std::optional<Word>    m_requested;
    uint32_t               m_changedArgb = 0xFFFF6B68;
};
