#pragma once

#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/KeyHintLine.h"





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
//  While the machine is stopped, the instructions it runs next follow the
//  last entry, with no entry number, cycles or registers.
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

    //  What a key pressed in the pane does.
    enum class KeyAction
    {
        None,
        StepInto,
        StepOver,
        StepOut,
        Run,
        ToggleTrace,
        ToggleBytes,
        Save,
    };

    static KeyAction                       GetKeyAction (WPARAM vk, bool ctrl, bool alt, bool shift);
    static std::vector<KeyHintLine::Pair>  GetKeyPairs  ();
    static std::wstring                    GetKeyHint   ();

    void  ToggleBytes    ();
    void  SetColors      (const DebuggerTextColors::Set & colors) { m_colors = colors; }
    void  ProvideRow     (int row, std::vector<DxuiListView::Cell> & out) const;

    //  A row's address, bytes, label and instruction in the disassembly's
    //  colors; the entry number and cycles in the muted one.
    static void  AddColors (std::vector<DxuiListView::Cell> & row, const DebuggerTextColors::Set & colors);
    bool  IsShowingBytes () const { return m_showBytes; }

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

    DxuiListView             * m_list      = nullptr;
    MoveFn                     m_move;
    uint64_t                   m_first     = 0;
    uint64_t                   m_total     = 0;
    std::vector<TraceRecord>   m_entries;
    std::vector<TraceRecord>   m_next;
    bool                       m_showBytes = true;
    std::optional<uint64_t>    m_requested = kFollowEnd;
    DebuggerTextColors::Set    m_colors;
};
