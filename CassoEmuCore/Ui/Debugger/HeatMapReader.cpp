#include "Pch.h"

#include "Ui/Debugger/HeatMapReader.h"
#include "Ui/Debugger/InstructionLookup.h"
#include "Ui/Debugger/MemoryRegionKeys.h"

#include "Debugger/AccessHeatMap.h"
#include "Debugger/DebugSession.h"
#include "Debugger/HeatMapSymbols.h"
#include "Disassembler.h"
#include "Debugger/HeatAccessJump.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::BuildHeatMap
//
//  The map is on only while its pane or a heat column is shown: a build with
//  them all hidden turns it off, and a build with one shown turns it on and
//  puts the counts in the snapshot (ReadHeatMap, ReadHeatColumns). The map
//  stays on while BRKUNINIT is, pane or no pane, since a read before written
//  is counted by the map alone.
//
//  A running machine's map is read every kHeatIntervalMs, and the snapshots
//  between hold that reading; reading all 64K addresses at every snapshot
//  took the thread that runs the machine several milliseconds a frame in a
//  debug build. A stopped machine's is read at every build, as is the map
//  after a change of its options or anything the window did. With the mouse
//  over a cell, the cell's last writer and reader go with every snapshot.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapReader::BuildHeatMap (DebugSession & session, DebuggerViewSnapshot & snapshot, bool isPaused) const
{
    IDebugTarget                                         & target = session.GetTarget();
    const AccessHeatMap                                  * map    = nullptr;
    DebuggerViewSnapshot::HeatMapState                   & state  = snapshot.heatMap;
    uint64_t                                               now    = m_clock();
    std::shared_ptr<DebuggerViewSnapshot::HeatMapState>    read;
    HeatSpace                                              space  = HeatSpace::Cpu;
    bool                                                   isDue  = isPaused || m_isHeatReadDue || now - m_heatReadAt >= kHeatIntervalMs;



    target.SetHeatMapOn         (m_isHeatMapShown || m_isHeatColumnsShown || session.GetStops().HasUnwrittenBreak());
    target.SetHeatMapFade       ((double) m_heatMapOptions.fadeSeconds);
    target.SetHeatMapCumulative (m_heatMapOptions.cumulative);
    map = target.FoldHeatMap();

    if (map == nullptr || (!m_isHeatMapShown && !m_isHeatColumnsShown))
    {
        m_isHeatReadDue = true;
        return;
    }

    if (m_isHeatColumnsShown && (m_heatColumnsRead == nullptr || isDue))
    {
        m_heatColumnsRead = ReadHeatColumns (*map);
    }

    if (m_isHeatMapShown && (m_heatRead == nullptr || isDue))
    {
        read = std::make_shared<DebuggerViewSnapshot::HeatMapState>();
        ReadHeatMap (session, *map, *read);

        read->serial = ++m_heatSerial;
        m_heatRead   = read;
    }

    if (isDue)
    {
        m_heatReadAt    = now;
        m_isHeatReadDue = false;
    }

    snapshot.heatColumns = m_isHeatColumnsShown ? m_heatColumnsRead : nullptr;

    if (!m_isHeatMapShown)
    {
        return;
    }

    state = *m_heatRead;
    space = HeatMapOptions::GetSpace (state.bank);

    if (!m_heatMapOptions.cumulative)
    {
        state.isRebuilding = target.IsHeatMapRebuilding();
    }

    if (m_heatMapHover.has_value() && HeatMapOptions::IsShown (state.bank, *m_heatMapHover))
    {
        state.hover = std::make_shared<const HeatAccessHover> (HeatAccessHover { *m_heatMapHover,
                                                                                 state.bank,
                                                                                 GetHeatAccess (session, space, true,  *m_heatMapHover),
                                                                                 GetHeatAccess (session, space, false, *m_heatMapHover) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::ReadHeatColumns
//
//  The CPU's address space's levels, as fading heat or as totals as the
//  heat map's options choose, with same-value writes left out under Skip
//  unchanged writes.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels> HeatMapReader::ReadHeatColumns (const AccessHeatMap & map) const
{
    auto        levels = std::make_shared<DebuggerViewSnapshot::HeatColumnLevels>();
    HeatKind    write  = m_heatMapOptions.ignoreSameWrites ? HeatKind::ChangedWrite : HeatKind::Write;



    if (m_heatMapOptions.cumulative)
    {
        map.GetTotalLevels (HeatSpace::Cpu, HeatKind::Execute,   levels->execute);
        map.GetTotalLevels (HeatSpace::Cpu, HeatKind::Read,      levels->read);
        map.GetTotalLevels (HeatSpace::Cpu, write,               levels->write);
        map.GetTotalLevels (HeatSpace::Cpu, HeatKind::Statement, levels->statement);
    }
    else
    {
        map.GetLevels (HeatSpace::Cpu, HeatKind::Execute,   levels->execute);
        map.GetLevels (HeatSpace::Cpu, HeatKind::Read,      levels->read);
        map.GetLevels (HeatSpace::Cpu, write,               levels->write);
        map.GetLevels (HeatSpace::Cpu, HeatKind::Statement, levels->statement);
    }

    levels->executeCount.resize   (0x10000);
    levels->readCount.resize      (0x10000);
    levels->writeCount.resize     (0x10000);
    levels->statementCount.resize (0x10000);

    for (size_t address = 0; address < 0x10000; address++)
    {
        auto  clamp = [] (uint64_t count) { return (uint32_t) std::min<uint64_t> (count, UINT32_MAX); };

        levels->executeCount[address]   = clamp (map.GetTotal (HeatSpace::Cpu, HeatKind::Execute,   (Word) address));
        levels->readCount[address]      = clamp (map.GetTotal (HeatSpace::Cpu, HeatKind::Read,      (Word) address));
        levels->writeCount[address]     = clamp (map.GetTotal (HeatSpace::Cpu, write,               (Word) address));
        levels->statementCount[address] = clamp (map.GetTotal (HeatSpace::Cpu, HeatKind::Statement, (Word) address));
    }

    return levels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::ReadHeatMap
//
//  What the map has counted, as fading heat or as totals, in the bank chosen
//  when the machine has it and in the CPU's otherwise. A language card's
//  view shows its 16 KB alone. With them go the bank's bytes, the opcodes
//  fetched, and where the PC and stack pointer are, for the overlays. Reads
//  before written in the ranges left out are dropped, and with
//  same-value writes left out the writes given are the changes alone.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapReader::ReadHeatMap (
    DebugSession                          & session,
    const AccessHeatMap                   & map,
    DebuggerViewSnapshot::HeatMapState    & state) const
{
    IDebugTarget  & target = session.GetTarget();
    HeatSpace       space  = HeatSpace::Cpu;



    for (size_t bank = 0; bank < HeatMapOptions::kBankCount; bank++)
    {
        if (HeatMapOptions::IsAvailable ((HeatMapOptions::Bank) bank, map.GetBankMap()))
        {
            state.banks.push_back ((HeatMapOptions::Bank) bank);
        }
    }

    state.hasAux = map.GetBankMap().HasSpace (HeatSpace::Aux);
    state.bank   = (std::ranges::find (state.banks, m_heatMapOptions.bank) != state.banks.end()) ? m_heatMapOptions.bank : HeatMapOptions::Bank::Cpu;
    space        = HeatMapOptions::GetSpace (state.bank);

    if (m_heatMapOptions.cumulative)
    {
        state.top = (double) map.GetMostTotal (space);

        map.GetTotalLevels (space, HeatKind::Execute,       state.execute);
        map.GetTotalLevels (space, HeatKind::Read,          state.read);
        map.GetTotalLevels (space, HeatKind::Write,         state.write);
        map.GetTotalLevels (space, HeatKind::UnwrittenRead, state.unwritten);
        map.GetTotalLevels (space, HeatKind::ChangedWrite,  state.changed);
    }
    else
    {
        state.top = AccessHeatMap::kHottestPerSecond;

        map.GetLevels (space, HeatKind::Execute,       state.execute);
        map.GetLevels (space, HeatKind::Read,          state.read);
        map.GetLevels (space, HeatKind::Write,         state.write);
        map.GetLevels (space, HeatKind::UnwrittenRead, state.unwritten);
        map.GetLevels (space, HeatKind::ChangedWrite,  state.changed);
    }

    if (m_heatMapOptions.ignoreSameWrites)
    {
        state.write = state.changed;
    }

    map.GetOpcodeMarks (space, state.opcodes);
    map.GetEditedMarks (space, state.edited);

    for (size_t address = 0; address < state.execute.size(); address++)
    {
        if (!HeatMapOptions::IsShown (state.bank, (Word) address))
        {
            state.execute[address]   = 0;
            state.read[address]      = 0;
            state.write[address]     = 0;
            state.unwritten[address] = 0;
            state.changed[address]   = 0;
            state.edited[address]    = 0;
        }

        if (map.IsUnwrittenIgnored (HeatMapOptions::GetCpuAddress (state.bank, (Word) address)))
        {
            state.unwritten[address] = 0;
        }
    }

    ReadHeatValues (target, map, state.bank, state.values, state.regions);
    FindHeatMarks  (target, map, state);

    state.opcodeForms = GetOpcodeForms (target);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::ReadHeatValues
//
//  The bank's bytes as they stand, for the cells large enough to show them.
//  The CPU's are what it would read, I/O as its devices' side-effect-free
//  peeks work it out; ROM's only where a read returns ROM now; a RAM bank's
//  straight from its buffer, whatever is banked in. In the CPU's bank, each
//  address's region, read a page at a time.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapReader::ReadHeatValues (
    IDebugTarget              & target,
    const AccessHeatMap       & map,
    HeatMapOptions::Bank        bank,
    std::vector<int16_t>      & values,
    std::vector<uint16_t>     & regions)
{
    constexpr int16_t     kUnknown  = -1;
    constexpr size_t      kPageSize = 0x100;
    HeatSpace             space     = HeatMapOptions::GetSpace (bank);
    const HeatBankMap   & bankMap   = map.GetBankMap();
    HeatLocation          location;
    MemoryRegion          region    = MemoryRegion::MainRam;
    Byte                  value     = 0;
    bool                  isRead    = false;



    values.assign (AccessHeatMap::kAddressCount, kUnknown);
    regions.clear();

    if (space == HeatSpace::Cpu)
    {
        regions.assign (AccessHeatMap::kAddressCount, 0);
    }

    for (size_t index = 0; index < AccessHeatMap::kAddressCount; index++)
    {
        Word  address = (Word) index;



        if (space == HeatSpace::Cpu && index % kPageSize == 0)
        {
            region = target.GetRegion (address);
        }

        if (!HeatMapOptions::IsShown (bank, address))
        {
            continue;
        }

        switch (space)
        {
        case HeatSpace::Cpu:
            regions[index] = MemoryRegionKeys::GetKey (region, address);
            isRead         = (region == MemoryRegion::Io) ? target.TryPeekIo (address, value) : target.TryPeek (address, value);
            break;

        case HeatSpace::Rom:
            isRead = bankMap.TryResolve (address, false, location) && location.space == HeatSpace::Rom && target.TryPeek (address, value);
            break;

        default:
            isRead = bankMap.TryPeek (space, address, value);
            break;
        }

        if (isRead)
        {
            values[index] = (int16_t) value;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::FindHeatMarks
//
//  The PC and the stack pointer's byte on page 1, where in the bank shown a
//  read of each lands now: the CPU's own addresses in its view, and in a
//  bank's only when that bank holds them, so aux RAM's view marks the stack
//  only while the zero page and stack are switched to aux.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapReader::FindHeatMarks (
    IDebugTarget                          & target,
    const AccessHeatMap                   & map,
    DebuggerViewSnapshot::HeatMapState    & state)
{
    constexpr Word     kStackPage = 0x0100;
    Cpu6502Registers   registers  = target.GetRegisters();
    HeatSpace          space      = HeatMapOptions::GetSpace (state.bank);
    Word               stack      = (Word) (kStackPage | registers.sp);
    HeatLocation       location;



    if (space == HeatSpace::Cpu)
    {
        state.pc    = registers.pc;
        state.stack = stack;
        return;
    }

    if (map.GetBankMap().TryResolve (registers.pc, false, location) && location.space == space && HeatMapOptions::IsShown (state.bank, location.index))
    {
        state.pc = location.index;
    }

    if (map.GetBankMap().TryResolve (stack, false, location) && location.space == space && HeatMapOptions::IsShown (state.bank, location.index))
    {
        state.stack = location.index;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::GetOpcodeForm
//
//  The instruction the opcode starts, decoded over zero operand bytes, with
//  each run of hex digits after a $ put down as that many dots: the form
//  reads the same whatever the operand holds.
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapReader::GetOpcodeForm (const Microcode * instructionSet, Byte opcode)
{
    HRESULT                                                hr      = S_OK;
    std::array<Byte, Disassembler::kMaxInstructionBytes>   bytes   = { opcode };
    DisassembledInstruction                                instruction;
    std::string                                            form;
    bool                                                   isDigit = false;



    CBRAEx (instructionSet != nullptr, E_INVALIDARG);

    hr = Disassembler (instructionSet).DisassembleOne (0, bytes, instruction);
    CHRA (hr);

    BAIL_OUT_IF (!instruction.isDefined, S_OK);

    form = instruction.mnemonic;

    if (!instruction.operand.empty())
    {
        form += ' ';
    }

    for (char ch : instruction.operand)
    {
        if (ch == '$')
        {
            isDigit = true;
            continue;
        }

        isDigit = isDigit && std::isxdigit ((unsigned char) ch) != 0;
        form   += isDigit ? '.' : ch;
    }

Error:
    return form;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::GetOpcodeForms
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DebuggerViewSnapshot::OpcodeForms> HeatMapReader::GetOpcodeForms (const IDebugTarget & target) const
{
    constexpr size_t                                    kOpcodeCount   = 256;
    const Microcode                                   * instructionSet = target.GetInstructionSet();
    std::shared_ptr<DebuggerViewSnapshot::OpcodeForms>  forms;



    if (m_opcodeForms != nullptr && m_opcodeFormsSet == instructionSet)
    {
        return m_opcodeForms;
    }

    forms = std::make_shared<DebuggerViewSnapshot::OpcodeForms> (kOpcodeCount);

    for (size_t opcode = 0; opcode < kOpcodeCount && instructionSet != nullptr; opcode++)
    {
        (*forms)[opcode] = GetOpcodeForm (instructionSet, (Byte) opcode);
    }

    m_opcodeForms      = forms;
    m_opcodeFormsSet   = instructionSet;
    m_opcodeFormsTable = target.GetInstructionTable();
    return m_opcodeForms;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::GetHeatAccess
//
//  The instruction that last wrote or read an address, looked up in history
//  where the map does not hold it, and disassembled from the code at its PC
//  as it stands now.
//
////////////////////////////////////////////////////////////////////////////////

HeatAccessInfo HeatMapReader::GetHeatAccess (
    DebugSession  & session,
    HeatSpace       space,
    bool            isWrite,
    Word            address)
{
    HeatAccessInfo                   info;
    HeatLastAccess                   access;
    HeatAccessState                  state = HeatAccessState::None;
    std::optional<DisassemblyLine>   line;



    state         = session.GetTarget().LookUpHeatMapAccess (space, isWrite, address, access);
    info.has      = state == HeatAccessState::Found;
    info.isTooOld = state == HeatAccessState::Unknown;

    if (!info.has)
    {
        return info;
    }

    info.pc    = access.GetPc();
    info.cycle = access.cycle;
    line       = InstructionLookup::GetInstructionAt (session, info.pc);

    if (line.has_value())
    {
        info.label       = line->label;
        info.instruction = line->instruction.mnemonic;

        if (!line->instruction.operand.empty())
        {
            info.instruction += " " + line->GetShownOperand();
        }
    }

    return info;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapReader::GetHeatMapSymbols
//
//  Every snapshot carries the copy, but it is taken only when the tables'
//  revision moves, so a running machine copies nothing frame to frame.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const HeatMapSymbols> HeatMapReader::GetHeatMapSymbols (const SymbolTable & symbols) const
{
    if (m_heatMapSymbols == nullptr || m_heatMapSymbolsRevision != symbols.GetRevision())
    {
        m_heatMapSymbols         = HeatMapSymbols::Build (symbols);
        m_heatMapSymbolsRevision = symbols.GetRevision();
    }

    return m_heatMapSymbols;
}
