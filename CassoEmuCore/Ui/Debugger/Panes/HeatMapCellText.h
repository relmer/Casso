#pragma once

class HeatMapSymbols;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapCellText
//
//  The text drawn inside a heat map cell large enough for it: its value in hex
//  on one line, and on a second the opcode's form when it was run as one,
//  or else the value in binary; an instruction drawn as one cell across its
//  bytes, split between the rows it is on.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapCellText
{
public:
    //  The lines a cell has room for and the font they are drawn in. A
    //  line's text is in the monospace face, as large as the cell lets it be
    //  up to the theme's size, and is drawn only where that is at least
    //  kMinDetailFontDip; the second line only with room for kDetailChars. A
    //  character is taken as kAdvancePerEm of the size across and a line
    //  kLinePerEm of it down.
    struct Detail
    {
        int    lines  = 0;
        float  fontPx = 0.0f;
    };

    static constexpr float  kMinDetailFontDip = 9.0f;
    static constexpr int    kDetailPadDip     = 2;
    static constexpr int    kValueChars       = 2;
    static constexpr int    kDetailChars      = 10;
    static constexpr float  kAdvancePerEm     = 0.6f;
    static constexpr float  kLinePerEm        = 1.25f;

    //  An instruction's text and bytes for each piece of a cell that wraps to
    //  the next row, by the number of cells in each piece.
    using InstructionLines = std::vector<std::pair<std::wstring, std::wstring>>;

    static Detail            GetDetail            (long widthPx, long heightPx, float maxFontPx, float minFontPx, float padPx);
    static std::wstring      GetDetailLine        (Byte value, bool isOpcode, const std::string & form);
    static uint32_t          GetDetailTextColor   (uint32_t cell, uint32_t foreground, uint32_t background);
    static int               GetInstructionLength (const std::string & form);
    static InstructionLines  SplitInstruction     (const std::wstring & instruction, const std::wstring & bytes, const std::vector<long> & pieceCells);

    //  An instruction from its form and operand bytes at a CPU address, with
    //  each address that has a symbol written as the symbol.
    static std::wstring      GetInstructionText   (const std::string & form, Word cpu, const std::vector<Byte> & operands, const HeatMapSymbols * symbols);
};
