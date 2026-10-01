#include "Pch.h"

#include "Ui/Debugger/BreakpointColumns.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetHeading
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BreakpointColumns::GetHeading (Column column)
{
    static constexpr LPCWSTR  kHeadings[] = { L"Name", L"Condition", L"Hit count", L"Kind", L"Address", L"Label", L"File", L"When hit" };



    return (column < Column::Count) ? kHeadings[(size_t) column] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetCells
//
//  A register breakpoint's condition is what it is, so it is the name and
//  its Condition column stays empty.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointColumns::Cells BreakpointColumns::GetCells (const DebuggerViewSnapshot & snapshot, const DebuggerViewSnapshot::BreakpointLine & bp)
{
    const BreakpointInfo  & info       = bp.info;
    Cells                   cells;
    std::string             file;
    int                     line       = 0;
    std::string             sourceLine;



    if (TryGetSourceLine (snapshot, bp.id, file, line))
    {
        sourceLine = std::format ("{}, line {}", file, line);
        cells[(size_t) Column::File] = std::format ("{}:{}", file, line);
    }

    cells[(size_t) Column::Name]      = GetName (bp, sourceLine);
    cells[(size_t) Column::Condition] = (info.kind == BreakpointKind::Register) ? std::string() : info.condition;
    cells[(size_t) Column::HitCount]  = info.stops ? std::to_string (info.hits) : std::format ("{} (count only)", info.hits);
    cells[(size_t) Column::Kind]      = GetKind (info);
    cells[(size_t) Column::Address]   = HasAddress (info) ? GetRange (info) : std::string();
    cells[(size_t) Column::Label]     = HasAddress (info) ? bp.label : std::string();
    cells[(size_t) Column::WhenHit]   = GetWhenHit (info);

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetOrder
//
//  Hit count and Address sort by number; the rest by their text, ignoring
//  case.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<size_t> BreakpointColumns::GetOrder (const DebuggerViewSnapshot & snapshot, Column column, bool descending)
{
    std::vector<size_t>       order (snapshot.breakpoints.size());
    std::vector<std::string>  keys  (snapshot.breakpoints.size());



    for (size_t i = 0; i < order.size(); i++)
    {
        order[i] = i;
        keys[i]  = GetCells (snapshot, snapshot.breakpoints[i])[(size_t) column];

        std::transform (keys[i].begin(), keys[i].end(), keys[i].begin(), [] (char ch) { return (char) std::tolower ((unsigned char) ch); });
    }

    std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b)
    {
        const BreakpointInfo  & left  = snapshot.breakpoints[descending ? b : a].info;
        const BreakpointInfo  & right = snapshot.breakpoints[descending ? a : b].info;

        if (column == Column::HitCount)
        {
            return left.hits < right.hits;
        }

        if (column == Column::Address && HasAddress (left) && HasAddress (right))
        {
            return left.address < right.address;
        }

        return descending ? keys[b] < keys[a] : keys[a] < keys[b];
    });

    return order;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetDefaultShown
//
////////////////////////////////////////////////////////////////////////////////

BreakpointColumns::Shown BreakpointColumns::GetDefaultShown()
{
    Shown  shown = {};



    shown[(size_t) Column::Name]      = true;
    shown[(size_t) Column::Condition] = true;
    shown[(size_t) Column::HitCount]  = true;
    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::FormatShown
//
//  One bit a column, in hex, after a space so it follows the other open
//  views' tokens.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointColumns::FormatShown (const Shown & shown)
{
    unsigned  mask = 0;



    if (shown == GetDefaultShown())
    {
        return std::string();
    }

    for (size_t i = 0; i < kCount; i++)
    {
        mask |= shown[i] ? (1u << i) : 0u;
    }

    return std::format (" {}={:X}", kpszToken, mask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::ParseShown
//
////////////////////////////////////////////////////////////////////////////////

BreakpointColumns::Shown BreakpointColumns::ParseShown (const std::string & text)
{
    Shown               shown  = GetDefaultShown();
    std::istringstream  in (text);
    std::string         token;
    std::string         prefix = std::string (kpszToken) + "=";
    unsigned            mask   = 0;



    while (in >> token)
    {
        std::string  value = token.substr (std::min (prefix.size(), token.size()));
        bool         isHex = false;

        if (!token.starts_with (prefix) || value.empty())
        {
            continue;
        }

        isHex = std::from_chars (value.data(), value.data() + value.size(), mask, 16).ptr == value.data() + value.size();

        if (!isHex)
        {
            continue;
        }

        for (size_t i = 0; i < kCount; i++)
        {
            shown[i] = (mask & (1u << i)) != 0;
        }

        shown[(size_t) Column::Name] = true;
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::TryGetSourcePlace
//
//  A breakpoint set from source knows its line. Any other at an address
//  takes the first line that produced code there.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointColumns::TryGetSourcePlace (
    const DebuggerViewSnapshot                  & snapshot,
    const DebuggerViewSnapshot::BreakpointLine  & bp,
    int                                         & fileId,
    int                                         & line)
{
    if (!snapshot.source.has_value())
    {
        return false;
    }

    for (const auto & [file, lineNumber, breakpointId] : snapshot.source->breakpointLines)
    {
        if (breakpointId == bp.id)
        {
            fileId = file;
            line   = lineNumber;
            return true;
        }
    }

    if (bp.info.kind != BreakpointKind::Address || snapshot.source->lineAddresses == nullptr)
    {
        return false;
    }

    for (const auto & [place, address] : *snapshot.source->lineAddresses)
    {
        if (address == bp.info.address)
        {
            fileId = place.first;
            line   = place.second;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetName
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointColumns::GetName (const DebuggerViewSnapshot::BreakpointLine & bp, const std::string & sourceLine)
{
    const BreakpointInfo  & info  = bp.info;
    std::string             range = GetRange (info);
    std::string             named = bp.label.empty() ? range : bp.label + " " + range;



    switch (info.kind)
    {
    case BreakpointKind::Address:     return sourceLine.empty() ? named : sourceLine;
    case BreakpointKind::Opcode:      return std::format ("Opcode ${:02X}", info.opcode);
    case BreakpointKind::Register:    return info.condition;
    case BreakpointKind::Io:          return "I/O " + range;
    case BreakpointKind::Brk:         return "BRK";
    case BreakpointKind::Interrupt:   return "Interrupt";
    case BreakpointKind::MemoryValue: return std::format ("{} = ${:02X}", named, info.value.value_or (0));
    case BreakpointKind::Memory:      return named;
    }

    return named;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetKind
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointColumns::GetKind (const BreakpointInfo & info)
{
    static constexpr const char *  kAccess[] = { "Data read", "Data write", "Data read or write" };



    switch (info.kind)
    {
    case BreakpointKind::Address:     return "Address";
    case BreakpointKind::Opcode:      return "Opcode";
    case BreakpointKind::Register:    return "Register";
    case BreakpointKind::Io:          return "I/O";
    case BreakpointKind::Brk:         return "BRK";
    case BreakpointKind::Interrupt:   return "Interrupt";
    case BreakpointKind::MemoryValue: return "Data value";
    case BreakpointKind::Memory:      return kAccess[(size_t) info.access];
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetRange
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointColumns::GetRange (const BreakpointInfo & info)
{
    return (info.last > info.address) ? std::format ("${:04X}-${:04X}", info.address, info.last)
                                      : std::format ("${:04X}", info.address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetWhenHit
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointColumns::GetWhenHit (const BreakpointInfo & info)
{
    if (!info.stops)
    {
        return "Count";
    }

    return info.temporary ? "Break once" : "Break";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::HasAddress
//
//  An opcode, a register condition, BRK and an interrupt stop anywhere.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointColumns::HasAddress (const BreakpointInfo & info)
{
    return info.kind == BreakpointKind::Address || info.kind == BreakpointKind::Memory ||
           info.kind == BreakpointKind::Io      || info.kind == BreakpointKind::MemoryValue;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::TryGetSourceLine
//
//  The file and line a breakpoint sits on, from the loaded debug file.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointColumns::TryGetSourceLine (const DebuggerViewSnapshot & snapshot, int id, std::string & file, int & line)
{
    if (!snapshot.source.has_value())
    {
        return false;
    }

    for (const auto & [fileId, lineNumber, breakpointId] : snapshot.source->breakpointLines)
    {
        if (breakpointId != id)
        {
            continue;
        }

        for (const DebugSourceFile & source : snapshot.source->files)
        {
            if (source.id == fileId)
            {
                file = source.name;
                line = lineNumber;
                return true;
            }
        }
    }

    return false;
}





