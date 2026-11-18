#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapTips.h"
#include "Ui/Debugger/Panes/HeatMapCellText.h"
#include "Ui/Debugger/Panes/HeatMapSections.h"
#include "Debugger/HeatMapSymbols.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/MemoryRegionKeys.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::FormatAmount
//
//  The value a level stands for, from the logarithmic scale the levels are
//  made on: within a few percent, so shown to two figures. While fading, a
//  rate a second, with level 255 meaning the top or more; while cumulative,
//  a count.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapTips::FormatAmount (Byte level) const
{
    constexpr double  kTopLevel = 255.0;
    constexpr double  kOneTenth = 10.0;
    constexpr double  kFigures  = 2.0;
    double            value     = std::expm1 ((double) level / kTopLevel * std::log1p (m_top));
    double            scale     = 1.0;
    uint64_t          rounded   = 0;
    std::wstring      grouped;



    if (m_top <= 0.0 || level == 0)
    {
        return {};
    }

    if (!m_model.GetOptions().cumulative && value < 1.0 / kOneTenth)
    {
        return L"under 0.1/s";
    }

    if (!m_model.GetOptions().cumulative && value < kOneTenth)
    {
        return std::format (L"{:.1f}/s", value);
    }

    scale   = std::pow (kOneTenth, std::max (0.0, std::floor (std::log10 (std::max (value, 1.0))) + 1.0 - kFigures));
    rounded = (uint64_t) std::max (1.0, std::round (value / scale) * scale);
    grouped = DebuggerStatusText::FormatCount (rounded);

    if (m_model.GetOptions().cumulative)
    {
        return (rounded == 1) ? std::wstring (L"once") : grouped + L" times";
    }

    return grouped + ((level == (Byte) kTopLevel) ? L"+/s" : L"/s");
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::GetTipText
//
//  "$C65E  executed 120/s, read 3.5/s" while fading; "$C65E  executed 1,200
//  times, read once" while cumulative; "$C65E  untouched" when nothing did.
//  In a bank's view the address is where in the bank the cell is, "Aux RAM
//  $2000". A byte only ever run as an operand is "executed as an operand".
//  A line follows for each overlay on the cell (DescribeMarks), then a line
//  for its last writer and one for its last reader while the machine looks
//  them up. Until it has for the cell, the two lines end in an ellipsis.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapTips::GetTipText (Word address) const
{
    constexpr size_t  kCount   = (size_t) HeatMapCells::kAddressCount;
    std::wstring      contents = GetContentsLine (address);
    std::wstring      text     = HeatMapOptions::DescribeLocation (m_model.GetShownBank(), address, m_hasAux);
    std::wstring      amount;
    int               shown    = 0;
    const std::pair<const wchar_t *, const std::vector<Byte> *>  kinds[] =
    {
        { m_model.IsOperand (address) ? L"executed as an operand" : L"executed", &m_model.GetExecuteLevels()   },
        { L"read",                                                        &m_model.GetReadLevels()      },
        { L"written",                                                     &m_model.GetWriteLevels()     },
        { L"changed",                                                     &m_model.GetChangedLevels()   },
        { L"read before written",                                         &m_model.GetUnwrittenLevels() },
    };



    for (const auto & [name, levels] : kinds)
    {
        Byte  level = (levels->size() == kCount) ? (*levels)[address] : (Byte) 0;



        if (level == 0)
        {
            continue;
        }

        amount  = FormatAmount (level);
        text   += (shown == 0) ? L"  " : L", ";
        text   += name;
        text   += amount.empty() ? std::wstring() : L" " + amount;
        shown++;
    }

    if (shown == 0)
    {
        text += L"  untouched";
    }

    text += DescribeMarks (address);

    if (m_model.GetEditedLevels().size() == kCount && m_model.GetEditedLevels()[address] != 0)
    {
        text += L"\nEdited in the debugger";
    }

    if (m_hoverAccess != nullptr && m_hoverAccess->address == address && m_hoverAccess->bank == m_model.GetShownBank())
    {
        text += L"\n" + HeatAccessJump::Describe (true,  m_hoverAccess->writer);
        text += L"\n" + HeatAccessJump::Describe (false, m_hoverAccess->reader);
    }
    else if (m_isLookingUp)
    {
        text += std::wstring (L"\nLast written by ") + s_kchEllipsis;
        text += std::wstring (L"\nLast read by ")    + s_kchEllipsis;
    }

    return contents.empty() ? text : contents + L"\n" + text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::GetContentsLine
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapTips::GetContentsLine (Word address) const
{
    int16_t       stored      = (m_model.GetValues().size() == (size_t) HeatMapCells::kAddressCount) ? m_model.GetValues()[address] : (int16_t) -1;
    std::wstring  symbol      = TextEncoding::NarrowToWide (m_model.GetSymbolAt (address));
    std::wstring  instruction = DescribeInstruction (address);



    if (stored < 0)
    {
        return symbol;
    }

    if (!instruction.empty())
    {
        instruction.erase (0, 1);
        return symbol.empty() ? instruction : symbol + L": " + instruction;
    }

    return std::format (L"{}: {:02X}", symbol.empty() ? std::wstring (L"Value") : symbol, (Byte) stored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::GetTipTag
//
//  The PC and the stack pointer move as the machine runs, so they are a tag
//  at the end of the tip's first line rather than separate lines that come
//  and go.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapTips::GetTipTag (Word address, std::wstring & tag, uint32_t & argb) const
{
    bool  isPc    = m_model.GetPc()    == address;
    bool  isStack = m_model.GetStack() == address;



    tag  = (isPc && isStack) ? L"PC, SP" : isPc ? L"PC" : isStack ? L"SP" : L"";
    argb = isPc ? m_model.GetPalette().pc : isStack ? m_model.GetPalette().stack : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::GetTipFrame
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapTips::GetTipFrame (Word address, std::wstring & label, uint32_t & argb) const
{
    uint16_t  key = m_model.GetRegionAt (address);



    label.clear();
    argb = 0;

    if (m_model.GetRegions().size() != (size_t) HeatMapCells::kAddressCount)
    {
        return;
    }

    if (key == 0)
    {
        label = HeatMapSections::GetRamLabel (address);
        return;
    }

    if (key == MemoryRegionKeys::kRegionAux && HeatMapSections::GetRamLabel (address) != L"RAM")
    {
        label = L"Aux RAM: " + HeatMapSections::GetRamLabel (address);
        argb  = MemoryRegionKeys::GetColor (key, m_model.GetRegionColors());
        return;
    }

    label = MemoryRegionKeys::GetRomLabel (key, address, m_model.GetSlotCards(), m_model.GetMachineId());
    argb  = MemoryRegionKeys::GetColor (key, m_model.GetRegionColors());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::GetTipLead
//
//  The label of the kind that drew the cell, which the legend lists as well
//  ("Value changed"), and the cell's color as drawn, so the reader matches
//  the swatch to the map.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapTips::TipLead HeatMapTips::GetTipLead (Word address) const
{
    constexpr const wchar_t  * kpszUntouched = L"Untouched";
    constexpr const wchar_t  * kpszBlended   = L"Blend: mixed from the kinds that touched it";
    TipLead                    lead;



    lead.argb = m_model.HasCellColors() ? m_model.GetCellColor (address) : (m_model.GetPalette().cold | 0xFF000000u);

    switch (m_model.GetCellKind (address))
    {
    case HeatMapCells::CellKind::Code:
    case HeatMapCells::CellKind::Operand:       lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatCode);          break;
    case HeatMapCells::CellKind::Read:          lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatRead);          break;
    case HeatMapCells::CellKind::Written:       lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatWrite);         break;
    case HeatMapCells::CellKind::SelfModifying: lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatSelfModifying); break;
    case HeatMapCells::CellKind::Unwritten:     lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatUnwritten);     break;
    case HeatMapCells::CellKind::Changed:       lead.meaning = ColorLegend::GetLabel (ColorLegend::Meaning::HeatChanged);       break;
    case HeatMapCells::CellKind::Blended:       lead.meaning = kpszBlended;                                                     break;
    case HeatMapCells::CellKind::Untouched:     lead.meaning = kpszUntouched;                                                   break;
    }

    return lead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::DescribeMarks
//
//  A line each, every one starting a new line of the tip: "Self-modifying:
//  run as code and written", "PC", "Stack pointer", each breakpoint as
//  "HeatMapCells::Breakpoint #2", "Read watchpoint #3", "Write watchpoint #4" or "Read
//  and write watchpoint #5", and "Symbol COUT".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapTips::DescribeMarks (Word address) const
{
    constexpr size_t  kCount   = (size_t) HeatMapCells::kAddressCount;
    Word              cpu      = HeatMapOptions::GetCpuAddress (m_model.GetShownBank(), address);
    bool              hasRun   = m_model.GetExecuteLevels().size() == kCount && m_model.GetExecuteLevels()[address] > 0;
    bool              hasWrite = m_model.GetWriteLevels().size()   == kCount && m_model.GetWriteLevels()[address]   > 0;
    std::wstring      text;
    std::string       name;



    if (hasRun && hasWrite)
    {
        text += L"\nSelf-modifying: run as code and written";
    }

    for (const HeatMapCells::Breakpoint & breakpoint : m_model.GetBreakpoints())
    {
        if (cpu < breakpoint.first || cpu > breakpoint.last)
        {
            continue;
        }

        switch (breakpoint.kind)
        {
        case HeatMapCells::BreakKind::Execute: text += std::format (L"\nBreakpoint #{}",                breakpoint.id); break;
        case HeatMapCells::BreakKind::Read:    text += std::format (L"\nRead data breakpoint #{}",           breakpoint.id); break;
        case HeatMapCells::BreakKind::Write:   text += std::format (L"\nWrite data breakpoint #{}",          breakpoint.id); break;
        default:                 text += std::format (L"\nRead and write data breakpoint #{}", breakpoint.id); break;
        }
    }

    if (m_model.GetSymbols() != nullptr && m_model.GetSymbols()->TryGetNameAt (cpu, name))
    {
        text += L"\nSymbol " + TextEncoding::NarrowToWide (name);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::DescribeInstruction
//
//  For a byte run as an opcode, a line with its instruction as the merged
//  cell writes it and the instruction's bytes, "LDA KBD  (AD 00 C0)"; empty
//  for any other byte, or one whose instruction's bytes are not all known.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapTips::DescribeInstruction (Word address) const
{
    constexpr size_t   kCount = (size_t) HeatMapCells::kAddressCount;
    int16_t            stored = (m_model.GetValues().size() == kCount) ? m_model.GetValues()[address] : (int16_t) -1;
    std::string        form;
    std::vector<Byte>  operands;
    std::wstring       bytes;
    int                length = 0;



    if (stored < 0 || m_model.GetOpcodes().size() != kCount || m_model.GetOpcodes()[address] == 0 || m_model.GetOpcodeForms() == nullptr ||
        m_model.GetOpcodeForms()->size() <= (size_t) (Byte) stored || (*m_model.GetOpcodeForms())[(Byte) stored].empty())
    {
        return {};
    }

    form   = (*m_model.GetOpcodeForms())[(Byte) stored];
    length = HeatMapCellText::GetInstructionLength (form);
    bytes  = std::format (L"{:02X}", (Byte) stored);

    for (int i = 1; i < length; i++)
    {
        int16_t  value = m_model.GetValues()[(Word) (address + i)];



        if (value < 0)
        {
            return {};
        }

        operands.push_back ((Byte) value);
        bytes += std::format (L" {:02X}", (Byte) value);
    }

    return L"\n" + HeatMapCellText::GetInstructionText (form, HeatMapOptions::GetCpuAddress (m_model.GetShownBank(), address), operands, m_model.GetSymbols().get()) + L"  (" + bytes + L")";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips::HasHoverAccess
//
//  The machine has looked up the cell's last writer and reader.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapTips::HasHoverAccess (Word address) const
{
    return m_hoverAccess != nullptr && m_hoverAccess->address == address && m_hoverAccess->bank == m_model.GetShownBank();
}
