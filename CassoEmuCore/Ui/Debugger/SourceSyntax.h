#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax
//
//  The runs of an assembly line to color: mnemonics, directives, symbols,
//  numbers, strings and comments, for the source pane and the disassembly.
//
//  ONE READING FOR as65, MERLIN AND ca65. The three agree on a line's fields
//  -- a label, an opcode, an operand, a comment -- and differ in details this
//  can take in together: a comment starts at a semicolon anywhere, or at a
//  star in the first column (not a star followed by an equals sign, which
//  sets the origin); a label is in the first column or ends in a colon; an
//  opcode starting with a period is a directive, as is any opcode that is not
//  a 65C02 mnemonic; a local symbol may start with ], @ or a colon.
//
//  Registers after a comma are left uncolored, as are operators.
//
////////////////////////////////////////////////////////////////////////////////

class SourceSyntax
{
public:
    enum class Token { Mnemonic, Directive, Symbol, Number, String, Comment };

    struct Run
    {
        int    start  = 0;
        int    length = 0;
        Token  token  = Token::Symbol;
    };

    //  A color for each token, from the window's theme.
    struct Colors
    {
        uint32_t  mnemonic  = 0;
        uint32_t  directive = 0;
        uint32_t  symbol    = 0;
        uint32_t  number    = 0;
        uint32_t  string    = 0;
        uint32_t  comment   = 0;

        uint32_t  Get (Token token) const;
        bool operator== (const Colors & other) const = default;
    };

    //  A line of source, in order and not overlapping.
    static std::vector<Run>  GetSourceRuns      (const std::wstring & line);

    //  A disassembled instruction: its mnemonic, then its operand.
    static std::vector<Run>  GetInstructionRuns (const std::wstring & instruction);

    static bool  IsMnemonic (const std::wstring & word);

private:
    static void  AddOperandRuns (const std::wstring & line, size_t from, std::vector<Run> & runs);
    static bool  IsSymbolStart  (wchar_t ch);
    static bool  IsSymbolChar   (wchar_t ch);
};
