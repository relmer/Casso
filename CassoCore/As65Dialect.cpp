#include "Pch.h"

#include "As65Dialect.h"

#include "Directive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  As65Dialect::ParseLine
//
//  One source line into its parts. The ORDER of the tests is the grammar --
//  each rules out a shape so the next can assume it is gone:
//
//    comments first, so a ';' inside nothing later can be mistaken for code
//    blank             nothing else to decide
//    label             `name:` splits off, and a label-only line stops here
//    .directive        a leading dot is unambiguous, so it settles next
//    NAME = / equ / set   constant definition
//    everything else   mnemonic + operand
//
//  Reordering these breaks things quietly. Strip comments after splitting on
//  ':' and a commented-out label steals the line; test for a constant before
//  the dot and `.if X = 1` parses as a definition of `.if X`.
//
//  startsAtColumn0 is captured from the STRIPPED line before trimming, since
//  it is the one fact trimming destroys -- and it is what later lets a bare
//  word in column 0 be recognized as a colon-less label.
//
//  Unknown dotted spellings resolve to Directive::None and keep their text,
//  so pass 1 reports them rather than the parser guessing.
//
////////////////////////////////////////////////////////////////////////////////

ParsedLine As65Dialect::ParseLine (const std::string & line, int lineNumber) const
{
    HRESULT      hr                 = S_OK;
    ParsedLine   result             = {};
    std::string  stripped           = Parser::StripComments (line);   // comments go first
    std::string  trimmed            = Parser::Trim (stripped);
    std::string  remainder;
    std::string  firstWordUpper;
    std::string  canonicalDirective;
    Directive    directiveToken     = Directive::None;
    size_t       colonPos           = std::string::npos;
    size_t       spacePos           = std::string::npos;
    size_t       eqPos              = std::string::npos;
    bool         startsAtColumn0    = false;
    bool         isBlank            = trimmed.empty();
    bool         labelOnly          = false;
    bool         isDotDirective     = false;
    bool         isConstant         = false;
    bool         isBareDirective    = false;



    result.lineNumber  = lineNumber;
    result.isEmpty     = isBlank;
    result.isDirective = false;
    result.isConstant  = false;

    BAIL_OUT_IF (isBlank, S_OK);

    remainder = trimmed;

    // Check for colon-less label: line starts at column 0 with an identifier
    startsAtColumn0 = !stripped.empty() && !isspace ((unsigned char) stripped[0]);

    // Check for label (contains ':')
    colonPos = remainder.find (':');

    if (colonPos != std::string::npos)
    {
        result.label = Parser::Trim (remainder.substr (0, colonPos));
        remainder    = Parser::Trim (remainder.substr (colonPos + 1));
        labelOnly    = remainder.empty();
    }

    BAIL_OUT_IF (labelOnly, S_OK);

    // Check for directive (starts with '.')
    isDotDirective = !remainder.empty() && remainder[0] == '.';

    if (isDotDirective)
    {
        result.isDirective = true;
        spacePos           = remainder.find_first_of (" \t");

        if (spacePos == std::string::npos)
        {
            result.directive = Parser::ToUpper (remainder);
        }
        else
        {
            result.directive    = Parser::ToUpper (remainder.substr (0, spacePos));
            result.directiveArg = Parser::Trim (remainder.substr (spacePos + 1));
        }

        // Unknown dotted spellings resolve to None and stay a string, which is
        // what lets the pass-1 dispatch report the spelling by name.
        result.directiveToken = DirectiveTable::FromSpelling (result.directive);
    }

    BAIL_OUT_IF (isDotDirective, S_OK);

    // Check for constant definition: NAME = EXPR, NAME equ EXPR, NAME set EXPR
    // Extract first word and check what follows
    spacePos = remainder.find_first_of (" \t");
    eqPos    = remainder.find ('=');

    // NAME = EXPR (= can appear right after name or with spaces)
    if (eqPos != std::string::npos)
    {
        std::string beforeEq = Parser::Trim (remainder.substr (0, eqPos));
        std::string afterEq  = Parser::Trim (remainder.substr (eqPos + 1));

        // Ensure beforeEq is a valid identifier (not a mnemonic)
        if (!beforeEq.empty() && (isalpha ((unsigned char) beforeEq[0]) || beforeEq[0] == '_'))
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

            if (validId && !afterEq.empty())
            {
                result.isConstant   = true;
                result.constantName = beforeEq;
                result.constantExpr = afterEq;
                result.constantKind = SymbolKind::Set;
                isConstant          = true;
            }
        }
    }

    // NAME equ EXPR / NAME set EXPR
    if (!isConstant && spacePos != std::string::npos)
    {
        std::string  firstWord   = remainder.substr (0, spacePos);
        std::string  afterFirst  = Parser::Trim (remainder.substr (spacePos + 1));
        std::string  secondWord;
        std::string  secondUpper;

        size_t sp2 = afterFirst.find_first_of (" \t");
        secondWord = (sp2 == std::string::npos) ? afterFirst : afterFirst.substr (0, sp2);
        secondUpper = Parser::ToUpper (secondWord);

        if (secondUpper == "EQU" || secondUpper == "SET")
        {
            std::string expr = (sp2 == std::string::npos) ? "" : Parser::Trim (afterFirst.substr (sp2 + 1));

            if (!firstWord.empty() && (isalpha ((unsigned char) firstWord[0]) || firstWord[0] == '_'))
            {
                result.isConstant   = true;
                result.constantName = firstWord;
                result.constantExpr = expr;
                result.constantKind = (secondUpper == "EQU") ? SymbolKind::Equ : SymbolKind::Set;
                isConstant          = true;
            }
        }
    }

    BAIL_OUT_IF (isConstant, S_OK);

    // Extract mnemonic (first word)
    firstWordUpper = (spacePos == std::string::npos)
                         ? Parser::ToUpper (remainder)
                         : Parser::ToUpper (remainder.substr (0, spacePos));

    // as65 spells its directives without a leading dot (DB / FCB / FCC for
    // .BYTE, and so on). DirectiveTable holds every accepted spelling, so
    // this is one lookup rather than a chain -- and that table is the seam a
    // second assembler dialect plugs into.
    directiveToken = DirectiveTable::FromSpelling (firstWordUpper);

    if (directiveToken != Directive::None)
    {
        canonicalDirective = DirectiveTable::GetCanonicalName (directiveToken);
    }

    // A spelling the table rejected may still be one of the dual-purpose forms
    // that cannot live in it -- today only RMB, where `rmb <count>` reserves
    // storage but `rmb <bit>,<zp>` is the Rockwell instruction. The comma is
    // what separates them, so once it is absent the instruction is ruled out
    // and DirectiveTable can resolve the rest.
    //
    // The dialect's other dual-purpose mnemonic, `nop <count>`, cannot be
    // decided here -- see AssemblySession::HandleMultiNop.
    if (directiveToken == Directive::None)
    {
        Directive ambiguous = DirectiveTable::FromAmbiguousSpelling (firstWordUpper);

        if (ambiguous != Directive::None)
        {
            std::string operandText = (spacePos == std::string::npos) ? "" : remainder.substr (spacePos + 1);

            // The comma is what rules the instruction out.
            if (operandText.find (',') == std::string::npos)
            {
                directiveToken     = ambiguous;
                canonicalDirective = DirectiveTable::GetCanonicalName (ambiguous);
            }
        }
    }

    isBareDirective = !canonicalDirective.empty();

    if (isBareDirective)
    {
        result.isDirective    = true;
        result.directive      = canonicalDirective;
        result.directiveToken = directiveToken;

        if (spacePos != std::string::npos)
        {
            result.directiveArg = Parser::Trim (remainder.substr (spacePos + 1));
        }
    }

    BAIL_OUT_IF (isBareDirective, S_OK);

    // Plain instruction. The operand is whatever follows the mnemonic, if
    // anything -- an implied-mode instruction has none.
    result.mnemonic        = firstWordUpper;
    result.startsAtColumn0 = startsAtColumn0;

    if (spacePos != std::string::npos)
    {
        result.operand = Parser::Trim (remainder.substr (spacePos + 1));
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  As65Dialect::GetMacroSyntax
//
//  as65 closes a body with `endm`, declares the labels to rename with `local`,
//  and takes comma-separated arguments substituted by name or by backslash
//  number. Everything else keeps the default, which is what the default is for.
//
////////////////////////////////////////////////////////////////////////////////

MacroSyntax As65Dialect::GetMacroSyntax() const
{
    MacroSyntax  syntax = {};



    syntax.endKeyword   = "ENDM";
    syntax.localKeyword = "LOCAL";

    return syntax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  As65Dialect::GetDirectiveForSpelling
//
//  Whether as65 claims a word, for the benefit of a diagnostic raised by some
//  OTHER dialect.
//
//  One lookup in the shared table, which holds every accepted spelling -- the
//  dotted forms and the bare ones alike -- so the answer cannot drift from what
//  ParseLine would have resolved. The dual-purpose spellings are deliberately
//  not consulted: whether `rmb` is a directive or the Rockwell instruction turns
//  on the operand, and a word being attributable to a dialect is a question
//  about the word alone.
//
////////////////////////////////////////////////////////////////////////////////

Directive As65Dialect::GetDirectiveForSpelling (const std::string & upperSpelling) const
{
    return DirectiveTable::FromSpelling (upperSpelling);
}
