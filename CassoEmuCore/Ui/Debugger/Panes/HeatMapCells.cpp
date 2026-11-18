#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapCells.h"
#include "Ui/Debugger/Panes/HeatMapCellText.h"
#include "Ui/Debugger/Panes/HeatMapSections.h"
#include "Debugger/HeatMapSymbols.h"
#include "Debugger/Reply.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/HeatStrip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::SetLevels
//
//  One level per address for each kind; empty vectors are a cold map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapCells::SetLevels (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write)
{
    m_execute = execute;
    m_read    = read;
    m_write   = write;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::SetChannelLevels
//
//  Kept for the next SetLevels, which draws them with the rest.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapCells::SetChannelLevels (const std::vector<Byte> & unwritten, const std::vector<Byte> & changed, const std::vector<Byte> & edited)
{
    m_unwritten = unwritten;
    m_changed   = changed;
    m_edited    = edited;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::SetPalette
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapCells::SetPalette (const Palette & palette)
{
    if (palette == m_palette)
    {
        return false;
    }

    m_palette = palette;

    BuildCells();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::SetMode
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapCells::SetMode (Mode mode)
{
    m_options.view = mode;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::SetOptions
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapCells::SetOptions (const HeatMapOptions & options)
{
    m_options = options;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetColor
//
//  An address takes the color of the kind that touched it most, as the mode
//  lets it be seen, at its level on that kind's scale, the disassembly's
//  heat column's (HeatStrip::GetScaleColor). Where code and data are equally
//  hot, code is drawn. A write is drawn over a read as hot. An address nothing
//  touched is the cold gray.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapCells::GetColor (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette)
{
    return GetColor (mode, execute, read, write, palette, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetColor
//
//  With Blend, an address takes the colors of every kind the mode shows that
//  touched it, each at its level on its scale and weighted by how hot it
//  is; one both run as code and written takes the self-modifying color,
//  mixed toward it from the cold gray by how hot it is.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapCells::GetColor (
    Mode               mode,
    Byte               execute,
    Byte               read,
    Byte               write,
    const Palette    & palette,
    bool               isBlend)
{
    constexpr float  kTop            = 255.0f;
    constexpr int    kRedShift       = 16;
    constexpr int    kGreenShift     = 8;
    constexpr int    kBlueShift      = 0;
    uint32_t         cold            = palette.cold | 0xFF000000u;
    Byte             ran             = (mode == Mode::Data) ? (Byte) 0 : execute;
    Byte             got             = (mode == Mode::Code) ? (Byte) 0 : read;
    Byte             put             = (mode == Mode::Code) ? (Byte) 0 : write;
    Byte             data            = std::max (read, write);
    Byte             level           = std::max ({ ran, got, put });
    float            total           = std::max (1.0f, (float) ran + (float) got + (float) put);   // at least 1, so C4723 cannot fire; a nonzero total of whole counts is 1 or more already
    uint32_t         code            = HeatStrip::GetScaleColor (HeatStrip::Kind::Run, ran, palette);
    uint32_t         readColor       = HeatStrip::GetScaleColor (HeatStrip::Kind::Read,  got, palette);
    uint32_t         writeColor      = HeatStrip::GetScaleColor (HeatStrip::Kind::Write, put, palette);
    auto             mixChannel      = [&] (int shift)
    {
        float  sum = (float) ((code >> shift) & 0xFF) * ran + (float) ((readColor >> shift) & 0xFF) * got + (float) ((writeColor >> shift) & 0xFF) * put;

        return (uint32_t) std::lround (sum / total) << shift;
    };



    if (isBlend && level == 0)
    {
        return cold;
    }

    if (isBlend && ran > 0 && put > 0)
    {
        return DxuiColor::Mix (cold, palette.selfModifying | 0xFF000000u, kFaintest + (1.0f - kFaintest) * (float) level / kTop) | 0xFF000000u;
    }

    if (isBlend)
    {
        return mixChannel (kRedShift) | mixChannel (kGreenShift) | mixChannel (kBlueShift) | 0xFF000000u;
    }

    if (mode == Mode::Code || (mode == Mode::All && execute >= data))
    {
        return (execute == 0) ? cold : HeatStrip::GetScaleColor (HeatStrip::Kind::Run, execute, palette);
    }

    if (data == 0)
    {
        return cold;
    }

    return HeatStrip::GetScaleColor ((write >= read) ? HeatStrip::Kind::Write : HeatStrip::Kind::Read, data, palette);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetChannelColor
//
//  Changed shows the changes alone, mixed toward their color from the cold
//  gray as the others are. All and Data draw a read before written over
//  whatever else the address is, since it is the rare thing worth seeing,
//  from kUnwrittenFaintest rather than kFaintest.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapCells::GetChannelColor (
    Mode              mode,
    Byte              unwritten,
    Byte              changed,
    uint32_t          color,
    const Palette   & palette)
{
    constexpr float  kTop   = 255.0f;
    uint32_t         hue    = palette.unwritten;
    Byte             level  = unwritten;
    float            floor  = kUnwrittenFaintest;



    if (mode == Mode::Changed)
    {
        hue   = palette.changed;
        level = changed;
        floor = kFaintest;
    }
    else if (mode == Mode::Code || unwritten == 0)
    {
        return color;
    }

    if (level == 0)
    {
        return palette.cold | 0xFF000000u;
    }

    return DxuiColor::Mix (palette.cold | 0xFF000000u, hue | 0xFF000000u,
                           floor + (1.0f - floor) * (float) level / kTop) | 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetModeLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapCells::GetModeLabel (Mode mode)
{
    switch (mode)
    {
    case Mode::Code:    return L"Code";
    case Mode::Data:    return L"Data";
    case Mode::Changed: return L"Changed";
    default:            return L"All";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetShownLevel
//
//  How hot an address is as the mode shows it.
//
////////////////////////////////////////////////////////////////////////////////

Byte HeatMapCells::GetShownLevel (Word address) const
{
    constexpr size_t  kCount  = (size_t) kAddressCount;
    Byte              execute = (m_execute.size() == kCount) ? m_execute[address] : (Byte) 0;
    Byte              read    = (m_read.size()    == kCount) ? m_read[address]    : (Byte) 0;
    Byte              write   = (m_write.size()   == kCount) ? m_write[address]   : (Byte) 0;
    Byte              changed = (m_changed.size() == kCount) ? m_changed[address] : (Byte) 0;



    switch (m_options.view)
    {
    case Mode::Code:    return execute;
    case Mode::Data:    return std::max (read, write);
    case Mode::Changed: return changed;
    default:            return std::max ({ execute, read, write });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::BuildCells
//
//  Each address's color; a kind with no levels is cold everywhere. An
//  address with a symbol is tinted toward the symbol color, and the symbols
//  are found again first when the bank shown has changed.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapCells::BuildCells()
{
    constexpr size_t  kCount     = (size_t) kAddressCount;
    bool              hasExecute = m_execute.size() == kCount;
    bool              hasRead    = m_read.size()    == kCount;
    bool              hasWrite   = m_write.size()   == kCount;
    bool              hasSymbols = false;
    bool              hasUnread  = m_unwritten.size() == kCount;
    bool              hasChanged = m_changed.size() == kCount;



    if (m_symbols != nullptr && m_symbolsBank != m_shownBank)
    {
        MarkSymbols();
    }

    hasSymbols = m_symbolMarks.size() == kCount;

    m_cells.resize (kCount);

    for (size_t address = 0; address < kCount; address++)
    {
        m_cells[address] = GetColor (m_options.view,
                                     hasExecute ? m_execute[address] : (Byte) 0,
                                     hasRead    ? m_read[address]    : (Byte) 0,
                                     hasWrite   ? m_write[address]   : (Byte) 0,
                                     m_palette,
                                     m_options.blend);

        m_cells[address] = GetChannelColor (m_options.view,
                                            hasUnread  ? m_unwritten[address] : (Byte) 0,
                                            hasChanged ? m_changed[address]   : (Byte) 0,
                                            m_cells[address],
                                            m_palette);

        if (hasSymbols && m_symbolMarks[address] != 0)
        {
            m_cells[address] = DxuiColor::Mix (m_cells[address], m_palette.symbol | 0xFF000000u, kSymbolTint) | 0xFF000000u;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::SetOpcodes
//
//  Drawn from the next levels set, which come with every snapshot.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapCells::SetOpcodes (const std::vector<Byte> & opcodes)
{
    m_opcodes = opcodes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::IsOperand
//
//  Run as code but never fetched as an opcode: only ever an operand. With
//  no opcodes known, no byte is.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapCells::IsOperand (Word address) const
{
    constexpr size_t  kCount = (size_t) kAddressCount;



    return m_opcodes.size() == kCount && m_execute.size() == kCount && m_execute[address] > 0 && m_opcodes[address] == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::SetBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapCells::SetBreakpoints (std::vector<Breakpoint> breakpoints)
{
    m_breakpoints = std::move (breakpoints);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::SetSymbols
//
//  The same copy again changes nothing; another is marked and drawn.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapCells::SetSymbols (std::shared_ptr<const HeatMapSymbols> symbols)
{
    if (symbols == m_symbols)
    {
        return false;
    }

    m_symbols = std::move (symbols);

    MarkSymbols();
    BuildCells();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::MarkSymbols
//
//  Each address of the bank shown whose CPU address has a symbol.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapCells::MarkSymbols()
{
    constexpr size_t  kCount = (size_t) kAddressCount;
    std::string       name;



    m_symbolsBank = m_shownBank;
    m_symbolMarks.clear();

    if (m_symbols == nullptr)
    {
        return;
    }

    m_symbolMarks.assign (kCount, 0);

    for (size_t address = 0; address < kCount; address++)
    {
        if (HeatMapOptions::IsShown (m_shownBank, (Word) address) &&
            m_symbols->TryGetNameAt (HeatMapOptions::GetCpuAddress (m_shownBank, (Word) address), name))
        {
            m_symbolMarks[address] = 1;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetBreakKind
//
//  A breakpoint on an address is run; a watchpoint on memory or I/O is read,
//  written or both, as it is set; one on memory taking a value is written.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapCells::BreakKind> HeatMapCells::GetBreakKind (const BreakpointInfo & info)
{
    if (!info.enabled)
    {
        return std::nullopt;
    }

    switch (info.kind)
    {
    case BreakpointKind::Address:
        return BreakKind::Execute;

    case BreakpointKind::Memory:
        switch (info.access)
        {
        case WatchAccess::Read:  return BreakKind::Read;
        case WatchAccess::Write: return BreakKind::Write;
        default:                 return BreakKind::ReadWrite;
        }

    case BreakpointKind::MemoryValue:
        return BreakKind::Write;

    default:
        return std::nullopt;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetOutlineColors
//
//  A watchpoint on both reads and writes is outlined twice, read outside.
//  A breakpoint is on the CPU's address, so in a bank's view it marks the
//  bank's byte the CPU reaches at that address.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> HeatMapCells::GetOutlineColors (Word address) const
{
    std::vector<uint32_t>  colors;
    Word                   cpu    = HeatMapOptions::GetCpuAddress (m_shownBank, address);



    if (m_found == address)
    {
        colors.push_back (m_palette.found);
    }

    if (m_pc == address)
    {
        colors.push_back (m_palette.pc);
    }

    if (m_stack == address)
    {
        colors.push_back (m_palette.stack);
    }

    for (const Breakpoint & breakpoint : m_breakpoints)
    {
        if (cpu < breakpoint.first || cpu > breakpoint.last)
        {
            continue;
        }

        switch (breakpoint.kind)
        {
        case BreakKind::Execute:
            colors.push_back (m_palette.breakpoint);
            break;

        case BreakKind::Read:
            colors.push_back (m_palette.readWatch);
            break;

        case BreakKind::Write:
            colors.push_back (m_palette.writeWatch);
            break;

        default:
            colors.push_back (m_palette.readWatch);
            colors.push_back (m_palette.writeWatch);
            break;
        }
    }

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetCellColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapCells::GetCellColor (Word address) const
{
    return m_cells.empty() ? (m_palette.cold | 0xFF000000u) : m_cells[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetCellKind
//
//  What GetColor and GetChannelColor draw for the levels, in the order they
//  draw it: Changed draws the changed writes alone; a read before written
//  covers the rest in All and Data; then Blend gives a mix of kinds, or the
//  self-modifying color, and else the hotter of code and data is drawn.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapCells::CellKind HeatMapCells::GetCellKind (
    Mode   mode,
    Byte   execute,
    Byte   read,
    Byte   write,
    Byte   unwritten,
    Byte   changed,
    bool   isOperand,
    bool   isBlend)
{
    Byte      ran       = (mode == Mode::Data) ? (Byte) 0 : execute;
    Byte      got       = (mode == Mode::Code) ? (Byte) 0 : read;
    Byte      put       = (mode == Mode::Code) ? (Byte) 0 : write;
    Byte      data      = std::max (read, write);
    int       kindCount = (ran > 0 ? 1 : 0) + (got > 0 ? 1 : 0) + (put > 0 ? 1 : 0);
    CellKind  code      = isOperand ? CellKind::Operand : CellKind::Code;



    if (mode == Mode::Changed)
    {
        return (changed > 0) ? CellKind::Changed : CellKind::Untouched;
    }

    if (mode != Mode::Code && unwritten > 0)
    {
        return CellKind::Unwritten;
    }

    if (isBlend)
    {
        if (kindCount == 0)
        {
            return CellKind::Untouched;
        }

        if (ran > 0 && put > 0)
        {
            return CellKind::SelfModifying;
        }

        if (kindCount > 1)
        {
            return CellKind::Blended;
        }

        return (ran > 0) ? code : (got > 0) ? CellKind::Read : CellKind::Written;
    }

    if (mode == Mode::Code || (mode == Mode::All && execute >= data))
    {
        return (execute > 0) ? code : CellKind::Untouched;
    }

    if (data == 0)
    {
        return CellKind::Untouched;
    }

    return (write >= read) ? CellKind::Written : CellKind::Read;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetCellKind
//
////////////////////////////////////////////////////////////////////////////////

HeatMapCells::CellKind HeatMapCells::GetCellKind (Word address) const
{
    constexpr size_t  kCount = (size_t) kAddressCount;
    Byte              level[5] =
    {
        (m_execute.size()   == kCount) ? m_execute[address]   : (Byte) 0,
        (m_read.size()      == kCount) ? m_read[address]      : (Byte) 0,
        (m_write.size()     == kCount) ? m_write[address]     : (Byte) 0,
        (m_unwritten.size() == kCount) ? m_unwritten[address] : (Byte) 0,
        (m_changed.size()   == kCount) ? m_changed[address]   : (Byte) 0,
    };



    return GetCellKind (m_options.view, level[0], level[1], level[2], level[3], level[4], IsOperand (address), m_options.blend);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::TryGetCellScale
//
//  The cell's color is mixed from the cold gray toward its kind's color by
//  its level, from kFaintest at level 1 to the full color at level 255
//  (GetColor, GetChannelColor); the scale has those two ends and the middle.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapCells::TryGetCellScale (Word address, std::vector<uint32_t> & gradient, float & mark) const
{
    constexpr size_t  kCount     = (size_t) kAddressCount;
    constexpr float   kTop       = 255.0f;
    constexpr float   kMiddle    = 0.5f;
    uint32_t          cold       = m_palette.cold | 0xFF000000u;
    Byte              execute    = (m_execute.size()   == kCount) ? m_execute[address]   : (Byte) 0;
    Byte              read       = (m_read.size()      == kCount) ? m_read[address]      : (Byte) 0;
    Byte              write      = (m_write.size()     == kCount) ? m_write[address]     : (Byte) 0;
    Byte              unwritten  = (m_unwritten.size() == kCount) ? m_unwritten[address] : (Byte) 0;
    Byte              changed    = (m_changed.size()   == kCount) ? m_changed[address]   : (Byte) 0;
    uint32_t          hue        = 0;
    Byte              level      = 0;
    float             floor      = kFaintest;



    switch (GetCellKind (address))
    {
    case CellKind::Code:
    case CellKind::Operand:       hue = m_palette.execute;                                                           level = execute;                          break;
    case CellKind::Read:          hue = m_palette.read;                                                              level = read;                             break;
    case CellKind::Written:       hue = m_palette.write;                                                             level = write;                            break;
    case CellKind::SelfModifying: hue = m_palette.selfModifying;                                                     level = std::max ({ execute, read, write }); break;
    case CellKind::Unwritten:     hue = m_palette.unwritten;                                                         level = unwritten;                        break;
    case CellKind::Changed:       hue = m_palette.changed;                                                           level = changed;                          break;
    default:                                                                                                                                                   break;
    }

    if (level == 0)
    {
        return false;
    }

    mark = (float) level / kTop;

    gradient = GetScaleStops (GetCellKind (address));

    if (!gradient.empty())
    {
        return true;
    }

    floor     = (GetCellKind (address) == CellKind::Unwritten) ? kUnwrittenFaintest : kFaintest;
    hue      |= 0xFF000000u;
    gradient  = { DxuiColor::Mix (cold, hue, floor) | 0xFF000000u,
                  DxuiColor::Mix (cold, hue, floor + (1.0f - floor) * kMiddle) | 0xFF000000u,
                  hue };
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetScaleStops
//
//  A kind's scale as the cells draw it: the heat column's stops for code,
//  an operand, a read or a write; none for the other kinds.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> HeatMapCells::GetScaleStops (CellKind kind) const
{
    std::vector<uint32_t>  stops;
    HeatStrip::Kind        strip = HeatStrip::Kind::Run;



    switch (kind)
    {
    case CellKind::Code:
    case CellKind::Operand: strip = HeatStrip::Kind::Run; break;
    case CellKind::Read:    strip = HeatStrip::Kind::Read;    break;
    case CellKind::Written: strip = HeatStrip::Kind::Write;   break;
    default:                return stops;
    }

    for (uint32_t stop : HeatStrip::GetScaleStops (strip))
    {
        stops.push_back (stop);
    }

    return stops;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetValueInk
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapCells::GetValueInk (Word address, uint32_t cell, uint32_t plain) const
{
    uint16_t  key = GetRegionAt (address);



    if (MemoryRegionKeys::IsRom (key) && m_regionColors.rom != 0)
    {
        return DebuggerTextColors::GetReadable (m_regionColors.rom | 0xFF000000u, cell | 0xFF000000u);
    }

    if (MemoryRegionKeys::IsIo (key) && m_regionColors.io != 0)
    {
        return DebuggerTextColors::GetReadable (m_regionColors.io | 0xFF000000u, cell | 0xFF000000u);
    }

    return plain;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetSymbolAt
//
//  The marks are checked first, so a cell with no symbol costs a lookup in
//  a table rather than a search of the symbols.
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapCells::GetSymbolAt (Word address) const
{
    std::string  name;



    if (m_symbols == nullptr || m_symbolMarks.size() != (size_t) kAddressCount || m_symbolMarks[address] == 0)
    {
        return {};
    }

    if (!m_symbols->TryGetNameAt (HeatMapOptions::GetCpuAddress (m_shownBank, address), name))
    {
        name.clear();
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::IsMarked
//
//  Whether an outline goes around the cell: the PC, the stack pointer, the
//  address found, or a breakpoint on the CPU address the cell is.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapCells::IsMarked (Word address) const
{
    Word  cpu = HeatMapOptions::GetCpuAddress (m_shownBank, address);



    return m_pc == address || m_stack == address || m_found == address ||
           std::ranges::any_of (m_breakpoints, [cpu] (const Breakpoint & each) { return cpu >= each.first && cpu <= each.last; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::CanMerge
//
//  The bytes, the opcodes and their forms are known.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapCells::CanMerge() const
{
    return m_values.size() == (size_t) kAddressCount && m_opcodes.size() == (size_t) kAddressCount && m_opcodeForms != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCells::GetMergeLength
//
//  The length of the instruction whose opcode is at an address, when it is
//  drawn as one cell: a byte run as an opcode, drawn in the code color, with
//  its form and every byte of it known. Zero otherwise. Only while CanMerge.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapCells::GetMergeLength (Word address) const
{
    int16_t  stored = m_values[address];
    int      length = 0;



    if (stored < 0 || m_opcodes[address] == 0 || m_opcodeForms->size() <= (size_t) (Byte) stored ||
        (*m_opcodeForms)[(Byte) stored].empty() || GetCellKind (address) != CellKind::Code)
    {
        return 0;
    }

    length = HeatMapCellText::GetInstructionLength ((*m_opcodeForms)[(Byte) stored]);

    for (int i = 1; i < length; i++)
    {
        if (m_values[(Word) (address + i)] < 0)
        {
            return 0;
        }
    }

    return length;
}
