#pragma once

#include "Debugger/DebugMemoryImage.h"
#include "Debugger/DisassemblyLineList.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"

class DebugSession;
class Disassembler;





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews
//
//  The disassembly views: where each starts, which one follows the PC, how
//  far each has scrolled, and the disassembly of the whole address space
//  they scroll through while the machine is paused.
//
////////////////////////////////////////////////////////////////////////////////

class CodeViews
{
public:
    static constexpr int  kCodeLines = 20;

    //  Up to four disassembly views; the first is always open. Each call
    //  takes a view, the first when it is left out.
    static constexpr int  kMaxCodeViews = 4;

    //  How long a view following a running machine's PC is left where it was
    //  scrolled after the last wheel turn, arrow key or scroll bar move,
    //  before it follows the PC again.
    static constexpr uint64_t  kScrollHoldMs = 5000;

    //  A code view's find searches every line's text, which takes several
    //  milliseconds to make, so it is made only once the machine has been
    //  paused kFindTextIdleMs without a step; until then a find searches the
    //  rows on screen. Whether the text is due now, so the views are built
    //  again to make it; and the delay, which a test sets to 0.
    static constexpr uint64_t  kFindTextIdleMs = 300;
    bool                       IsFindTextDue   (uint64_t nowMs) const;
    void                       SetFindTextIdle (uint64_t ms) { m_findTextIdleMs = ms; }

    using Clock = uint64_t (*)();
    void  SetClock (Clock clock) { m_clock = clock; }

    //  Where a code view starts. The one following the PC follows it unless
    //  the user moved it.
    void  SetCodeAddress   (std::optional<Word> address, int view = 0) { CodeView & v = m_code[(size_t) view]; v.address = address; v.centerOn.reset(); v.scrollLines = 0; }

    //  Moves a code view so `address` is on its middle line, or as near the
    //  middle as the top of memory allows. Every navigation goes through
    //  here, so what was asked for is never on an edge.
    void  CenterCodeOn     (Word address, int view = 0) { m_code[(size_t) view].centerOn = address; m_code[(size_t) view].scrollLines = 0; }
    void  SelectCodeAt     (Word address, int view = 0) { CenterCodeOn (address, view); m_code[(size_t) view].selectOn = address; m_code[(size_t) view].selectSerial++; }

    //  The line list's known starts besides the debug file's and the fetch
    //  bits: the PC, and each call frame's JSR where memory has it.
    static std::vector<Word>  GetKnownStarts   (const CallStackData & calls, Word pc, const DebugMemoryImage & image);
    static std::vector<Word>  GetCodeLabels    (const DebugSession & session);
    static std::vector<Word>  GetBranchTargets (const Disassembler & disassembler, const DebugMemoryImage & image);

    //  Scrolls a code view by instructions, down when positive, up when
    //  negative, through the whole address space.
    void  ScrollCode       (int lines, int view = 0)    { m_code[(size_t) view].scrollLines += lines; m_code[(size_t) view].scrolledAtMs = m_clock(); }

    //  While a view's menu is open, a running machine doesn't move it; once
    //  the menu closes, it stays for kScrollHoldMs, as after a scroll.
    void  HoldCode         (bool isHeld, int view = 0);

    //  Show next statement: the view follows the PC again AND brings it to
    //  the middle, where following alone would leave a PC already on screen
    //  wherever it sat.
    void  ShowPcIn         (int view)                   { CodeView & v = m_code[(size_t) view]; v.address.reset(); v.pinnedAtPc.reset(); v.scrollLines = 0; v.scrolledAtMs.reset(); v.centerOnPc = true; }

    //  How many lines a code view has room for. The window measures it and
    //  sets it; the default is what fits the smallest pane worth having.
    void  SetCodeLines     (int lines, int view = 0);

    //  How many of a view's lines the window has in view, which with source
    //  rows is fewer than its lines: a PC on a line past them isn't in view,
    //  and the middle is the middle of these. Zero until the window has sent it.
    void  SetCodeShown     (int lines, int view = 0)    { m_code[(size_t) view].visible = (std::max) (0, lines); }
    int   GetCodeLines     (int view = 0) const { return m_code[(size_t) view].lines; }

    std::optional<Word>  GetCodeAddress   (int view = 0) const { return m_code[(size_t) view].address; }

    //  Views 2 to 4 (indexes 1 to 3) open centered on an address, and close;
    //  a view closing while it follows the PC passes following to the first.
    void  OpenCodeView     (int view, Word address);

