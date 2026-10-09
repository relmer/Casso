#include "Pch.h"

#include "Ui/Debugger/BreakpointColumns.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetHeading
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BreakpointColumns::GetHeading (Column column)
{
    static constexpr LPCWSTR  kHeadings[] = { L"Name", L"Condition", L"Hit count", L"Kind", L"Trigger", L"Symbol", L"When hit", L"Function", L"File", L"Address", L"Data" };



    return (column >= Column::Name && column < Column::Count) ? kHeadings[(size_t) column] : L"";
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
    const BreakpointInfo  & info           = bp.info;
    Cells                   cells;
    std::string             file;
    int                     line           = 0;
    bool                    isOnSourceLine = false;
    std::string             sourceLine;



    isOnSourceLine = TryGetSourceLine (snapshot, bp.id, file, line);

    if (isOnSourceLine)
    {
        sourceLine = std::format ("{}, line {}", file, line);
        cells[(size_t) Column::File] = std::format ("{}:{}", file, line);
    }

    cells[(size_t) Column::Name]      = GetName (bp, sourceLine);
    cells[(size_t) Column::Condition] = (info.kind == BreakpointKind::Register) ? std::string() : info.condition;
    cells[(size_t) Column::HitCount]  = info.stops ? std::to_string (info.hits) : std::format ("{} (count only)", info.hits);
    cells[(size_t) Column::Kind]      = GetKindText    (GetRowKind (info));
    cells[(size_t) Column::Trigger]   = GetTriggerText (GetRowKind (info));
    cells[(size_t) Column::Symbol]    = HasAddress (info) ? bp.label : std::string();
    cells[(size_t) Column::WhenHit]   = GetWhenHit (info);
    cells[(size_t) Column::Function]  = (info.kind == BreakpointKind::Address) ? bp.label : std::string();
    cells[(size_t) Column::Address]   = HasAddress (info) ? GetRange (info) : std::string();
    cells[(size_t) Column::Data]      = GetData (info);

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetRowKind
//
//  A data breakpoint's kind is its access. Every execution breakpoint is
//  Execution: a symbol at the address does not make it a function
//  breakpoint, since the symbol may label a loop or data as well as a
//  routine, and the engine keeps the address, not how it was given.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointColumns::RowKind BreakpointColumns::GetRowKind (const BreakpointInfo & info)
{
    static constexpr RowKind  kDataKinds[] = { RowKind::DataRead, RowKind::DataWrite, RowKind::DataReadOrWrite };



    switch (info.kind)
    {
    case BreakpointKind::Opcode:      return RowKind::Opcode;
    case BreakpointKind::Register:    return RowKind::Register;
    case BreakpointKind::Memory:      return kDataKinds[(size_t) info.access];
    case BreakpointKind::Io:          return RowKind::Io;
    case BreakpointKind::Brk:         return RowKind::Brk;
    case BreakpointKind::Interrupt:   return RowKind::Interrupt;
    case BreakpointKind::MemoryValue: return RowKind::DataValue;
    case BreakpointKind::Address:     break;
    }

    return RowKind::Execution;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetKindText
//
//  What the Kind column shows, in sentence case: what the breakpoint
//  watches.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointColumns::GetKindText (RowKind kind)
{
    static constexpr const char *  kTexts[] = { "Execution", "Memory", "Memory", "Memory", "Memory",
                                                "Register", "Opcode", "I/O", "BRK", "Interrupt" };



    static_assert (std::size (kTexts) == (size_t) RowKind::Count, "one text for each kind");

    return (kind >= RowKind::Execution && kind < RowKind::Count) ? kTexts[(size_t) kind] : "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetTriggerText
//
//  What the Trigger column shows, in sentence case: what about the watched
//  thing stops the machine. An I/O breakpoint stops on any access, and the
//  interrupt breakpoint on either an IRQ or an NMI.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointColumns::GetTriggerText (RowKind kind)
{
    static constexpr const char *  kTexts[] = { "Execute", "Read", "Write", "Read or write", "Value",
                                                "Condition", "Execute", "Read or write", "Execute", "IRQ or NMI" };



    static_assert (std::size (kTexts) == (size_t) RowKind::Count, "one text for each kind");

    return (kind >= RowKind::Execution && kind < RowKind::Count) ? kTexts[(size_t) kind] : "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::GetOrder
//
//  Hit count and Address sort by number, Kind by the order RowKind lists
//  the kinds in; the rest by their text, ignoring case.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<size_t> BreakpointColumns::GetOrder (const DebuggerViewSnapshot & snapshot, Column column, bool descending)
{
    std::vector<size_t>       order (snapshot.breakpoints.size());
    std::vector<std::string>  keys  (snapshot.breakpoints.size());
    std::vector<RowKind>      kinds (snapshot.breakpoints.size());



    for (size_t i = 0; i < order.size(); i++)
    {
        order[i] = i;

        if (column == Column::Kind)
        {
            kinds[i] = GetRowKind (snapshot.breakpoints[i].info);
            continue;
        }

        keys[i] = GetCells (snapshot, snapshot.breakpoints[i])[(size_t) column];

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

        if (column == Column::Kind)
        {
            return descending ? kinds[b] < kinds[a] : kinds[a] < kinds[b];
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
    shown[(size_t) Column::Kind]      = true;
    shown[(size_t) Column::Trigger]   = true;
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
//  A choice saved under an earlier token is read through the columns' order
//  then. The newest token found wins, wherever it sits in the text.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointColumns::Shown BreakpointColumns::ParseShown (const std::string & text)
{
    Shown               shown      = GetDefaultShown();
    std::istringstream  in (text);
    std::string         token;
    unsigned            mask       = 0;
    int                 found      = 0;   // 0 none, 1 "bpcols", 2 "bpcols2", 3 "bpcols3"



    while (in >> token)
    {
        if (TryReadMask (token, kpszToken, mask))
        {
            shown = MakeShown (mask);
            found = 3;
        }
        else if (found < 3 && TryReadMask (token, kpszKindToken, mask))
        {
            shown = MigrateKindShown (mask);
            found = 2;
        }
        else if (found < 2 && TryReadMask (token, kpszOldToken, mask))
        {
            shown = MigrateShown (mask);
            found = 1;
        }
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::TryReadMask
//
//  The hex mask of a "token=mask" word, when the word is that token's.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointColumns::TryReadMask (
    const std::string  & token,
    const char         * pszToken,
    unsigned           & mask)
{
    std::string  prefix = std::string (pszToken) + "=";
    std::string  value  = token.substr (std::min (prefix.size(), token.size()));
    unsigned     read   = 0;
    bool         isHex  = false;



    if (!token.starts_with (prefix) || value.empty())
    {
        return false;
    }

    isHex = std::from_chars (value.data(), value.data() + value.size(), read, 16).ptr == value.data() + value.size();

    if (!isHex)
    {
        return false;
    }

    mask = read;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::MakeShown
//
//  One bit a column, in the columns' order. Name shows whether or not its
//  bit is set.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointColumns::Shown BreakpointColumns::MakeShown (unsigned mask)
{
    Shown  shown = {};



    for (size_t i = 0; i < kCount; i++)
    {
        shown[i] = (mask & (1u << i)) != 0;
    }

    shown[(size_t) Column::Name] = true;
    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::MigrateShown
//
//  A mask saved under the old token, whose bits followed the columns' order
//  before Kind: Name, Condition, Labels, Hit count, Filter, When hit,
//  Function, File, Address and Data. Labels' bit is Symbol's now, and
//  Filter's is ignored. Kind and Trigger, which that choice could not hide,
//  show, as they do by default.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointColumns::Shown BreakpointColumns::MigrateShown (unsigned mask)
{
    static constexpr Column  kOldOrder[] = { Column::Name, Column::Condition, Column::Symbol, Column::HitCount, Column::Count,
                                             Column::WhenHit, Column::Function, Column::File, Column::Address, Column::Data };
    Shown                    shown       = {};



    for (size_t i = 0; i < std::size (kOldOrder); i++)
    {
        if (kOldOrder[i] == Column::Count)
        {
            continue;
        }

        shown[(size_t) kOldOrder[i]] = (mask & (1u << i)) != 0;
    }

    shown[(size_t) Column::Kind]    = true;
    shown[(size_t) Column::Trigger] = true;
    shown[(size_t) Column::Name]    = true;
    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns::MigrateKindShown
//
//  A mask saved under "bpcols2", whose bits followed the columns' order
//  before Trigger: Name, Condition, Hit count, Kind, Symbol, When hit,
//  Function, File, Address and Data. Trigger, which that choice could not
//  hide, shows with Kind when Kind shows, since it holds half of what Kind
//  held then.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointColumns::Shown BreakpointColumns::MigrateKindShown (unsigned mask)
{
    static constexpr Column  kKindOrder[] = { Column::Name, Column::Condition, Column::HitCount, Column::Kind, Column::Symbol,
                                              Column::WhenHit, Column::Function, Column::File, Column::Address, Column::Data };
    Shown                    shown        = {};



    for (size_t i = 0; i < std::size (kKindOrder); i++)
    {
        shown[(size_t) kKindOrder[i]] = (mask & (1u << i)) != 0;
    }

    shown[(size_t) Column::Trigger] = shown[(size_t) Column::Kind];
    shown[(size_t) Column::Name]    = true;
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
//  BreakpointColumns::GetData
//
//  What a data breakpoint watches; any other kind has nothing here.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointColumns::GetData (const BreakpointInfo & info)
{
    static constexpr const char *  kAccess[] = { "Read", "Write", "Read or write" };



    switch (info.kind)
    {
    case BreakpointKind::Memory:      return std::format ("{} {}", kAccess[(size_t) info.access], GetRange (info));
    case BreakpointKind::MemoryValue: return std::format ("{} = ${:02X}", GetRange (info), info.value.value_or (0));
    case BreakpointKind::Io:          return "I/O " + GetRange (info);
    default:                          return std::string();
    }
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





