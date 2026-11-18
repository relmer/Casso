#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapCellText.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCellText::GetDetail
//
//  Two lines where both fit at kMinDetailFontDip or more, else one, else
//  none; the text as large as fits, up to the theme's monospace size.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapCellText::Detail HeatMapCellText::GetDetail (long widthPx, long heightPx, float maxFontPx, float minFontPx, float padPx)
{
    float  innerW = (float) widthPx  - padPx * 2.0f;
    float  innerH = (float) heightPx - padPx * 2.0f;
    float  one    = std::min ({ maxFontPx, innerH / kLinePerEm,        innerW / ((float) kValueChars  * kAdvancePerEm) });
    float  two    = std::min ({ maxFontPx, innerH / (kLinePerEm * 2.0f), innerW / ((float) kDetailChars * kAdvancePerEm) });



    if (two >= minFontPx)
    {
        return Detail { 2, two };
    }

    if (one >= minFontPx)
    {
        return Detail { 1, one };
    }

    return Detail {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCellText::GetDetailLine
//
//  The opcode's form for a byte fetched as an opcode whose instruction is
//  known; otherwise the byte's bits, high first.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapCellText::GetDetailLine (Byte value, bool isOpcode, const std::string & form)
{
    constexpr int  kBits = 8;
    std::wstring   bits;



    if (isOpcode && !form.empty())
    {
        return TextEncoding::NarrowToWide (form);
    }

    for (int bit = kBits - 1; bit >= 0; bit--)
    {
        bits += ((value >> bit) & 1) ? L'1' : L'0';
    }

    return bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCellText::GetDetailTextColor
//
//  The theme's text color or its page color, whichever stands out more from
//  the cell, so a value reads on a dim cell and a bright one alike.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapCellText::GetDetailTextColor (uint32_t cell, uint32_t foreground, uint32_t background)
{
    float  onText = DxuiColor::ComputeContrastRatio (foreground | 0xFF000000u, cell | 0xFF000000u);
    float  onPage = DxuiColor::ComputeContrastRatio (background | 0xFF000000u, cell | 0xFF000000u);



    return ((onText >= onPage) ? foreground : background) | 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCellText::GetInstructionLength
//
//  From the opcode's form, its operand's digits dots: a branch has one byte
//  of offset however its target is written, BBR and BBS a zero-page byte
//  and an offset, and any other operand a byte for each two dots.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapCellText::GetInstructionLength (const std::string & form)
{
    static constexpr const char *  kBranches[] = { "BCC", "BCS", "BEQ", "BMI", "BNE", "BPL", "BVC", "BVS", "BRA" };
    size_t                         space       = form.find (' ');
    std::string                    mnemonic    = form.substr (0, space);



    if (form.empty() || space == std::string::npos)
    {
        return 1;
    }

    if (mnemonic.starts_with ("BBR") || mnemonic.starts_with ("BBS"))
    {
        return 3;
    }

    if (std::ranges::find (kBranches, mnemonic) != std::end (kBranches))
    {
        return 2;
    }

    return 1 + (int) std::ranges::count (form, '.') / 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCellText::SplitInstruction
//
////////////////////////////////////////////////////////////////////////////////

HeatMapCellText::InstructionLines HeatMapCellText::SplitInstruction (const std::wstring & instruction, const std::wstring & bytes, const std::vector<long> & pieceCells)
{
    InstructionLines  lines (pieceCells.size());
    size_t            space = instruction.find (L' ');
    size_t            gap   = bytes.find (L' ');



    if (lines.empty())
    {
        return lines;
    }

    if (lines.size() > 1 && pieceCells.front() == 1 && space != std::wstring::npos && gap != std::wstring::npos)
    {
        lines[0] = { instruction.substr (0, space), bytes.substr (0, gap) };
        lines[1] = { instruction.substr (space + 1), bytes.substr (gap + 1) };
        return lines;
    }

    lines[0] = { instruction, bytes };
    return lines;
}





