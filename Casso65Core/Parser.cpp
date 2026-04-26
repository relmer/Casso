#include "Pch.h"

#include "Parser.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SplitLines
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Parser::SplitLines (const std::string & source)
{
    std::vector<std::string> lines;
    std::string              line;
    std::istringstream       stream (source);

    while (std::getline (stream, line))
    {
        // Strip trailing \r if present (handles \r\n line endings)
        if (!line.empty () && line.back () == '\r')
        {
            line.pop_back ();
        }

        lines.push_back (line);
    }

    if (lines.empty ())
    {
        lines.push_back ("");
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripComments
//
////////////////////////////////////////////////////////////////////////////////

static std::string StripComments (const std::string & line)
{
    size_t pos = line.find (';');

    if (pos == std::string::npos)
    {
        return line;
    }

    return line.substr (0, pos);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Trim
//
////////////////////////////////////////////////////////////////////////////////

static std::string Trim (const std::string & s)
{
    size_t start = s.find_first_not_of (" \t");

    if (start == std::string::npos)
    {
        return "";
    }

    size_t end = s.find_last_not_of (" \t");
    return s.substr (start, end - start + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUpper
//
////////////////////////////////////////////////////////////////////////////////

static std::string ToUpper (const std::string & s)
{
    std::string result = s;

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseLine
//
////////////////////////////////////////////////////////////////////////////////

ParsedLine Parser::ParseLine (const std::string & line, int lineNumber)
{
    ParsedLine result  = {};
    result.lineNumber  = lineNumber;
    result.isEmpty     = true;
    result.isDirective = false;
    result.isConstant  = false;

    // Strip comments first
    std::string stripped = StripComments (line);
    std::string trimmed  = Trim (stripped);

    if (trimmed.empty ())
    {
        return result;
    }

    result.isEmpty = false;
    std::string remainder = trimmed;

    // Check for colon-less label: line starts at column 0 with an identifier
    bool startsAtColumn0 = !stripped.empty () && !isspace ((unsigned char) stripped[0]);

    // Check for label (contains ':')
    size_t colonPos = remainder.find (':');

    if (colonPos != std::string::npos)
    {
        result.label = Trim (remainder.substr (0, colonPos));
        remainder    = Trim (remainder.substr (colonPos + 1));

        if (remainder.empty ())
        {
            return result;
        }
    }

    // Check for directive (starts with '.')
    if (!remainder.empty () && remainder[0] == '.')
    {
        result.isDirective = true;

        size_t spacePos = remainder.find_first_of (" \t");

        if (spacePos == std::string::npos)
        {
            result.directive = ToUpper (remainder);
        }
        else
        {
            result.directive    = ToUpper (remainder.substr (0, spacePos));
            result.directiveArg = Trim (remainder.substr (spacePos + 1));
        }

        return result;
    }

    // Check for constant definition: NAME = EXPR, NAME equ EXPR, NAME set EXPR
    // Extract first word and check what follows
    size_t spacePos = remainder.find_first_of (" \t");
    size_t eqPos    = remainder.find ('=');

    // NAME = EXPR (= can appear right after name or with spaces)
    if (eqPos != std::string::npos)
    {
        std::string beforeEq = Trim (remainder.substr (0, eqPos));
        std::string afterEq  = Trim (remainder.substr (eqPos + 1));

        // Ensure beforeEq is a valid identifier (not a mnemonic)
        if (!beforeEq.empty () && (isalpha ((unsigned char) beforeEq[0]) || beforeEq[0] == '_'))
        {
            bool validId = true;

            for (char c : beforeEq)
            {
                if (!isalnum ((unsigned char) c) && c != '_')
                {
                    validId = false;
                    break;
                }
            }

            if (validId && !afterEq.empty ())
            {
                result.isConstant   = true;
                result.constantName = beforeEq;
                result.constantExpr = afterEq;
                result.constantKind = SymbolKind::Set;
                return result;
            }
        }
    }

    // NAME equ EXPR / NAME set EXPR
    if (spacePos != std::string::npos)
    {
        std::string firstWord  = remainder.substr (0, spacePos);
        std::string afterFirst = Trim (remainder.substr (spacePos + 1));

        size_t sp2 = afterFirst.find_first_of (" \t");
        std::string secondWord = (sp2 == std::string::npos) ? afterFirst : afterFirst.substr (0, sp2);
        std::string secondUpper = ToUpper (secondWord);

        if (secondUpper == "EQU" || secondUpper == "SET")
        {
            std::string expr = (sp2 == std::string::npos) ? "" : Trim (afterFirst.substr (sp2 + 1));

            if (!firstWord.empty () && (isalpha ((unsigned char) firstWord[0]) || firstWord[0] == '_'))
            {
                result.isConstant   = true;
                result.constantName = firstWord;
                result.constantExpr = expr;
                result.constantKind = (secondUpper == "EQU") ? SymbolKind::Equ : SymbolKind::Set;
                return result;
            }
        }
    }

    // Extract mnemonic (first word)
    std::string firstWordUpper;

    if (spacePos == std::string::npos)
    {
        firstWordUpper = ToUpper (remainder);
    }
    else
    {
        firstWordUpper = ToUpper (remainder.substr (0, spacePos));
    }

    // Check for AS65 directive synonyms (without leading dot)
    std::string canonicalDirective;

    if      (firstWordUpper == "ORG")                                                                    { canonicalDirective = ".ORG";   }
    else if (firstWordUpper == "DB"  || firstWordUpper == "BYT" || firstWordUpper == "BYTE" ||
             firstWordUpper == "FCB" || firstWordUpper == "FCC")                                         { canonicalDirective = ".BYTE";  }
    else if (firstWordUpper == "DW"  || firstWordUpper == "WORD" ||
             firstWordUpper == "FCW" || firstWordUpper == "FDB")                                         { canonicalDirective = ".WORD";  }
    else if (firstWordUpper == "DD")                                                                     { canonicalDirective = ".DD";    }
    else if (firstWordUpper == "END")                                                                    { canonicalDirective = ".END";   }
    else if (firstWordUpper == "DS"  || firstWordUpper == "DSB" || firstWordUpper == "RMB")              { canonicalDirective = ".DS";    }
    else if (firstWordUpper == "ALIGN")                                                                  { canonicalDirective = ".ALIGN"; }
    else if (firstWordUpper == "ERROR")                                                                  { canonicalDirective = ".ERROR"; }

    // Segment keywords recognized as no-ops
    else if (firstWordUpper == "CODE" || firstWordUpper == "DATA" || firstWordUpper == "BSS")            { canonicalDirective = ".SEGMENT_NOOP"; }
    else if (firstWordUpper == "NOOPT" || firstWordUpper == "OPT")                                      { canonicalDirective = ".SEGMENT_NOOP"; }

    if (!canonicalDirective.empty ())
    {
        result.isDirective = true;
        result.directive   = canonicalDirective;

        if (spacePos != std::string::npos)
        {
            result.directiveArg = Trim (remainder.substr (spacePos + 1));
        }

        return result;
    }

    // Extract mnemonic (first word)
    if (spacePos == std::string::npos)
    {
        result.mnemonic = firstWordUpper;
        result.startsAtColumn0 = startsAtColumn0;
        return result;
    }

    result.mnemonic = firstWordUpper;
    result.operand  = Trim (remainder.substr (spacePos + 1));
    result.startsAtColumn0 = startsAtColumn0;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrimOperand
//
////////////////////////////////////////////////////////////////////////////////

static std::string TrimOperand (const std::string & s)
{
    size_t start = s.find_first_not_of (" \t");

    if (start == std::string::npos)
    {
        return "";
    }

    size_t end = s.find_last_not_of (" \t");
    return s.substr (start, end - start + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUpperStr
//
////////////////////////////////////////////////////////////////////////////////

static std::string ToUpperStr (const std::string & s)
{
    std::string result = s;

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMatchingClose — find matching ')' or ']' respecting nesting
//
////////////////////////////////////////////////////////////////////////////////

static size_t FindMatchingClose (const std::string & s, size_t openPos)
{
    char openChar  = s[openPos];
    char closeChar = (openChar == '(') ? ')' : ']';
    int  depth     = 1;



    for (size_t i = openPos + 1; i < s.size (); i++)
    {
        char c = s[i];

        if (c == '\'' && i + 2 < s.size () && s[i + 2] == '\'')
        {
            i += 2;  // skip char literal
        }
        else if (c == openChar)
        {
            depth++;
        }
        else if (c == closeChar)
        {
            depth--;

            if (depth == 0)
            {
                return i;
            }
        }
    }

    return std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindTopLevelComma — find ',' not inside () [] or ''
//
////////////////////////////////////////////////////////////////////////////////

static size_t FindTopLevelComma (const std::string & s, size_t start = 0)
{
    int depth = 0;



    for (size_t i = start; i < s.size (); i++)
    {
        char c = s[i];

        if (c == '\'' && i + 2 < s.size () && s[i + 2] == '\'')
        {
            i += 2;  // skip char literal
        }
        else if (c == '(' || c == '[')
        {
            depth++;
        }
        else if (c == ')' || c == ']')
        {
            depth--;
        }
        else if (c == ',' && depth == 0)
        {
            return i;
        }
    }

    return std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyOperand — syntax detection only, no value parsing
//
////////////////////////////////////////////////////////////////////////////////

ClassifiedOperand Parser::ClassifyOperand (const std::string & operand)
{
    ClassifiedOperand result = {};
    result.syntax = OperandSyntax::None;

    std::string op = TrimOperand (operand);

    if (op.empty ())
    {
        return result;
    }

    // Immediate: #expr
    if (op[0] == '#')
    {
        result.syntax     = OperandSyntax::Immediate;
        result.expression = TrimOperand (op.substr (1));
        return result;
    }

    // Accumulator: "A" (exact match, case-insensitive)
    if (ToUpperStr (op) == "A")
    {
        result.syntax = OperandSyntax::Accumulator;
        return result;
    }

    // Indirect modes: starts with '('
    if (op[0] == '(')
    {
        size_t closePos = FindMatchingClose (op, 0);

        if (closePos == std::string::npos)
        {
            // Unmatched paren — treat as bare expression
            result.syntax     = OperandSyntax::Bare;
            result.expression = op;
            return result;
        }

        std::string inner = TrimOperand (op.substr (1, closePos - 1));
        std::string after = TrimOperand (op.substr (closePos + 1));

        // Check for (expr,X) — IndirectX
        size_t commaPos = FindTopLevelComma (inner);

        if (commaPos != std::string::npos)
        {
            std::string beforeComma = TrimOperand (inner.substr (0, commaPos));
            std::string afterComma  = ToUpperStr (TrimOperand (inner.substr (commaPos + 1)));

            if (afterComma == "X")
            {
                result.syntax     = OperandSyntax::IndirectX;
                result.expression = beforeComma;
                return result;
            }
        }

        // Check for (expr),Y — IndirectY
        if (!after.empty () && after[0] == ',')
        {
            std::string reg = ToUpperStr (TrimOperand (after.substr (1)));

            if (reg == "Y")
            {
                result.syntax     = OperandSyntax::IndirectY;
                result.expression = inner;
                return result;
            }
        }

        // Plain (expr) — Indirect (for JMP)
        result.syntax     = OperandSyntax::Indirect;
        result.expression = inner;
        return result;
    }

    // Check for top-level ,X or ,Y suffix
    size_t commaPos = FindTopLevelComma (op);

    if (commaPos != std::string::npos)
    {
        std::string exprPart = TrimOperand (op.substr (0, commaPos));
        std::string reg      = ToUpperStr (TrimOperand (op.substr (commaPos + 1)));

        if (reg == "X")
        {
            result.syntax     = OperandSyntax::IndexedX;
            result.expression = exprPart;
            return result;
        }

        if (reg == "Y")
        {
            result.syntax     = OperandSyntax::IndexedY;
            result.expression = exprPart;
            return result;
        }

        // Comma but not ,X or ,Y — treat as bare (shouldn't happen for 6502)
        result.syntax     = OperandSyntax::Bare;
        result.expression = op;
        return result;
    }

    // Bare expression (no prefix, no suffix)
    result.syntax     = OperandSyntax::Bare;
    result.expression = op;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseValue
//
////////////////////////////////////////////////////////////////////////////////

bool Parser::ParseValue (const std::string & text, int & value)
{
    if (text.empty ())
    {
        return false;
    }

    // Hex: $FF
    if (text[0] == '$')
    {
        std::string hex = text.substr (1);

        if (hex.empty ())
        {
            return false;
        }

        char * endPtr = nullptr;
        long   parsed = strtol (hex.c_str (), &endPtr, 16);

        if (*endPtr != '\0')
        {
            return false;
        }

        value = (int) parsed;
        return true;
    }

    // Binary: %10101010
    if (text[0] == '%')
    {
        std::string bin = text.substr (1);

        if (bin.empty ())
        {
            return false;
        }

        char * endPtr = nullptr;
        long   parsed = strtol (bin.c_str (), &endPtr, 2);

        if (*endPtr != '\0')
        {
            return false;
        }

        value = (int) parsed;
        return true;
    }

    // Decimal
    char * endPtr = nullptr;
    long   parsed = strtol (text.c_str (), &endPtr, 10);

    if (*endPtr != '\0')
    {
        return false;
    }

    value = (int) parsed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUpperValidate
//
////////////////////////////////////////////////////////////////////////////////

static std::string ToUpperValidate (const std::string & s)
{
    std::string result = s;

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValidateLabel
//
////////////////////////////////////////////////////////////////////////////////

bool Parser::ValidateLabel (const std::string & label, const OpcodeTable & opcodeTable, std::string & errorMessage)
{
    if (label.empty ())
    {
        errorMessage = "Empty label name";
        return false;
    }

    // Must start with letter or underscore
    char first = label[0];

    if (!isalpha ((unsigned char) first) && first != '_')
    {
        errorMessage = "Label must start with a letter or underscore: " + label;
        return false;
    }

    // Must contain only alphanumeric + underscore
    for (char c : label)
    {
        if (!isalnum ((unsigned char) c) && c != '_')
        {
            errorMessage = "Label contains invalid character: " + label;
            return false;
        }
    }

    // Must not be a register name (case-insensitive)
    std::string upper = ToUpperValidate (label);

    if (upper == "A" || upper == "X" || upper == "Y" || upper == "S")
    {
        errorMessage = "Label name conflicts with register name: " + label;
        return false;
    }

    // Must not be an exact mnemonic (e.g., "LDA" is rejected, but "lda" is only a warning)
    if (opcodeTable.IsMnemonic (label))
    {
        errorMessage = "Label name conflicts with mnemonic: " + label;
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SplitArgList — split comma-separated list respecting () [] '' nesting
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Parser::SplitArgList (const std::string & text)
{
    std::vector<std::string> args;
    size_t                   start = 0;



    while (start <= text.size ())
    {
        size_t commaPos = FindTopLevelComma (text, start);

        if (commaPos == std::string::npos)
        {
            std::string arg = TrimOperand (text.substr (start));

            if (!arg.empty ())
            {
                args.push_back (arg);
            }

            break;
        }

        std::string arg = TrimOperand (text.substr (start, commaPos - start));

        if (!arg.empty ())
        {
            args.push_back (arg);
        }

        start = commaPos + 1;
    }

    return args;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseQuotedString
//
////////////////////////////////////////////////////////////////////////////////

std::string Parser::ParseQuotedString (const std::string & text)
{
    std::string trimmed = TrimOperand (text);

    if (trimmed.size () < 2 || trimmed.front () != '"' || trimmed.back () != '"')
    {
        return "";
    }

    return trimmed.substr (1, trimmed.size () - 2);
}
