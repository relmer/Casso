#include "Pch.h"

#include "Ui/Debugger/DebuggerViewState.h"

#include "Debugger/DebugSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::Build
//
//  Each pane's command runs in AppleWin mode whatever mode the user is in, so
//  the panes read the same data in both; only the command box follows the
//  chosen mode.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot DebuggerViewState::Build (DebugSession & session) const
{
    DebuggerViewSnapshot  snapshot;
    Reply                 registers   = session.ExecuteLine ("R",     CommandMode::AppleWin);
    Reply                 breakpoints = session.ExecuteLine ("BPL",   CommandMode::AppleWin);
    Reply                 stack       = session.ExecuteLine ("STACK", CommandMode::AppleWin);
    Reply                 watches     = session.ExecuteLine ("WL",    CommandMode::AppleWin);
    Reply                 code;
    Reply                 memory;
    Word                  codeStart   = 0;



    if (const RegistersData * data = std::get_if<RegistersData> (&registers.data))
    {
        const Cpu6502Registers & r = data->registers;
        static const char        kFlagNames[] = "NV-BDIZC";

        snapshot.pc        = r.pc;
        snapshot.registers = { { "A",  std::format ("{:02X}", r.a) },
                               { "X",  std::format ("{:02X}", r.x) },
                               { "Y",  std::format ("{:02X}", r.y) },
                               { "P",  std::format ("{:02X}", r.p) },
                               { "S",  std::format ("{:02X}", r.sp) },
                               { "PC", std::format ("{:04X}", r.pc) } };

        for (int bit = 7; bit >= 0; bit--)
        {
            snapshot.flags += ((r.p >> bit) & 1) ? kFlagNames[7 - bit] : '.';
        }
    }

    if (const BreakpointListData * data = std::get_if<BreakpointListData> (&breakpoints.data))
    {
        for (const BreakpointInfo & info : data->breakpoints)
        {
            snapshot.breakpoints.push_back ({ info.id, info.address,
                                              std::format ("#{} ${:04X}{}", info.id, info.address, info.enabled ? "" : " (off)"),
                                              info.enabled });
        }
    }

    codeStart = m_codeAddress.value_or (snapshot.pc);
    code      = session.ExecuteLine (std::format ("U {:04X}", codeStart), CommandMode::AppleWin);

    if (const DisassemblyData * data = std::get_if<DisassemblyData> (&code.data))
    {
        for (const DisassemblyLine & line : data->lines)
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
                                                                 : line.instruction.mnemonic + " " + line.instruction.operand;
            row.symbol        = line.symbol;
            row.isCurrent     = row.address == snapshot.pc;
            row.hasBreakpoint = std::any_of (snapshot.breakpoints.begin(), snapshot.breakpoints.end(),
                                             [&] (const DebuggerViewSnapshot::BreakpointLine & bp) { return bp.address == row.address; });

            snapshot.code.push_back (std::move (row));

            if ((int) snapshot.code.size() >= kCodeLines)
            {
                break;
            }
        }
    }

    memory = session.ExecuteLine (std::format ("D {:04X}", m_memoryAddress), CommandMode::AppleWin);

    if (const MemoryData * data = std::get_if<MemoryData> (&memory.data))
    {
        for (const MemoryRow & row : data->rows)
        {
            DebuggerViewSnapshot::MemoryLine  line;



            line.address = row.address;
            line.region  = GetRegionLabel (row.region);

            for (const std::optional<Byte> & b : row.bytes)
            {
                //  An I/O byte is not read for display, because reading one can
                //  change the machine; it shows as "--" rather than a guess.
                line.bytes      += b.has_value() ? std::format ("{:02X} ", *b) : std::string ("-- ");
                line.characters += (b.has_value() && (*b & 0x7F) >= 0x20 && (*b & 0x7F) < 0x7F) ? (char) (*b & 0x7F) : '.';
            }

            if (!line.bytes.empty())
            {
                line.bytes.pop_back();
            }

            snapshot.memory.push_back (std::move (line));

            if ((int) snapshot.memory.size() >= kMemoryRows)
            {
                break;
            }
        }
    }

    if (const StackData * data = std::get_if<StackData> (&stack.data))
    {
        for (const StackEntry & entry : data->entries)
        {
            snapshot.stack.push_back ({ entry.address, entry.value });
        }
    }

    if (const WatchListData * data = std::get_if<WatchListData> (&watches.data))
    {
        for (const WatchEntry & entry : data->entries)
        {
            snapshot.watches.push_back ({ entry.id, entry.address,
                                          entry.value.has_value() ? std::format ("{:04X}", *entry.value) : std::string ("--") });
        }
    }

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetToggleBreakpointLine
//
//  A click sets a breakpoint where there is none and clears the one that is
//  there. Clearing goes by id, so a click never clears a different breakpoint
//  that happens to cover the same address.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetToggleBreakpointLine (const DebuggerViewSnapshot & snapshot, Word address)
{
    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (bp.address == address)
        {
            return std::format ("BPC {}", bp.id);
        }
    }

    return std::format ("BP {:04X}", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetPokeLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetPokeLine (Word address, Byte value)
{
    return std::format ("MEB {:04X} {:02X}", address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetRunToCursorLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetRunToCursorLine (Word address)
{
    return std::format ("G {:04X}", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::ExecuteLine
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewState::ExecuteLine (DebugSession & session, const std::string & line, CommandMode mode)
{
    Reply  reply = session.ExecuteLine (line, mode);



    session.FormatReply (reply, mode);
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState::GetRegionLabel
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerViewState::GetRegionLabel (MemoryRegion region)
{
    switch (region)
    {
    case MemoryRegion::MainRam: return "RAM";
    case MemoryRegion::AuxRam:  return "AUX";
    case MemoryRegion::LcBank1: return "LC1";
    case MemoryRegion::LcBank2: return "LC2";
    case MemoryRegion::Rom:     return "ROM";
    case MemoryRegion::SlotRom: return "SLOT";
    case MemoryRegion::Io:      return "I/O";
    }

    return "?";
}
