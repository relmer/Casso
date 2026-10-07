#include "Pch.h"

#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::Make
//
//  The keyword blue for a mnemonic, the control purple for a directive, the
//  type teal for a symbol, and Visual Studio's number, string and comment
//  colors. A listing's address takes the theme's text color and its bytes the
//  muted one; an operand's address takes Visual Studio's function color, apart
//  from an immediate's number. The rows the PC, a branch destination and a
//  navigation tint are opaque mixes of the page, and every text color reads on
//  each of them. A light page's result is a maroon, apart from the comment
//  green it follows.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerTextColors::Set DebuggerTextColors::Make (
    uint32_t background,
    uint32_t foreground,
    uint32_t muted,
    uint32_t resultText,
    uint32_t accent,
    uint32_t changedText,
    uint32_t romText,
    uint32_t ioText)
{
    Set                    set;
    bool                   dark    = IsDark (background);
    uint32_t               page    = background | 0xFF000000u;
    uint32_t               text    = dark ? foreground : DxuiColor::Mix (foreground, background, s_kLightTextSoftening);
    uint32_t               result  = (dark && resultText != 0) ? resultText : (dark ? 0xFF4EC9E0 : 0xFF8B1E5C);
    std::vector<uint32_t>  against;



    set.pcRow        = dark ? DxuiColor::Mix (page, 0xFFC8A000, s_kDarkPcRowMix)   : DxuiColor::Mix (page, 0xFFB08600, s_kLightPcRowMix);
    set.targetRow    = DxuiColor::Mix (page, accent | 0xFF000000u, dark ? s_kDarkTargetRowMix : s_kLightTargetRowMix);
    set.navigatedRow = dark ? DxuiColor::Mix (page, 0xFF3C8C3C, s_kDarkPcRowMix)   : DxuiColor::Mix (page, 0xFF5CB85C, s_kLightTargetRowMix);

    set.heatCold     = dark ? DxuiColor::Mix (page, 0xFFFFFFFF, s_kDarkHeatColdMix) : DxuiColor::Mix (page, 0xFF000000, s_kLightHeatColdMix);

    set.heatStack         = dark ? 0xFF35D0E0 : 0xFF00838F;
    set.heatReadWatch     = dark ? 0xFF7CDB6E : 0xFF1B7F2A;
    set.heatWriteWatch    = dark ? 0xFFFFA238 : 0xFFC05800;
    set.heatSelfModifying = dark ? 0xFFE060E0 : 0xFFA0179F;

    against = { page, set.pcRow, set.targetRow, set.navigatedRow };

    if (!dark)
    {
        against.push_back (GetRowShade (background));
    }

    set.syntax.mnemonic  = GetReadable (dark ? 0xFF8CBCF0 : 0xFF0000FF, against);
    set.syntax.directive = GetReadable (dark ? 0xFFC586C0 : 0xFFAF00DB, against);
    set.syntax.symbol    = GetReadable (dark ? 0xFF4EC9B0 : 0xFF2B91AF, against);
    set.syntax.number    = GetReadable (dark ? 0xFFB5CEA8 : 0xFF098658, against);
    set.syntax.string    = GetReadable (dark ? 0xFFD69D85 : 0xFFA31515, against);
    set.syntax.address   = GetReadable (text,       against);
    set.syntax.bytes     = GetReadable (muted,      against);
    set.operandAddress   = GetReadable (dark ? 0xFFDCDCAA : 0xFF795E26, against);
    set.annotation       = GetReadable (dark ? 0xFF57A64A : 0xFF008000, against);
    set.result           = GetReadable (result, against);
    set.muted            = set.syntax.bytes;
    set.syntax.comment   = set.annotation;

    //  A changed value is the theme's own red, or, for a theme with none,
    //  the dark page's coral or the light page's red. It need only read on
    //  the page, or on a light page's darkest row shade: a row fill lifts it
    //  where it sits on one, by GetChangedOn.
    set.changed = GetReadable (changedText != 0 ? changedText : (dark ? 0xFFFF6B68 : 0xFFD00000), dark ? background : GetRowShade (background));

    //  A memory window's ROM and I/O bytes take the theme's own colors, or,
    //  for a theme with none, a blue and a gray for the page's darkness, held
    //  to the same ratio as the changed red.
    set.rom = GetReadable (romText != 0 ? romText : (dark ? 0xFF7FB2E5 : 0xFF1F5FA8), dark ? background : GetRowShade (background));
    set.io  = GetReadable (ioText  != 0 ? ioText  : (dark ? 0xFF909090 : 0xFF6A6A6A), dark ? background : GetRowShade (background));

    //  The memory map's mid-tones, which a theme can replace through MakeFor.
    set.mapMain    = MemoryMapBar::GetSourceColor (MemorySource::Main);
    set.mapAux     = MemoryMapBar::GetSourceColor (MemorySource::Aux);
    set.mapLcBank1 = MemoryMapBar::GetSourceColor (MemorySource::LcBank1);
    set.mapLcBank2 = MemoryMapBar::GetSourceColor (MemorySource::LcBank2);
    set.mapRom     = MemoryMapBar::GetSourceColor (MemorySource::Rom);
    set.mapSlotRom = MemoryMapBar::GetSourceColor (MemorySource::SlotRom);
    set.mapIo      = MemoryMapBar::GetSourceColor (MemorySource::Io);

    return set;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::MakeFor
//
////////////////////////////////////////////////////////////////////////////////

DebuggerTextColors::Set DebuggerTextColors::MakeFor (const DxuiTheme & theme)
{
    Set  set = Make (theme.ContentBackground(), theme.Foreground(), theme.ForegroundMuted(), theme.resultText, theme.Accent(),
                     theme.changedText, theme.romText, theme.ioText);



    set.mapMain    = (theme.mapMainRam != 0) ? theme.mapMainRam : set.mapMain;
    set.mapAux     = (theme.mapAuxRam  != 0) ? theme.mapAuxRam  : set.mapAux;
    set.mapLcBank1 = (theme.mapLcBank1 != 0) ? theme.mapLcBank1 : set.mapLcBank1;
    set.mapLcBank2 = (theme.mapLcBank2 != 0) ? theme.mapLcBank2 : set.mapLcBank2;
    set.mapRom     = (theme.mapRom     != 0) ? theme.mapRom     : set.mapRom;
    set.mapSlotRom = (theme.mapSlotRom != 0) ? theme.mapSlotRom : set.mapSlotRom;
    set.mapIo      = (theme.mapIo      != 0) ? theme.mapIo      : set.mapIo;

    return set;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::GetChangedOn
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerTextColors::GetChangedOn (const Set & set, uint32_t fill)
{
    return (fill == 0) ? set.changed : GetReadable (set.changed, fill);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::GetInstructionRanges
//
////////////////////////////////////////////////////////////////////////////////

DebuggerTextColors::Ranges DebuggerTextColors::GetInstructionRanges (const std::wstring & instruction, const Set & set)
{
    Ranges  ranges;



    for (const SourceSyntax::Run & run : SourceSyntax::GetInstructionRuns (instruction))
    {
        uint32_t  argb        = set.syntax.Get (run.token);
        bool      isImmediate = instruction[(size_t) run.start] == L'#' || (run.start > 0 && instruction[(size_t) run.start - 1] == L'#');

        if (run.token == SourceSyntax::Token::Number && !isImmediate)
        {
            argb = set.operandAddress;
        }

        ranges.emplace_back (run.start, run.start + run.length, argb);
    }

    return ranges;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::GetRowShade
//
//  On a light page the selected row and the PC's row are tinted darker than
//  the page, so text must read on the tint as well. The shade stands in for
//  the darkest of those tints.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerTextColors::GetRowShade (uint32_t background)
{
    return DxuiColor::Mix (background | 0xFF000000u, 0xFF000000u, s_kLightRowShade);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::IsDark
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerTextColors::IsDark (uint32_t background)
{
    uint32_t  luma = ((background >> 16) & 0xFF) * 299 + ((background >> 8) & 0xFF) * 587 + (background & 0xFF) * 114;



    return luma < 128 * 1000;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::GetReadable
//
//  Mixes in black or white a twentieth at a time, so a color that already
//  reads is left exactly as it was and one that does not keeps as much of
//  its hue as it can.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerTextColors::GetReadable (uint32_t argb, uint32_t background)
{
    return GetReadable (argb, std::vector<uint32_t> { background }, s_kMinTextContrast);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::GetLowestRatio
//
////////////////////////////////////////////////////////////////////////////////

float DebuggerTextColors::GetLowestRatio (uint32_t argb, const std::vector<uint32_t> & backgrounds)
{
    float  lowest = FLT_MAX;



    for (uint32_t background : backgrounds)
    {
        lowest = (std::min) (lowest, DxuiColor::ComputeContrastRatio (argb, background));
    }

    return lowest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::GetReadable
//
//  The same, against every one of `backgrounds`, which are all dark or all
//  light as the first one is.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerTextColors::GetReadable (uint32_t argb, const std::vector<uint32_t> & backgrounds, float minRatio)
{
    constexpr int  s_kSteps = 20;
    uint32_t       opaque   = argb | 0xFF000000u;
    uint32_t       toward   = IsDark (backgrounds.front()) ? 0xFFFFFFFFu : 0xFF000000u;
    uint32_t       result   = opaque;
    int            step     = 0;



    for (step = 1; step <= s_kSteps && GetLowestRatio (result, backgrounds) < minRatio; step++)
    {
        result = DxuiColor::Mix (opaque, toward, (float) step / (float) s_kSteps);
    }

    return result;
}
