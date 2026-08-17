#pragma once

#include "Directive.h"

#include "AssemblerTypes.h"
#include "StringEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ParsedLine
//
//  One source line after lexical analysis, before anything has been resolved.
//
//  Everything here is SYNTACTIC. No expression has been evaluated, no symbol
//  looked up, no address assigned -- the operand and the constant expression
//  are raw strings, deliberately. That is what lets the same parse serve both
//  assembler passes: pass 1 sizes from this, pass 2 emits from it, and neither
//  re-parses text.
//
//  Directives carry two spellings. directiveToken is what code switches on;
//  the canonical dotted string is kept alongside it purely for diagnostics, so
//  an error can quote the directive back the way the source wrote it.
//
//  startsAtColumn0 is a lexical fact that only matters much later. A word in
//  column 0 is a label only once everything else has been ruled out, and by
//  then the leading whitespace is long gone -- so it is recorded here, where
//  it is still knowable.
//
//  stringMode is the same kind of fact one level up: WHICH spelling of a string
//  directive was written decides how its text becomes bytes, and only the
//  dialect knows its own spellings. Recording it here keeps the engine free of
//  a per-dialect table of string names -- it reads the mode and encodes.
//
////////////////////////////////////////////////////////////////////////////////

struct ParsedLine
{
    std::string                          label;
    std::string                          mnemonic;
    std::string                          operand;
    int                                  lineNumber;
    bool                                 isEmpty;
    bool                                 isDirective;
    std::string                          directive;    // canonical dotted spelling, for diagnostics
    Directive                            directiveToken = Directive::None;  // what to switch on
    std::string                          directiveArg; // raw argument string
    bool                                 isConstant;   // true for "NAME = EXPR", "NAME equ EXPR", "NAME set EXPR"
    std::string                          constantName;
    std::string                          constantExpr; // raw expression for evaluation
    SymbolKind                           constantKind; // Equ or Set
    bool                                 startsAtColumn0; // true if line had no leading whitespace
    StringEncodingMode                   stringMode = StringEncodingMode::Plain;  // meaningful only for Directive::StringData
};





////////////////////////////////////////////////////////////////////////////////
//
//  OperandSyntax — what the parser detected (syntax only, no encoding decisions)
//
////////////////////////////////////////////////////////////////////////////////

enum class OperandSyntax
{
    None,          // No operand (implied: NOP, RTS, etc.)
    Immediate,     // #expr
    Bare,          // expr (could be ZeroPage, Absolute, Relative, or JumpAbsolute)
    IndexedX,      // expr,X
    IndexedY,      // expr,Y
    IndirectX,     // (expr,X)
    IndirectY,     // (expr),Y
    Indirect,      // (expr)  — used for JMP ($addr)
    Accumulator,   // A
    ZeroPageRelative, // expr,expr — 65C02 BBRn/BBSn bit-branch (zp,target)

    Count,         // sentinel: sizes syntax-indexed tables
};





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifiedOperand — parser output: syntax form + inner expression
//
////////////////////////////////////////////////////////////////////////////////

struct ClassifiedOperand
{
    OperandSyntax syntax;           // Syntactic form detected by parser
    std::string   expression;       // Inner expression string for evaluation
    std::string   secondExpression; // Second operand (ZeroPageRelative branch target)
};



class OpcodeTable;





////////////////////////////////////////////////////////////////////////////////
//
//  Parser
//
////////////////////////////////////////////////////////////////////////////////

class DialectProfile;



class Parser
{
public:
    static std::vector<std::string> SplitLines (const std::string & source);

    // Delegates to the active dialect. The overload without a profile keeps
    // AS65 as the default, so every caller that predates dialect selection is
    // unaffected.
    static ParsedLine               ParseLine  (const std::string & line, int lineNumber);
    static ParsedLine               ParseLine  (const std::string & line, int lineNumber, const DialectProfile & dialect);

    // Lexical helpers shared with the dialect profiles, which need exactly the
    // same primitives to segment a line.
    static std::string              StripComments (const std::string & line);
    static std::string              Trim          (const std::string & s);
    static std::string              ToUpper       (const std::string & s);

    static ClassifiedOperand          ClassifyOperand   (const std::string & operand);
    static HRESULT                    ParseValue        (const std::string & text, int & value);
    static HRESULT                    ValidateLabel     (const std::string & label, const OpcodeTable & opcodeTable, std::string & errorMessage);
    static std::string                ParseQuotedString (const std::string & text);

    // Split a comma-separated argument list respecting () [] '' nesting
    static std::vector<std::string>   SplitArgList      (const std::string & text);

    // The same, on a separator the caller names -- macro arguments, whose
    // separator is the dialect's rather than the assembler's.
    static std::vector<std::string>   SplitOnSeparator  (const std::string & text, char separator);
};