    //  A view reopened as it was left: `top` on its first line, where
    //  OpenCodeView would put the address in the middle.
    void  OpenCodeViewAt   (int view, Word top);
    void  CloseCodeView    (int view);
    bool  IsCodeViewOpen   (int view) const { return view == 0 || m_code[(size_t) view].open; }

    //  Which open view follows the PC; exactly one does. The one giving it up
    //  stays where it was.
    void  SetFollowView    (int view);
    int   GetFollowView    () const { return m_follow; }

    //  The disassembly's lines for the whole address space, built while the
    //  machine is paused and again when what it is built from changes; the
    //  views scroll through it while it is. Each build also makes the lines a
    //  code view's find searches, which a paused snapshot has once they match
    //  the list.
    void  RefreshLineList (DebugSession & session, bool isPaused, const CallStackData & calls) const;
    void  BuildCodeViews  (DebugSession & session, DebuggerViewSnapshot & snapshot) const;

    std::shared_ptr<const std::vector<DebuggerViewSnapshot::CodeLine>>  GetFindLines (bool isPaused) const;

    //  CODE with a place to move a view to: an address, PC, the operand's
    //  address, the return address, or a scroll.
    void  MoveCodePane (DebugSession & session, const std::string & name, Reply & reply);

private:
    //  One disassembly view. `address` pins it; `centerOn` and `scrollLines`
    //  are moves the next build makes. `followAnchor` and `shown` are where a
    //  view following the PC is anchored and the addresses it last showed: it
    //  re-anchors only when the PC walks out of those, since anchoring on the
    //  PC itself re-disassembled from a new address every snapshot, a window
    //  that never stays still. Mutable because a build is const and these are
    //  what it learned while running.
    struct CodeView
    {
        std::optional<Word>      address;
        std::optional<Word>      pinnedAtPc;
        bool                     centerOnPc   = false;
        std::optional<Word>      centerOn;
        std::optional<Word>      selectOn;
        uint32_t                 selectSerial = 0;
        int                      scrollLines  = 0;
        std::optional<uint64_t>  scrolledAtMs;
        bool                     isMenuHeld   = false;
        int                      visible      = 0;
        Word                     followAnchor = 0;
        std::vector<Word>        shown;
        int                      lines        = kCodeLines;
        bool                     open         = false;
    };

    //  Where the code pane starts this build: the pinned address, the anchor
    //  it already had while the PC is among the lines it produced, or a new
    //  anchor that puts the PC in the middle.
    Word                                         ChooseCodeStart (DebugSession & session, Word pc, int view, bool isRunning) const;
    std::vector<DebuggerViewSnapshot::CodeLine>  BuildCode       (DebugSession & session, const DebuggerViewSnapshot & snapshot, int view) const;

    //  An address to disassemble from so that `pc` lands `before` lines in,
    //  or `pc` itself when no such address is found. The 6502 cannot be
    //  disassembled backwards -- an instruction is only where the one before
    //  it ended -- so this walks forward from far enough back and takes the
    //  alignment that reaches `pc` exactly.
    Word  FindStartAbove (DebugSession & session, Word pc, int before) const;

    //  The top line of a pane of `lines` lines moved `count` instructions,
    //  kept within the ends of memory so the pane fills.
    Word  ScrollCodeTop  (DebugSession & session, Word top, int lines, int count) const;

    std::optional<Word>                                                        StepLineList       (Word address, int count) const;
    static std::shared_ptr<const std::vector<DebuggerViewSnapshot::CodeLine>>  MakeFindLines      (DebugSession & session, const DisassemblyLineList & list);
    void                                                                       MakeFindTextIfIdle (DebugSession & session) const;

    mutable std::array<CodeView, kMaxCodeViews>                                 m_code;
    int                                                                         m_follow           = 0;
    mutable std::mutex                                                          m_lineListMutex;
    mutable DisassemblyLineList                                                 m_lineList;
    mutable uint64_t                                                            m_lineListInputs   = 0;
    mutable bool                                                                m_isLineListLive   = false;
    mutable std::shared_ptr<const std::vector<DebuggerViewSnapshot::CodeLine>>  m_findLines;
    mutable uint64_t                                                            m_findLinesVersion = UINT64_MAX;  // the list's version m_findLines was made from
    mutable uint64_t                                                            m_lineListBuiltMs  = 0;
    uint64_t                                                                    m_findTextIdleMs   = kFindTextIdleMs;
    Clock                                                                       m_clock            = [] () -> uint64_t { return GetTickCount64(); };
};
