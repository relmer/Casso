#pragma once

#include "Ui/Debugger/DebuggerTextColors.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegend
//
//  What each color the debugger window draws with means, said once: the
//  same sentence is a hovered element's tip and its line in the Colors
//  legend, and the swatch beside it is the color the theme in force gives
//  it. Syntax colors are not listed; they mean what the text says.
//
////////////////////////////////////////////////////////////////////////////////

class ColorLegend
{
public:
    enum class Meaning
    {
        PcMarker,
        PcRow,
        NavigatedRow,
        TargetRow,
        BranchTaken,
        BranchNotTaken,
        BreakpointEnabled,
        BreakpointDisabled,
        BreakpointHover,
        Changed,
        Annotation,
        Result,
        PreviousAutoWatch,
        DisabledWatch,
        UnverifiedFrame,
        LastReturn,
        RomByte,
        IoByte,
        LcBank1Box,
        LcBank2Box,
        AuxBox,
        MapMain,
        MapAux,
        MapLcBank1,
        MapLcBank2,
        MapRom,
        MapSlotRom,
        MapIo,
        HeatCode,
        HeatOperand,
        HeatRead,
        HeatWrite,
        HeatSelfModifying,
        HeatSymbol,
        HeatPc,
        HeatStack,
        HeatBreakpoint,
        HeatReadWatch,
        HeatWriteWatch,
        HeadMotorOff,
        HeadMoving,
        HeadSettled,
        LampLit,
        HistoryEmpty,
        HistoryFull,
        HeatUnwritten,
        HeatChanged,
        Count,
    };

    //  How a meaning's swatch is drawn: a filled square for a fill or a
    //  bar, a sample of text on a row's fill, a sample of text in a text
    //  color, the PC's arrow, a dot, a ring, a short line, or a square's
    //  outline.
    enum class Swatch
    {
        Fill,
        Row,
        Text,
        Marker,
        Dot,
        Ring,
        Line,
        Outline,
    };

    //  Every color a meaning can take, which the window gathers from the theme
    //  in force.
    struct Palette
    {
        DebuggerTextColors::Set  text;
        uint32_t                 background = 0;
        uint32_t                 pcMarker   = 0;
        uint32_t                 breakpoint = 0;
        uint32_t                 muted      = 0;
        uint32_t                 disabled   = 0;
        uint32_t                 accent     = 0;
        uint32_t                 flash      = 0;
        uint32_t                 meterEmpty = 0;
        uint32_t                 meterFull  = 0;
    };

    struct Entry
    {
        const wchar_t  * group   = nullptr;
        Meaning          meaning = Meaning::PcRow;
        Swatch           swatch  = Swatch::Fill;
    };

    //  The legend's lines, grouped by the pane they are seen in, in the order
    //  the panes are listed.
    static const std::vector<Entry> &  GetEntries();

    //  The one-line meaning, which is also the tip over what it colors.
    static const wchar_t *  GetText (Meaning meaning);

    //  The meaning's color in a palette.
    static uint32_t  GetArgb (Meaning meaning, const Palette & palette);

    //  The tip over the PC's branch arrow: whether the branch is taken, and,
    //  for a branch on a flag, which flag it tests and how that flag stands,
    //  "Branch taken: BNE branches while Z is clear, and Z is clear".
    static std::wstring  GetBranchTip (const std::string & instruction, bool isTaken, std::optional<Byte> p);

    //  The tip over the status bar's history meter, whose colors are fixed.
    static const wchar_t *  GetHistoryMeterTip();

    //  A swatch drawn as an image, for the shapes that are not text: a square
    //  filled or outlined, a dot, a ring, or a line across. Its edges are
    //  smoothed, so it scales down cleanly.
    static constexpr int  kSwatchPx = 48;

    static std::shared_ptr<DxuiIconImage>  MakeSwatchIcon (Swatch swatch, uint32_t argb);
    //  Lines joined one to a line, empty ones left out.
    static std::wstring  JoinLines (const std::vector<std::wstring> & lines);
};
