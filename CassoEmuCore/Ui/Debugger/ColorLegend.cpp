#include "Pch.h"

#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/Panes/HeatMapView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::GetEntries
//
//  Each pane's lines together, in the order its info button lists them.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<ColorLegend::Entry> & ColorLegend::GetEntries()
{
    static const std::vector<Entry>  kEntries =
    {
        { Pane::Disassembly, Meaning::PcMarker,           Swatch::Marker  },
        { Pane::Disassembly, Meaning::PcRow,              Swatch::Row     },
        { Pane::Disassembly, Meaning::NavigatedRow,       Swatch::Row     },
        { Pane::Disassembly, Meaning::TargetRow,          Swatch::Row     },
        { Pane::Disassembly, Meaning::BranchTaken,        Swatch::Line    },
        { Pane::Disassembly, Meaning::BranchNotTaken,     Swatch::Line    },
        { Pane::Disassembly, Meaning::BreakpointEnabled,  Swatch::Dot     },
        { Pane::Disassembly, Meaning::BreakpointDisabled, Swatch::Ring    },
        { Pane::Disassembly, Meaning::BreakpointHover,    Swatch::Dot     },
        { Pane::Disassembly, Meaning::Changed,            Swatch::Text    },
        { Pane::Disassembly, Meaning::Annotation,         Swatch::Text    },
        { Pane::Disassembly, Meaning::Result,             Swatch::Text    },
        { Pane::Source,      Meaning::PcRow,              Swatch::Row     },
        { Pane::Source,      Meaning::BreakpointEnabled,  Swatch::Dot     },
        { Pane::Source,      Meaning::BreakpointDisabled, Swatch::Ring    },
        { Pane::Source,      Meaning::Result,             Swatch::Text    },
        { Pane::Registers,   Meaning::Changed,            Swatch::Text    },
        { Pane::Watch,       Meaning::Changed,            Swatch::Text    },
        { Pane::Watch,       Meaning::PreviousAutoWatch,  Swatch::Text    },
        { Pane::Watch,       Meaning::DisabledWatch,      Swatch::Text    },
        { Pane::Stack,       Meaning::Changed,            Swatch::Text    },
        { Pane::CallStack,   Meaning::UnverifiedFrame,    Swatch::Text    },
        { Pane::CallStack,   Meaning::LastReturn,         Swatch::Text    },
        { Pane::Memory,      Meaning::Changed,            Swatch::Text    },
        { Pane::Memory,      Meaning::RomByte,            Swatch::Text    },
        { Pane::Memory,      Meaning::IoByte,             Swatch::Text    },
        { Pane::Memory,      Meaning::MemoryUnread,       Swatch::Text    },
        { Pane::Memory,      Meaning::LcBank1Box,         Swatch::Outline },
        { Pane::Memory,      Meaning::LcBank2Box,         Swatch::Outline },
        { Pane::Memory,      Meaning::AuxBox,             Swatch::Outline },
        { Pane::MemoryMap,   Meaning::MapMain,            Swatch::Fill    },
        { Pane::MemoryMap,   Meaning::MapAux,             Swatch::Fill    },
        { Pane::MemoryMap,   Meaning::MapLcBank1,         Swatch::Fill    },
        { Pane::MemoryMap,   Meaning::MapLcBank2,         Swatch::Fill    },
        { Pane::MemoryMap,   Meaning::MapRom,             Swatch::Fill    },
        { Pane::MemoryMap,   Meaning::MapSlotRom,         Swatch::Fill    },
        { Pane::MemoryMap,   Meaning::MapIo,              Swatch::Fill    },
        { Pane::HeatMap,     Meaning::HeatCode,           Swatch::Fill    },
        { Pane::HeatMap,     Meaning::HeatOperand,        Swatch::Fill    },
        { Pane::HeatMap,     Meaning::HeatRead,           Swatch::Fill    },
        { Pane::HeatMap,     Meaning::HeatWrite,          Swatch::Fill    },
        { Pane::HeatMap,     Meaning::HeatSelfModifying,  Swatch::Fill    },
        { Pane::HeatMap,     Meaning::HeatSymbol,         Swatch::Fill    },
        { Pane::HeatMap,     Meaning::HeatPc,             Swatch::Outline },
        { Pane::HeatMap,     Meaning::HeatStack,          Swatch::Outline },
        { Pane::HeatMap,     Meaning::HeatBreakpoint,     Swatch::Outline },
        { Pane::HeatMap,     Meaning::HeatReadWatch,      Swatch::Outline },
        { Pane::HeatMap,     Meaning::HeatWriteWatch,     Swatch::Outline },
        { Pane::HeatMap,     Meaning::HeatUnwritten,      Swatch::Fill    },
        { Pane::HeatMap,     Meaning::HeatChanged,        Swatch::Fill    },
        { Pane::DiskHead,    Meaning::HeadMotorOff,       Swatch::Fill    },
        { Pane::DiskHead,    Meaning::HeadMoving,         Swatch::Fill    },
        { Pane::DiskHead,    Meaning::HeadSettled,        Swatch::Fill    },
        { Pane::DiskHead,    Meaning::LampLit,            Swatch::Fill    },
        { Pane::Breakpoints, Meaning::BreakpointEnabled,  Swatch::Dot     },
        { Pane::Breakpoints, Meaning::BreakpointDisabled, Swatch::Ring    },
        { Pane::StatusBar,   Meaning::HistoryEmpty,       Swatch::Fill    },
        { Pane::StatusBar,   Meaning::HistoryFull,        Swatch::Fill    },
    };



    return kEntries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::GetEntriesFor
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ColorLegend::Entry> ColorLegend::GetEntriesFor (Pane pane)
{
    std::vector<Entry>  entries;



    for (const Entry & entry : GetEntries())
    {
        if (entry.pane == pane)
        {
            entries.push_back (entry);
        }
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::GetPaneTitle
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ColorLegend::GetPaneTitle (Pane pane)
{
    switch (pane)
    {
    case Pane::Disassembly: return L"Disassembly";
    case Pane::Source:      return L"Source";
    case Pane::Registers:   return L"Registers";
    case Pane::Watch:       return L"Watch";
    case Pane::Stack:       return L"Stack";
    case Pane::CallStack:   return L"Call stack";
    case Pane::Memory:      return L"Memory";
    case Pane::MemoryMap:   return L"Memory map";
    case Pane::HeatMap:     return L"Heat map";
    case Pane::DiskHead:    return L"Disk head";
    case Pane::Breakpoints: return L"Breakpoints";
    case Pane::StatusBar:   return L"Status bar";
    case Pane::Count:       break;
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::GetText
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ColorLegend::GetText (Meaning meaning)
{
    switch (meaning)
    {
    case Meaning::PcMarker:           return L"Next instruction to run (PC); drag it to set the next statement";
    case Meaning::PcRow:              return L"Next instruction to run (PC)";
    case Meaning::NavigatedRow:       return L"Line brought into view by Go to, the call stack or a branch arrow";
    case Meaning::TargetRow:          return L"Where the branch at the PC goes";
    case Meaning::BranchTaken:        return L"Branch taken: the flags as they stand send the PC to the target";
    case Meaning::BranchNotTaken:     return L"Branch not taken: the flags as they stand fall through";
    case Meaning::BreakpointEnabled:  return L"Breakpoint";
    case Meaning::BreakpointDisabled: return L"Disabled breakpoint";
    case Meaning::BreakpointHover:    return L"Click to set a breakpoint here";
    case Meaning::Changed:            return L"Changed since the last stop";
    case Meaning::Annotation:         return L"What this instruction reads";
    case Meaning::Result:             return L"Result: what this instruction leaves behind";
    case Meaning::PreviousAutoWatch:  return L"Touched by the previous instruction";
    case Meaning::DisabledWatch:      return L"Disabled watch";
    case Meaning::UnverifiedFrame:    return L"Unverified frame: it may no longer be a live call";
    case Meaning::LastReturn:         return L"Last return: the call has returned and left the stack";
    case Meaning::RomByte:            return L"ROM, and the box around it";
    case Meaning::IoByte:             return L"I/O, and the box around it";
    case Meaning::LcBank1Box:         return L"Language Card bank 1 RAM";
    case Meaning::LcBank2Box:         return L"Language Card bank 2 RAM";
    case Meaning::AuxBox:             return L"Aux RAM";
    case Meaning::MapMain:            return L"Main RAM";
    case Meaning::MapAux:             return L"Aux RAM";
    case Meaning::MapLcBank1:         return L"Language Card bank 1";
    case Meaning::MapLcBank2:         return L"Language Card bank 2";
    case Meaning::MapRom:             return L"ROM";
    case Meaning::MapSlotRom:         return L"Slot ROM";
    case Meaning::MapIo:              return L"I/O";
    case Meaning::HeatCode:           return L"Run as code";
    case Meaning::HeatOperand:        return L"Run as code, but only ever as an instruction's operand";
    case Meaning::HeatRead:           return L"Read";
    case Meaning::HeatWrite:          return L"Written";
    case Meaning::HeatSelfModifying:  return L"Self-modifying: run as code and written, with Blend on";
    case Meaning::HeatSymbol:         return L"Has a symbol";
    case Meaning::HeatPc:             return L"PC";
    case Meaning::HeatStack:          return L"Stack pointer";
    case Meaning::HeatBreakpoint:     return L"Breakpoint";
    case Meaning::HeatReadWatch:      return L"Read watchpoint, or the read half of a read and write one";
    case Meaning::HeatWriteWatch:     return L"Write watchpoint, or the write half of a read and write one";
    case Meaning::HeadMotorOff:       return L"Head, with the motor off";
    case Meaning::HeadMoving:         return L"Head stepping to a track";
    case Meaning::HeadSettled:        return L"Head settled on its track";
    case Meaning::LampLit:            return L"Phase magnet or motor on";
    case Meaning::HistoryEmpty:       return L"History buffer empty";
    case Meaning::HistoryFull:        return L"History buffer full; the oldest history makes room";
    case Meaning::HeatUnwritten:      return L"Read before written: RAM read before anything wrote it since power-on";
    case Meaning::HeatChanged:        return L"Value changed: a write that stored a different value";
    case Meaning::MemoryUnread:       return L"Not read yet: shown when the window has read it";
    case Meaning::Count:              break;
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::GetArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ColorLegend::GetArgb (Meaning meaning, const Palette & palette)
{
    switch (meaning)
    {
    case Meaning::PcMarker:           return palette.pcMarker;
    case Meaning::PcRow:              return palette.text.pcRow;
    case Meaning::NavigatedRow:       return palette.text.navigatedRow;
    case Meaning::TargetRow:          return palette.text.targetRow;
    case Meaning::BranchTaken:        return palette.pcMarker;
    case Meaning::BranchNotTaken:     return palette.disabled;
    case Meaning::BreakpointEnabled:  return palette.breakpoint;
    case Meaning::BreakpointDisabled: return palette.breakpoint;
    case Meaning::BreakpointHover:    return palette.muted;
    case Meaning::Changed:            return palette.text.changed;
    case Meaning::Annotation:         return palette.text.annotation;
    case Meaning::Result:             return palette.text.result;
    case Meaning::PreviousAutoWatch:  return palette.muted;
    case Meaning::DisabledWatch:      return palette.muted;
    case Meaning::UnverifiedFrame:    return palette.muted;
    case Meaning::LastReturn:         return palette.muted;
    case Meaning::RomByte:            return palette.text.rom;
    case Meaning::IoByte:             return palette.text.io;
    case Meaning::LcBank1Box:         return palette.text.mapLcBank1;
    case Meaning::LcBank2Box:         return palette.text.mapLcBank2;
    case Meaning::AuxBox:             return palette.text.mapAux;
    case Meaning::MapMain:            return palette.text.mapMain;
    case Meaning::MapAux:             return palette.text.mapAux;
    case Meaning::MapLcBank1:         return palette.text.mapLcBank1;
    case Meaning::MapLcBank2:         return palette.text.mapLcBank2;
    case Meaning::MapRom:             return palette.text.mapRom;
    case Meaning::MapSlotRom:         return palette.text.mapSlotRom;
    case Meaning::MapIo:              return palette.text.mapIo;
    case Meaning::HeatCode:           return palette.text.syntax.mnemonic;
    case Meaning::HeatOperand:        return DxuiColor::Mix (palette.text.syntax.mnemonic | 0xFF000000u, palette.text.heatCold | 0xFF000000u, HeatMapView::kOperandDim);
    case Meaning::HeatRead:           return palette.text.annotation;
    case Meaning::HeatWrite:          return palette.text.changed;
    case Meaning::HeatSelfModifying:  return palette.text.heatSelfModifying;
    case Meaning::HeatSymbol:         return DxuiColor::Mix (palette.text.heatCold | 0xFF000000u, palette.text.syntax.symbol | 0xFF000000u, HeatMapView::kSymbolTint);
    case Meaning::HeatPc:             return palette.pcMarker;
    case Meaning::HeatStack:          return palette.text.heatStack;
    case Meaning::HeatBreakpoint:     return palette.breakpoint;
    case Meaning::HeatReadWatch:      return palette.text.heatReadWatch;
    case Meaning::HeatWriteWatch:     return palette.text.heatWriteWatch;
    case Meaning::HeadMotorOff:       return palette.muted;
    case Meaning::HeadMoving:         return palette.flash;
    case Meaning::HeadSettled:        return palette.accent;
    case Meaning::LampLit:            return palette.accent;
    case Meaning::HistoryEmpty:       return palette.meterEmpty;
    case Meaning::HistoryFull:        return palette.meterFull;
    case Meaning::HeatUnwritten:      return palette.text.heatUnwritten;
    case Meaning::HeatChanged:        return palette.text.heatChanged;
    case Meaning::MemoryUnread:       return palette.text.muted;
    case Meaning::Count:              break;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::GetBranchTip
//
//  The eight branches on a flag say which flag and how it stands; BBR and
//  BBS, which test a bit of a byte rather than a flag, say which bit; a
//  branch or jump that always goes says so.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ColorLegend::GetBranchTip (const std::string & instruction, bool isTaken, std::optional<Byte> p)
{
    struct FlagBranch
    {
        const char  * mnemonic = nullptr;
        wchar_t       flag     = 0;
        Byte          bit      = 0;
        bool          whenSet  = false;
    };

    static constexpr FlagBranch  kBranches[] =
    {
        { "BPL", L'N', 0x80, false },
        { "BMI", L'N', 0x80, true  },
        { "BVC", L'V', 0x40, false },
        { "BVS", L'V', 0x40, true  },
        { "BCC", L'C', 0x01, false },
        { "BCS", L'C', 0x01, true  },
        { "BNE", L'Z', 0x02, false },
        { "BEQ", L'Z', 0x02, true  },
    };

    constexpr size_t  kBitBranchLength = 4;
    std::string       mnemonic;
    std::wstring      wide;
    const wchar_t   * head = isTaken ? L"Branch taken" : L"Branch not taken";



    for (char ch : instruction)
    {
        if (ch == ' ' || ch == '\t')
        {
            if (!mnemonic.empty())
            {
                break;
            }

            continue;
        }

        mnemonic += (char) std::toupper ((unsigned char) ch);
    }

    wide = std::wstring (mnemonic.begin(), mnemonic.end());

    for (const FlagBranch & branch : kBranches)
    {
        if (mnemonic != branch.mnemonic)
        {
            continue;
        }

        if (!p.has_value())
        {
            return std::format (L"{}: {} branches while {} is {}", head, wide, branch.flag, branch.whenSet ? L"set" : L"clear");
        }

        return std::format (L"{}: {} branches while {} is {}, and {} is {}", head, wide, branch.flag, branch.whenSet ? L"set" : L"clear",
                            branch.flag, (*p & branch.bit) != 0 ? L"set" : L"clear");
    }

    if (mnemonic.size() == kBitBranchLength && (mnemonic.starts_with ("BBR") || mnemonic.starts_with ("BBS")))
    {
        return std::format (L"{}: {} branches while bit {} of its byte is {}", head, wide, wide.back(), mnemonic[2] == 'S' ? L"set" : L"clear");
    }

    if (mnemonic == "BRA")
    {
        return L"Branch taken: BRA always branches";
    }

    if (mnemonic == "JMP")
    {
        return L"Jump: JMP always goes to its target";
    }

    if (mnemonic == "JSR")
    {
        return L"Call: JSR always calls its target";
    }

    return head;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::GetHistoryMeterTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ColorLegend::GetHistoryMeterTip()
{
    return L"History buffer used: green while empty, blending to blue as it fills";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::MakeSwatchIcon
//
//  Each pixel's coverage of the shape, worked out from its distance to the
//  shape's edge, is its alpha: a dot and a ring are measured from the
//  center, a square, its outline and a line from their sides.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> ColorLegend::MakeSwatchIcon (Swatch swatch, uint32_t argb)
{
    constexpr float  kRadius  = 20.0f;
    constexpr float  kRing    = 5.0f;
    constexpr float  kHalfBar = 4.0f;
    constexpr float  kEdge    = 0.5f;
    constexpr float  kOpaque  = 255.0f;
    auto             image    = std::make_shared<DxuiIconImage>();
    float            half     = (float) kSwatchPx * 0.5f;



    image->width  = kSwatchPx;
    image->height = kSwatchPx;
    image->bgraPremul.assign ((size_t) (kSwatchPx * kSwatchPx), 0u);

    for (int y = 0; y < kSwatchPx; y++)
    {
        for (int x = 0; x < kSwatchPx; x++)
        {
            float  dx    = x + kEdge - half;
            float  dy    = y + kEdge - half;
            float  round = std::hypot (dx, dy);
            float  box   = (std::max) (std::fabs (dx), std::fabs (dy));
            float  a     = 0.0f;
            auto   ch    = [&a] (uint32_t c) { return (uint32_t) std::lround ((float) (c & 0xFF) * a); };

            switch (swatch)
            {
            case Swatch::Dot:     a = std::clamp (kRadius - round + kEdge, 0.0f, 1.0f); break;
            case Swatch::Ring:    a = std::clamp (kRadius - round + kEdge, 0.0f, 1.0f) - std::clamp (kRadius - kRing - round + kEdge, 0.0f, 1.0f); break;
            case Swatch::Outline: a = std::clamp (kRadius - box + kEdge, 0.0f, 1.0f) - std::clamp (kRadius - kRing - box + kEdge, 0.0f, 1.0f); break;
            case Swatch::Line:    a = std::clamp (kHalfBar - std::fabs (dy) + kEdge, 0.0f, 1.0f) * std::clamp (kRadius - std::fabs (dx) + kEdge, 0.0f, 1.0f); break;
            default:              a = std::clamp (kRadius - box + kEdge, 0.0f, 1.0f); break;
            }

            image->bgraPremul[(size_t) (y * kSwatchPx + x)] = ((uint32_t) std::lround (a * kOpaque) << 24) |
                                                              (ch (argb >> 16) << 16) | (ch (argb >> 8) << 8) | ch (argb);
        }
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::JoinLines
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ColorLegend::JoinLines (const std::vector<std::wstring> & lines)
{
    std::wstring  joined;



    for (const std::wstring & line : lines)
    {
        if (line.empty())
        {
            continue;
        }

        joined += joined.empty() ? line : L"\n" + line;
    }

    return joined;
}
