#include "Pch.h"

#include "Ui/Debugger/CodeViews.h"
#include "Ui/Debugger/CodeAnnotations.h"
#include "Ui/Debugger/DebuggerCommandLines.h"
#include "Ui/Debugger/InstructionLookup.h"

#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"
#include "Disassembler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::BuildCodeViews
//
//  Each disassembly view open, the first always; one of them follows the
//  PC and the rest stay where they were put. One job for every view, since
//  where a view starts depends on where the others are.
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::BuildCodeViews (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    for (int view = 0; view < kMaxCodeViews; view++)
    {
        if (view == 0 || m_code[(size_t) view].open)
        {
            snapshot.codeViews[(size_t) view]  = BuildCode (session, snapshot, view);
            snapshot.codeOpen[(size_t) view]   = true;
            snapshot.codeSelect[(size_t) view]       = m_code[(size_t) view].selectOn;
            snapshot.codeSelectSerial[(size_t) view] = m_code[(size_t) view].selectSerial;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::GetFindLines
//
//  Only a paused machine's, and only once they were made from the line list
//  as it is now.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const std::vector<DebuggerViewSnapshot::CodeLine>> CodeViews::GetFindLines (bool isPaused) const
{
    std::lock_guard<std::mutex>  held (m_lineListMutex);



    return (isPaused && m_findLinesVersion == m_lineList.GetVersion()) ? m_findLines : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::BuildCode
//
//  One disassembly view's lines, from where ChooseCodeStart puts it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerViewSnapshot::CodeLine> CodeViews::BuildCode (DebugSession & session, const DebuggerViewSnapshot & snapshot, int view) const
{
    CodeView                                     & v         = m_code[(size_t) view];
    std::vector<DebuggerViewSnapshot::CodeLine>    lines;
    DisassemblyData                                code;
    std::vector<DisassemblyLine>                 & listed    = code.lines;



    //  As many lines as the pane has room for, stopping at $FFFF rather than
    //  wrapping below the start.
    (void) DataDirectiveHandlers::Disassemble (session, ChooseCodeStart (session, snapshot.pc, view, !snapshot.isPaused), std::nullopt, v.lines, code,
                                               snapshot.isPaused ? std::optional<Word> (snapshot.pc) : std::nullopt);

    v.shown.clear();

    for (const DisassemblyLine & line : listed)
    {
        DebuggerViewSnapshot::CodeLine  row;
        std::string                     bytes;



        for (Byte b : line.instruction.bytes)
        {
            bytes += std::format ("{:02X} ", b);
        }

        row.address       = line.instruction.address;
        row.bytes         = bytes.empty() ? bytes : bytes.substr (0, bytes.size() - 1);
        row.instruction   = line.instruction.operand.empty() ? line.instruction.mnemonic
                                                             : line.instruction.mnemonic + " " + line.GetShownOperand();
        row.label         = line.label;
        row.dataOrigin    = line.dataOrigin;
        row.isCurrent     = row.address == snapshot.pc;
        row.isBranchedFrom = snapshot.branchedFrom == row.address;
        row.isRom          = session.GetTarget().GetRegion (row.address) == MemoryRegion::Rom ||
                             session.GetTarget().GetRegion (row.address) == MemoryRegion::SlotRom;
        row.target        = line.instruction.hasTarget ? std::optional<Word> (line.instruction.target) : std::nullopt;
        //  Only while the machine is paused (FR-110): a running machine
        //  is somewhere else by the time these are drawn. A data line isn't
        //  an instruction, so it doesn't have them.
        //
        //  ONE RUN ANSWERS BOTH COLUMNS. What the instruction reads is
        //  the left annotation and what it leaves is the right one, and
        //  both come from the same trip through the core.
        if (snapshot.isPaused && line.dataOrigin == DataOrigin::None)
        {
            const Cpu6502Registers    & now     = session.GetTarget().GetRegisters();
            InstructionTouches::Result  touches = InstructionTouches::Find (session, session.GetTarget().GetInstructionSet(),
                                                                            now, line.instruction.address,
                                                                            (Word) line.instruction.bytes.size());

            row.annotationParts = CodeAnnotations::GetAnnotationParts (session, line, now, touches);
            row.annotation      = CodeAnnotations::ComposeAnnotation (row.annotationParts, true, false);

            row.effect     = row.isCurrent ? CodeAnnotations::GetEffect (session, now, touches,
                                                                         (Word) (line.instruction.address + line.instruction.bytes.size()))
                                           : std::string();

            if (row.isCurrent && row.target.has_value() && touches.isKnown)
            {
                row.isTargetTaken = touches.after.pc == *row.target;
            }
        }

        if (line.instruction.hasOperandAddress || line.instruction.operand.starts_with ("("))
        {
            row.memoryOperand = line.instruction.operand;
            row.shownOperand  = line.GetShownOperand();
        }

        for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
        {
            if (DebuggerCommandLines::IsCodeBreakpointAt (bp, row.address) || DebuggerCommandLines::IsDiskRoutineAt (bp, row.address))
            {
                row.isEnabled     = row.hasBreakpoint ? (row.isEnabled || bp.enabled) : bp.enabled;
                row.hasBreakpoint = true;
            }
        }

        lines.push_back (row);

        v.shown.push_back (row.address);

        if ((int) lines.size() >= v.lines)
        {
            break;
        }
    }


    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::OpenCodeView
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::OpenCodeView (int view, Word address)
{
    if (view < 1 || view >= kMaxCodeViews)
    {
        return;
    }

    m_code[(size_t) view].open = true;
    SetCodeAddress (std::nullopt, view);
    CenterCodeOn   (address, view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::OpenCodeViewAt
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::OpenCodeViewAt (int view, Word top)
{
    if (view < 1 || view >= kMaxCodeViews)
    {
        return;
    }

    m_code[(size_t) view].open = true;
    SetCodeAddress (top, view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::CloseCodeView
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::CloseCodeView (int view)
{
    if (view < 1 || view >= kMaxCodeViews)
    {
        return;
    }

    m_code[(size_t) view].open = false;

    if (m_follow == view)
    {
        SetFollowView (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::SetFollowView
//
//  The view giving up the PC is pinned where it stands, so it does not jump;
//  the one taking it follows from where it is, re-anchoring only once the PC
//  is off its lines.
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::SetFollowView (int view)
{
    CodeView  & from = m_code[(size_t) m_follow];



    if (view < 0 || view >= kMaxCodeViews || !IsCodeViewOpen (view) || view == m_follow)
    {
        return;
    }

    if (!from.address.has_value())
    {
        from.address = from.shown.empty() ? from.followAnchor : from.shown.front();
    }

    m_follow = view;
    SetCodeAddress (std::nullopt, view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::SetCodeLines
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::SetCodeLines (int lines, int view)
{
    //  A pane too short to hold anything still gets a line; the ceiling keeps
    //  a dragged-tall pane from disassembling half the address space.
    m_code[(size_t) view].lines = (std::clamp) (lines, 1, 200);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::HoldCode
//
//  The top line of a view following the PC is pinned, and ChooseCodeStart
//  returns it until the menu closes and the scroll pause runs out.
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::HoldCode (bool isHeld, int view)
{
    CodeView  & v = m_code[(size_t) view];



    v.isMenuHeld   = isHeld;
    v.scrolledAtMs = m_clock();

    if (isHeld && !v.address.has_value())
    {
        v.address = v.followAnchor;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::ChooseCodeStart
//
//  A pinned pane stays where it was put. A following pane MOVES ONLY WHEN IT
//  HAS TO: while the PC is among the lines already shown, the anchor is left
//  alone and the marker moves down the rows the user is reading. Anchoring on
//  the PC itself, which is what this replaces, re-disassembled from a new
//  address on every snapshot -- a window that never holds still, and one that
//  always showed the PC on its top line with nothing above it.
//
//  When the PC does leave, it comes back in the MIDDLE, so what led there is
//  on screen with it. With source rows, only some of the lines fit in the
//  view, and the window reports how many: the PC on a line past those has
//  left, and the middle is theirs.
//
//  While the machine runs, a view the user scrolled isn't moved until
//  kScrollHoldMs pass with no more scrolling, so the code near the PC can
//  be read while the PC moves, nor while its menu is open. Once the pause
//  ends, the PC is put in the view's middle: following from where the user
//  scrolled to put the PC on the view's top or bottom row.
//
////////////////////////////////////////////////////////////////////////////////

Word CodeViews::ChooseCodeStart (DebugSession & session, Word pc, int view, bool isRunning) const
{
    CodeView  & v       = m_code[(size_t) view];
    size_t      inView  = (v.visible > 0) ? (std::min) ((size_t) v.visible, v.shown.size()) : v.shown.size();
    int         half    = ((v.visible > 0) ? (std::min) (v.visible, v.lines) : v.lines) / 2;
    bool        shown   = std::find (v.shown.begin(), v.shown.begin() + (std::ptrdiff_t) inView, pc) != v.shown.begin() + (std::ptrdiff_t) inView;
    bool        isHeld  = isRunning && (v.isMenuHeld || (v.scrolledAtMs.has_value() && m_clock() - *v.scrolledAtMs < kScrollHoldMs));
    bool        hasHold = isRunning && v.scrolledAtMs.has_value();
    Word        top     = 0;



    if (v.centerOnPc)
    {
        v.centerOn   = pc;
        v.centerOnPc = false;
    }

    //  The following view put somewhere -- scrolled, or navigated to -- is
    //  pinned only until the PC moves. It then follows again from its place,
    //  so a PC on its lines moves the marker, not the view. A scroll pause
    //  that runs out while the machine runs puts the PC in the middle.
    if (view == m_follow && v.address.has_value() && v.pinnedAtPc.has_value() && (*v.pinnedAtPc != pc || hasHold) && !isHeld)
    {
        v.followAnchor = hasHold ? FindStartAbove (session, pc, half) : *v.address;
        v.address.reset();
        v.pinnedAtPc.reset();
        v.scrolledAtMs.reset();
    }

    //  A view that does not follow the PC and has not been put anywhere
    //  starts with the PC in its middle, and stays there after.
    if (view != m_follow && !v.address.has_value() && !v.centerOn.has_value())
    {
        v.centerOn = pc;
    }

    //  A navigation: the address in the middle, or as far down as there are
    //  instructions above it to fill the lines over it.
    if (v.centerOn.has_value())
    {
        top = *v.centerOn;

        for (int above = half; above > 0 && top == *v.centerOn; above--)
        {
            top = FindStartAbove (session, *v.centerOn, above);
        }

        v.address = top;
        v.centerOn.reset();
    }

    if (v.scrollLines != 0)
    {
        top = v.address.value_or (shown ? v.followAnchor : FindStartAbove (session, pc, half));

        v.address     = ScrollCodeTop (session, top, v.lines, v.scrollLines);
        v.scrollLines = 0;
    }

    if (v.address.has_value())
    {
        if (!v.pinnedAtPc.has_value())
        {
            v.pinnedAtPc = pc;
        }

        return *v.address;
    }

    v.pinnedAtPc.reset();

    if (!shown)
    {
        v.followAnchor = FindStartAbove (session, pc, half);
    }

    return v.followAnchor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::ScrollCodeTop
//
//  The top line of a pane of `lines` lines moved `count` instructions from
//  `top`, down when positive. Down goes no further than the page whose last
//  line holds $FFFF, as up stops at $0000, so the pane always fills.
//
////////////////////////////////////////////////////////////////////////////////

Word CodeViews::ScrollCodeTop (DebugSession & session, Word top, int lines, int count) const
{
    Word            from   = top;
    Word            last   = 0;
    int             shown  = 1;
    DataBlockEntry  block;
    DataOrigin      origin = DataOrigin::None;



    //  While the line list is built, a scroll is its lines after the top
    //  line or before it, with the pane's lines filled at the end of memory.
    if (StepLineList (top, count).has_value())
    {
        Word  lowest = *StepLineList (0xFFFF, -(std::max) (0, lines - 1));



        return (std::min) (*StepLineList (top, count), lowest);
    }

    for (int i = 0; i < count && top < 0xFFFF; i++)
    {
        Word  next = (Word) (top + InstructionLookup::GetInstructionLength (session, top));

        top = (next > top) ? next : top;
    }

    if (count > 0)
    {
        last = top;

        while (shown < lines)
        {
            Word  next = (Word) (last + InstructionLookup::GetInstructionLength (session, last));

            if (next <= last)
            {
                break;
            }

            last = next;
            shown++;
        }

        if (shown < lines)
        {
            top = FindStartAbove (session, last, lines - 1);
        }
    }

    //  Up: a data block's lines are known, so those step back one line at a
    //  time. Above code, the alignment that lands on the top line, from as
    //  many lines above as can be found; a byte at a time where none can.
    if (count < 0)
    {
        while (count < 0 && top > 0 && session.TryFindData ((Word) (top - 1), block, origin))
        {
            top = InstructionLookup::GetPreviousInstruction (session, top);
            count++;
        }

        from = top;

        for (int above = -count; above > 0 && top == from; above--)
        {
            top = FindStartAbove (session, from, above);
        }

        if (top == from && from > 0 && count < 0)
        {
            top = (Word) (from - (Word) (std::min) ((int) from, -count));
        }
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::GetCodeLabels
//
//  The addresses of the symbols that could label code, as the views'
//  decode tests each address.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> CodeViews::GetCodeLabels (const DebugSession & session)
{
    std::vector<Word>        labels;
    std::vector<SymbolInfo>  symbols;



    for (int table = 0; table <= (int) SymbolTableId::ProDos; table++)
    {
        if (!session.GetSymbols().IsEnabled ((SymbolTableId) table))
        {
            continue;
        }

        symbols.clear();
        session.GetSymbols().GetAll ((SymbolTableId) table, symbols);

        for (const SymbolInfo & symbol : symbols)
        {
            if (DataDirectiveHandlers::IsCodeLabel (session, symbol.address))
            {
                labels.push_back (symbol.address);
            }
        }
    }

    std::ranges::sort (labels);
    labels.erase (std::unique (labels.begin(), labels.end()), labels.end());

    return labels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::GetBranchTargets
//
//  Where each executed relative branch, JMP and JSR goes, outside the I/O
//  page, sorted. Only an instruction the machine fetched is read, so data
//  that decodes as a branch adds nothing. An indirect jump's operand is a
//  pointer and is left out.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> CodeViews::GetBranchTargets (const Disassembler & disassembler, const DebugMemoryImage & image)
{
    static constexpr Byte    kJmp     = 0x4C;
    static constexpr Byte    kJsr     = 0x20;
    static constexpr Word    kIoFirst = 0xC000;
    static constexpr Word    kIoLast  = 0xC0FF;
    static constexpr size_t  kBranch  = 2;
    std::vector<Word>        targets;



    for (size_t at = 0; at + Disassembler::kMaxInstructionBytes <= DebugMemoryImage::kBytes; at++)
    {
        DisassembledInstruction  instruction;
        Byte                     opcode      = image.bytes[at];
        HRESULT                  hr          = S_OK;
        bool                     isCandidate = image.fetched[at] && (opcode == kJmp || opcode == kJsr || disassembler.GetLength (opcode) == kBranch);



        if (!isCandidate)
        {
            continue;
        }

        hr = disassembler.DisassembleOne ((Word) at, std::span<const Byte> (&image.bytes[at], Disassembler::kMaxInstructionBytes), instruction);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (instruction.hasTarget && (instruction.target < kIoFirst || instruction.target > kIoLast))
        {
            targets.push_back (instruction.target);
        }
    }

    std::ranges::sort (targets);
    targets.erase (std::unique (targets.begin(), targets.end()), targets.end());

    return targets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::RefreshLineList
//
//  While the machine runs, nothing builds the list and the views decode as
//  they did. Paused, the list is built again only when what it is built
//  from changed: memory, the fetch bits, the known starts, the marks, the
//  debug file or the symbols, which a step, an edit, Load file, a symbol
//  edit and a seek all change.
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::RefreshLineList (DebugSession & session, bool isPaused, const CallStackData & calls) const
{
    static constexpr uint64_t    kFnvOffset   = 1469598103934665603ull;
    static constexpr uint64_t    kFnvPrime    = 1099511628211ull;
    auto                         image        = std::make_unique<DebugMemoryImage>();
    Disassembler                 disassembler (session.GetTarget().GetInstructionSet());
    DisassemblyLineList::Inputs  inputs;
    std::vector<Word>            targets;
    uint64_t                     hash         = kFnvOffset;
    Word                         pc           = session.GetTarget().GetRegisters().pc;
    auto                         mix          = [&hash] (uint64_t value) { hash = (hash ^ value) * kFnvPrime; };
    std::lock_guard<std::mutex>  held (m_lineListMutex);



    m_isLineListLive = isPaused;

    if (!isPaused)
    {
        return;
    }

    session.GetTarget().TakeMemoryImage (*image);

    for (size_t at = 0; at < DebugMemoryImage::kBytes; at += 8)
    {
        uint64_t  word = 0;

        std::memcpy (&word, &image->bytes[at], sizeof (word));
        mix (word);
    }

    mix (std::hash<std::bitset<DebugMemoryImage::kBytes>> {} (image->fetched));
    inputs.starts = GetKnownStarts (calls, pc, *image);
    inputs.labels = GetCodeLabels (session);
    inputs.pc     = pc;

    for (Word start : inputs.starts)
    {
        mix (start);
    }

    for (Word label : inputs.labels)
    {
        mix (label);
    }

    mix (session.GetDataBlocks().GetAll().size());

    for (const DataBlockEntry & block : session.GetDataBlocks().GetAll())
    {
        mix (((uint64_t) block.first << 16) | block.last);
    }

    mix (session.GetFiles().GetDebugFileLines().size());
    mix (session.GetFiles().GetFilesRevision());
    mix (session.GetSymbols().GetRevision());

    if (m_lineList.IsBuilt() && hash == m_lineListInputs)
    {
        MakeFindTextIfIdle (session);
        return;
    }

    for (int opcode = 0; opcode < 256; opcode++)
    {
        inputs.lengths[(size_t) opcode] = (uint8_t) disassembler.GetLength ((Byte) opcode);
    }

    //  The targets are found from the bytes and the fetch marks, which the
    //  hash above has, so they are found only when the list is built.
    targets = GetBranchTargets (disassembler, *image);
    session.SetBranchTargets (targets);

    inputs.labels.insert (inputs.labels.end(), targets.begin(), targets.end());
    std::ranges::sort (inputs.labels);
    inputs.labels.erase (std::unique (inputs.labels.begin(), inputs.labels.end()), inputs.labels.end());

    inputs.image      = image.get();
    inputs.debugLines = session.GetFiles().GetDebugFileLines();
    inputs.dataLine   = [&session] (Word address, bool & isMarked) -> int
    {
        DataBlockEntry   block;
        DataOrigin       origin = DataOrigin::None;
        DisassemblyData  data;



        if (!session.TryFindData (address, block, origin))
        {
            return 0;
        }

        isMarked = origin != DataOrigin::DebugFile;
        return (int) (DataDirectiveHandlers::Disassemble (session, address, std::nullopt, 1, data) - address);
    };

    m_lineList.Build (inputs);
    m_lineListInputs = hash;
    m_lineListBuiltMs = GetTickCount64();
    MakeFindTextIfIdle (session);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::MakeFindTextIfIdle
//
//  The find text for the list as it is, once the list hasn't changed for
//  the idle delay. The caller has locked m_lineListMutex.
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::MakeFindTextIfIdle (DebugSession & session) const
{
    bool  isDue = m_findLinesVersion != m_lineList.GetVersion() && GetTickCount64() - m_lineListBuiltMs >= m_findTextIdleMs;



    if (isDue)
    {
        m_findLines        = MakeFindLines (session, m_lineList);
        m_findLinesVersion = m_lineList.GetVersion();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::IsFindTextDue
//
////////////////////////////////////////////////////////////////////////////////

bool CodeViews::IsFindTextDue (uint64_t nowMs) const
{
    std::lock_guard<std::mutex>  held (m_lineListMutex);



    return m_isLineListLive && m_lineList.IsBuilt() && m_findLinesVersion != m_lineList.GetVersion() && nowMs - m_lineListBuiltMs >= m_findTextIdleMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::MakeFindLines
//
//  Each line of the list as a code line with what a find searches: its
//  address, bytes, label and instruction with its symbols, as a view's line
//  has them, and the source line it comes from. The annotation is left
//  out; it is worked out for the lines on screen alone, and its values
//  change with the registers.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const std::vector<DebuggerViewSnapshot::CodeLine>> CodeViews::MakeFindLines (DebugSession & session, const DisassemblyLineList & list)
{
    auto  lines = std::make_shared<std::vector<DebuggerViewSnapshot::CodeLine>>();



    lines->reserve (list.GetLines().size());

    for (const DisassemblyLineList::Line & each : list.GetLines())
    {
        DisassemblyData                 data;
        DebuggerViewSnapshot::CodeLine  row;
        std::string                     bytes;



        (void) DataDirectiveHandlers::Disassemble (session, each.address, std::nullopt, 1, data);

        if (data.lines.empty())
        {
            continue;
        }

        const DisassemblyLine & line = data.lines.front();

        for (size_t at = 0; at < each.length && at < line.instruction.bytes.size(); at++)
        {
            bytes += std::format ("{:02X} ", line.instruction.bytes[at]);
        }

        row.address     = each.address;
        row.bytes       = bytes.empty() ? bytes : bytes.substr (0, bytes.size() - 1);
        row.instruction = line.instruction.operand.empty() ? line.instruction.mnemonic : line.instruction.mnemonic + " " + line.GetShownOperand();
        row.label       = line.label;
        row.dataOrigin  = line.dataOrigin;

        if (line.instruction.hasOperandAddress || line.instruction.operand.starts_with ("("))
        {
            row.memoryOperand = line.instruction.operand;
            row.shownOperand  = line.GetShownOperand();
        }

        if (!session.GetFiles().GetLineTable().GetPositionsAt (each.address).empty())
        {
            row.sourceFileId = session.GetFiles().GetLineTable().GetPositionsAt (each.address).front().file;
            row.sourceLine   = session.GetFiles().GetLineTable().GetPositionsAt (each.address).front().line;
        }

        lines->push_back (std::move (row));
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::GetKnownStarts
//
//  A call frame's call site is a start only where memory has a JSR there,
//  so an interrupt's frame or a JSR overwritten since isn't one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> CodeViews::GetKnownStarts (const CallStackData & calls, Word pc, const DebugMemoryImage & image)
{
    static constexpr Byte  kJsr   = 0x20;
    std::vector<Word>      starts = { pc };



    for (const CallStackRow & row : calls.rows)
    {
        bool  isJsr = row.frame.has_value() && row.frame->kind == CallFrameKind::Call && image.bytes[row.frame->callSite] == kJsr;

        if (isJsr && !std::ranges::contains (starts, row.frame->callSite))
        {
            starts.push_back (row.frame->callSite);
        }
    }

    return starts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::StepLineList
//
//  The address `count` lines from the line holding an address, while the
//  machine is paused and the list is built; none otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> CodeViews::StepLineList (Word address, int count) const
{
    std::lock_guard<std::mutex>  held (m_lineListMutex);



    if (!m_isLineListLive || !m_lineList.IsBuilt())
    {
        return std::nullopt;
    }

    return m_lineList.StepLines (address, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::FindStartAbove
//
//  The 6502 cannot be disassembled backwards: an instruction begins where the
//  one before it ended, and reading the bytes above an address says nothing
//  about where they start. So this walks FORWARD from as far back as `before`
//  instructions could reach -- three bytes each -- and keeps the first
//  alignment that arrives exactly at `pc`, which is the one the machine was
//  executing. Where none does, the PC anchors the pane itself, which is what
//  the pane did before and is right for a PC in data.
//
////////////////////////////////////////////////////////////////////////////////

Word CodeViews::FindStartAbove (DebugSession & session, Word pc, int before) const
{
    Word             first = 0;
    DisassemblyData  data;
    int              index = 0;



    //  The line list's lines above, where it is built: no guessed alignment.
    if (StepLineList (pc, -before).has_value())
    {
        return *StepLineList (pc, -before);
    }

    if (before <= 0 || pc < (Word) (before * 3))
    {
        return pc;
    }

    first = (Word) (pc - (Word) (before * 3));
    (void) DataDirectiveHandlers::Disassemble (session, first, pc, before * 3 + 1, data);

    for (index = 0; index < (int) data.lines.size(); index++)
    {
        if (data.lines[(size_t) index].instruction.address == pc)
        {
            //  `before` lines above the PC, or as many as were found.
            return data.lines[(size_t) (std::max) (0, index - before)].instruction.address;
        }
    }

    return pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViews::MoveCodePane
//
//  Where the code pane is pinned, by name: "." lets it follow the PC again,
//  RET and -> bring an address to its middle line as any navigation does,
//  and the rest move its top line. A following pane moves from the lines it
//  shows, which have the PC in their middle, not from the PC.
//
////////////////////////////////////////////////////////////////////////////////

void CodeViews::MoveCodePane (DebugSession & session, const std::string & name, Reply & reply)
{
    CodeView             & v      = m_code[(size_t) m_follow];
    Word                   pc     = session.GetTarget().GetRegisters().pc;
    Word                   start  = v.address.value_or (v.shown.empty() ? pc : v.shown.front());
    std::optional<Word>    target = start;
    int                    count  = 0;



    if (name == ".")
    {
        //  Through SetCodeAddress, so a scroll or navigation queued ahead of
        //  the line is dropped and does not land on top of it, then centered
        //  on the PC as the Follow PC control does.
        SetCodeAddress (std::nullopt, m_follow);
        ShowPcIn (m_follow);
        reply.data = MessageData { { "The code pane follows the PC." } };
        return;
    }

    if (name == "RET" || name == "->")
    {
        //  -> reads the pinned top line's operand, or the PC's.
        start  = v.address.value_or (pc);
        target = (name == "RET") ? InstructionLookup::GetReturnAddress (session) : InstructionLookup::GetOperandAddress (session, start);

        if (!target.has_value())
        {
            reply.SetError (CommandStatus::Error, "no address",
                            (name == "RET") ? std::string ("The stack cannot be read, so there is no return address.")
                                            : std::format ("The instruction at ${:04X} has no address operand.", start));
            return;
        }

        CenterCodeOn (*target, m_follow);
        reply.data = MessageData { { std::format ("The code pane shows ${:04X}.", *target) } };
        return;
    }

    if (name == "^" || name == "V" || name == "PAGEUP" || name == "PAGEDN")
    {
        count   = (name == "^" || name == "V") ? 1 : v.lines;
        count   = (name == "^" || name == "PAGEUP") ? -count : count;
        *target = ScrollCodeTop (session, start, v.lines, count);
    }
    else if (name == "PAGEUP256")   { *target = (Word) (start - 0x0100); }
    else if (name == "PAGEUP4K")    { *target = (Word) (start - 0x1000); }
    else if (name == "PAGEDOWN256") { *target = (Word) (start + 0x0100); }
    else if (name == "PAGEDOWN4K")  { *target = (Word) (start + 0x1000); }

    SetCodeAddress (target, m_follow);
    reply.data    = MessageData { { std::format ("The code pane is at ${:04X}.", *target) } };
}
