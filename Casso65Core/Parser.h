#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ParsedLine
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
    std::string                          directive;    // ".org", ".byte", ".word", ".text"
    std::string                          directiveArg; // raw argument string
    bool                                 isConstant;   // true for "NAME = EXPR", "NAME equ EXPR", "NAME set EXPR"
    std::string                          constantName;
    std::string                          constantExpr; // raw expression for evaluation
    SymbolKind                           constantKind; // Equ or Set
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
};





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifiedOperand — parser output: syntax form + inner expression
//
////////////////////////////////////////////////////////////////////////////////

struct ClassifiedOperand
{
    OperandSyntax syntax;       // Syntactic form detected by parser
    std::string   expression;   // Inner expression string for evaluation
};



class OpcodeTable;


////////////////////////////////////////////////////////////////////////////////
//
//  Parser
//
////////////////////////////////////////////////////////////////////////////////

class Parser
{
public:
    static std::vector<std::string> SplitLines (const std::string & source);
    static ParsedLine               ParseLine  (const std::string & line, int lineNumber);

    static ClassifiedOperand          ClassifyOperand   (const std::string & operand);
    static bool                       ParseValue        (const std::string & text, int & value);
    static bool                       ValidateLabel     (const std::string & label, const OpcodeTable & opcodeTable, std::string & errorMessage);
    static std::string                ParseQuotedString (const std::string & text);

    // Split a comma-separated argument list respecting () [] '' nesting
    static std::vector<std::string>   SplitArgList      (const std::string & text);
};
