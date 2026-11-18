#pragma once

#include "Debugger/Reply.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText
//
//  What a line of source text has at a character: the word there and the
//  symbol it is, an operand's symbol, and the text of a symbol's tip; and
//  whether a line is an instruction, a label alone or a macro's definition.
//
////////////////////////////////////////////////////////////////////////////////

class SourceLineText
{
public:
    //  The symbol a source line has at a caret position, by its name, as
    //  "count_text = $60F2", and after a < or > before it, the byte that
    //  operator takes. False off a word, in a comment, or for a word that
    //  isn't a symbol. The symbol found is in `found`, when one is passed.
    static bool  TryGetSymbolText (const std::wstring & line, int ch, const std::vector<SymbolInfo> & symbols, std::wstring & text,
                                   const SymbolInfo ** found = nullptr);

    //  The lines of a symbol's tip after its address: the bytes there, as
    //  "$60C1: 05", for a symbol that is not a constant, then its description
    //  and the comment on its line in the source.
    static void  AppendSymbolDetails (std::wstring & text, const std::string & name, Word address, bool isConstant,
                                      const DebuggerViewSnapshot & snapshot, const std::string & comment);

    //  The symbol whose name is the word at a character of a line, before a
    //  comment, and where the word starts; null for none.
    static const SymbolInfo *  FindSymbolAt (const std::wstring & line, int ch, const std::vector<SymbolInfo> & symbols, int & start);

    //  The word at a character of a line, before a comment, as FindSymbolAt
    //  takes it; empty for none.
    static std::wstring  GetWordAt (const std::wstring & line, int ch);

    //  Whether a line defines a macro of a name: "name macro" as AS65 and
    //  Merlin's MAC write it, or ".macro name" as ca65 writes it.
    static bool  IsMacroDefinition (const std::wstring & line, const std::wstring & name);

    //  Where a line's code starts: past its label and the spaces after it, or
    //  at its start for a line with no code past a label.
    static int   FindCodeStart (const std::wstring & line);

    //  The first symbol the operand of the instruction starting at `codeAt`
    //  is written with, before a comment, as "count_text = $60F2", and
    //  where its word starts. False, and -1, for an operand with none.
    //  Whether the word at a character is that symbol.
    static bool  TryGetOperandSymbolText (const std::wstring & line, int codeAt, const std::vector<SymbolInfo> & symbols, std::wstring & text);
    static int   FindOperandSymbol       (const std::wstring & line, int codeAt, const std::vector<SymbolInfo> & symbols);
    static bool  IsOperandSymbolAt       (const std::wstring & line, int codeAt, int ch, const std::vector<SymbolInfo> & symbols);

    //  Whether a line's opcode is a 65C02 mnemonic, so that it is one instruction.
    static bool  IsInstructionLine (const std::wstring & line, SourceSyntax::Assembler assembler = SourceSyntax::Assembler::Any);

    //  Whether a line is a label alone, with at most a comment after it.
    static bool  IsLabelLine       (const std::wstring & line, SourceSyntax::Assembler assembler = SourceSyntax::Assembler::Any);
};
