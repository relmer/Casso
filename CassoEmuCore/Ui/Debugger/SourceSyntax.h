#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax
//
//  The runs of an assembly line to color: mnemonics, directives, symbols,
//  numbers, strings and comments, for the source pane and the disassembly.
//
//  A READING PER ASSEMBLER. Merlin's line is read by the assembler's own
//  Merlin profile, so its comment field needs no semicolon, its first column
//  is always a label, and its directives and string delimiters are the ones
//  the assembler accepts. as65 and ca65 share a reading: a comment starts at
//  a semicolon, a label is in the first column or ends in a colon, and an
//  opcode that is not a 65C02 mnemonic is a directive. ca65 adds its @ local
//  labels and its unnamed :+ and :- references.
//
//  Any is the one reading for all three, for a file whose assembler is not
//  known: a comment also starts at a star in the first column (not a star
//  followed by an equals sign, which sets the origin), and a local symbol may
//  start with ], @ or a colon.
//
//  Registers after a comma are left uncolored, as are operators.
//
////////////////////////////////////////////////////////////////////////////////

class SourceSyntax
{
public:
    enum class Token { Mnemonic, Directive, Symbol, Number, String, Comment };

    //  Whose grammar a file is read with.
    enum class Assembler { Any, As65, Merlin, Ca65 };

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
    static std::vector<Run>  GetSourceRuns      (const std::wstring & line, Assembler assembler = Assembler::Any);

    //  A disassembled instruction: its mnemonic, then its operand.
    static std::vector<Run>  GetInstructionRuns (const std::wstring & instruction);

    static bool  IsMnemonic (const std::wstring & word);

    //  The assembler a file was written for, from its directives and labels,
    //  then its extension; as65 when nothing tells.
    static Assembler  DetectAssembler (const std::vector<std::wstring> & lines, const std::wstring & fileName);

    //  What the user picks an assembler by.
    static const wchar_t *  GetAssemblerLabel (Assembler assembler);

private:
    static std::vector<Run>  GetFieldRuns   (const std::wstring & line, Assembler assembler);
    static std::vector<Run>  GetMerlinRuns  (const std::wstring & line);
    static void  AddOperandRuns (const std::wstring & line, size_t from, size_t to, Assembler assembler, std::vector<Run> & runs);
    static bool  IsSymbolStart  (wchar_t ch, Assembler assembler);
    static bool  IsSymbolChar   (wchar_t ch);
    static bool  IsMerlinOnlyOpcode (const std::wstring & word);
    static bool  IsCa65OnlyOpcode   (const std::wstring & word);
    static std::string  ToNarrow (const std::wstring & text);
};
