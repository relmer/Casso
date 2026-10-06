#include "Pch.h"

#include "Ui/Debugger/ColorLegend.h"





//  The panes, in the order the legend lists them.
static constexpr const wchar_t * s_kpszDisassembly = L"Disassembly";
static constexpr const wchar_t * s_kpszValues      = L"Registers, watch and stack";
static constexpr const wchar_t * s_kpszCallStack   = L"Call stack";
static constexpr const wchar_t * s_kpszMemory      = L"Memory";
static constexpr const wchar_t * s_kpszMemoryMap   = L"Memory map";
static constexpr const wchar_t * s_kpszDiskHead    = L"Disk head";
static constexpr const wchar_t * s_kpszStatusBar   = L"Status bar";





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend::GetEntries
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<ColorLegend::Entry> & ColorLegend::GetEntries()
{
    static const std::vector<Entry>  kEntries =
    {
        { s_kpszDisassembly, Meaning::PcMarker,           Swatch::Marker  },
        { s_kpszDisassembly, Meaning::PcRow,              Swatch::Row     },
        { s_kpszDisassembly, Meaning::NavigatedRow,       Swatch::Row     },
        { s_kpszDisassembly, Meaning::TargetRow,          Swatch::Row     },
        { s_kpszDisassembly, Meaning::BranchTaken,        Swatch::Line    },
        { s_kpszDisassembly, Meaning::BranchNotTaken,     Swatch::Line    },
        { s_kpszDisassembly, Meaning::BreakpointEnabled,  Swatch::Dot     },
        { s_kpszDisassembly, Meaning::BreakpointDisabled, Swatch::Ring    },
        { s_kpszDisassembly, Meaning::BreakpointHover,    Swatch::Dot     },
        { s_kpszDisassembly, Meaning::Changed,            Swatch::Text    },
        { s_kpszDisassembly, Meaning::Annotation,         Swatch::Text    },
        { s_kpszDisassembly, Meaning::Result,             Swatch::Text    },
        { s_kpszValues,      Meaning::Changed,            Swatch::Text    },
        { s_kpszValues,      Meaning::PreviousAutoWatch,  Swatch::Text    },
        { s_kpszValues,      Meaning::DisabledWatch,      Swatch::Text    },
        { s_kpszCallStack,   Meaning::UnverifiedFrame,    Swatch::Text    },
        { s_kpszCallStack,   Meaning::LastReturn,         Swatch::Text    },
        { s_kpszMemory,      Meaning::Changed,            Swatch::Text    },
        { s_kpszMemory,      Meaning::RomByte,            Swatch::Text    },
        { s_kpszMemory,      Meaning::IoByte,             Swatch::Text    },
        { s_kpszMemory,      Meaning::LcBank1Box,         Swatch::Outline },
        { s_kpszMemory,      Meaning::LcBank2Box,         Swatch::Outline },
        { s_kpszMemory,      Meaning::AuxBox,             Swatch::Outline },
        { s_kpszMemoryMap,   Meaning::MapMain,            Swatch::Fill    },
        { s_kpszMemoryMap,   Meaning::MapAux,             Swatch::Fill    },
        { s_kpszMemoryMap,   Meaning::MapLcBank1,         Swatch::Fill    },
        { s_kpszMemoryMap,   Meaning::MapLcBank2,         Swatch::Fill    },
        { s_kpszMemoryMap,   Meaning::MapRom,             Swatch::Fill    },
        { s_kpszMemoryMap,   Meaning::MapSlotRom,         Swatch::Fill    },
        { s_kpszMemoryMap,   Meaning::MapIo,              Swatch::Fill    },
        { s_kpszDiskHead,    Meaning::HeadMotorOff,       Swatch::Fill    },
        { s_kpszDiskHead,    Meaning::HeadMoving,         Swatch::Fill    },
        { s_kpszDiskHead,    Meaning::HeadSettled,        Swatch::Fill    },
        { s_kpszDiskHead,    Meaning::LampLit,            Swatch::Fill    },
        { s_kpszStatusBar,   Meaning::HistoryEmpty,       Swatch::Fill    },
        { s_kpszStatusBar,   Meaning::HistoryFull,        Swatch::Fill    },
    };



    return kEntries;
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
    case Meaning::HeadMotorOff:       return L"Head, with the motor off";
    case Meaning::HeadMoving:         return L"Head stepping to a track";
    case Meaning::HeadSettled:        return L"Head settled on its track";
    case Meaning::LampLit:            return L"Phase magnet or motor on";
    case Meaning::HistoryEmpty:       return L"History buffer empty";
    case Meaning::HistoryFull:        return L"History buffer full; the oldest history makes room";
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
    case Meaning::HeadMotorOff:       return palette.muted;
    case Meaning::HeadMoving:         return palette.flash;
    case Meaning::HeadSettled:        return palette.accent;
    case Meaning::LampLit:            return palette.accent;
    case Meaning::HistoryEmpty:       return palette.meterEmpty;
    case Meaning::HistoryFull:        return palette.meterFull;
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
