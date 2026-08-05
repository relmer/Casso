#include "Pch.h"

#include "Parser.h"
#include "Directive.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SplitLines
//
//  Source text into lines, then continuations joined -- two passes, because
//  finding a trailing backslash requires the lines to exist first.
//
//  All three line endings are accepted: LF, CRLF, and bare CR. The last
//  matters more than it looks -- period Apple II sources really are CR-only,
//  so treating CR as mere whitespace would read a whole file as one line.
//
//  A trailing backslash continues onto the next line, EXCEPT when itself
//  escaped: `\\` at end of line is a literal backslash, not a continuation.
//  Without that test a string ending in a path separator would silently
//  swallow the line after it.
//
//  A final line with no terminator is still pushed, so a file not ending in a
//  newline does not lose its last instruction.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Parser::SplitLines (const std::string & source)
{
    std::vector<std::string> lines;
    std::string              line;



    for (size_t i = 0; i < source.size(); i++)
    {
        char c = source[i];

        if (c == '\n')
        {
            // LF or the LF of a CRLF — end of line
            lines.push_back (line);
            line.clear();
        }
        else if (c == '\r')
        {
            // CR — end of line (peek ahead for CRLF)
            lines.push_back (line);
            line.clear();

            if (i + 1 < source.size() && source[i + 1] == '\n')
            {
                i++;  // consume the LF of CRLF
            }
        }
        else
        {
            line += c;
        }
    }

    // Push remaining content (last line without trailing newline)
    lines.push_back (line);

    if (lines.empty())
    {
        lines.push_back ("");
    }

    // Join continuation lines (trailing backslash before EOL)
    std::vector<std::string> joined;
    std::string              current;

    for (const auto & raw : lines)
    {
        size_t last = raw.find_last_not_of (" \t\r");

        if (last != std::string::npos && raw[last] == '\\')
        {
            // Check for escaped backslash (\\) — not a continuation
            if (last > 0 && raw[last - 1] == '\\')
            {
                current += raw;
                joined.push_back (current);
                current.clear();
            }
            else
            {
                current += raw.substr (0, last);
            }
        }
        else
        {
            current += raw;
            joined.push_back (current);
            current.clear();
        }
    }

    if (!current.empty())
    {
        joined.push_back (current);
    }

    return joined;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripComments
//
////////////////////////////////////////////////////////////////////////////////

static std::string StripComments (const std::string & line)
{
    size_t  pos = line.find (';');


    return (pos == std::string::npos) ? line : line.substr (0, pos);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Trim
//
////////////////////////////////////////////////////////////////////////////////

static std::string Trim (const std::string & s)
{
    std::string  out;
    size_t       start = s.find_first_not_of (" \t");
    size_t       end   = 0;



    // All-whitespace (or empty) leaves `out` empty.
    if (start != std::string::npos)
    {
        end = s.find_last_not_of (" \t");
        out = s.substr (start, end - start + 1);
    }

    return out;
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

ParsedLine Parser::ParseLine (const std::string & line, int lineNumber)
{
    HRESULT      hr                 = S_OK;
    ParsedLine   result             = {};
    std::string  stripped           = StripComments (line);   // comments go first
    std::string  trimmed            = Trim (stripped);
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
        result.label = Trim (remainder.substr (0, colonPos));
        remainder    = Trim (remainder.substr (colonPos + 1));
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
            result.directive = ToUpper (remainder);
        }
        else
        {
            result.directive    = ToUpper (remainder.substr (0, spacePos));
            result.directiveArg = Trim (remainder.substr (spacePos + 1));
        }

        // Unknown dotted spellings resolve to None and stay a string; the
        // pass-1 dispatch reports them as unhandled exactly as before.
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
        std::string beforeEq = Trim (remainder.substr (0, eqPos));
        std::string afterEq  = Trim (remainder.substr (eqPos + 1));

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
        std::string firstWord  = remainder.substr (0, spacePos);
        std::string afterFirst = Trim (remainder.substr (spacePos + 1));

        size_t sp2 = afterFirst.find_first_of (" \t");
        std::string  secondWord  = (sp2 == std::string::npos) ? afterFirst : afterFirst.substr (0, sp2);
        std::string  secondUpper = ToUpper (secondWord);

        if (secondUpper == "EQU" || secondUpper == "SET")
        {
            std::string expr = (sp2 == std::string::npos) ? "" : Trim (afterFirst.substr (sp2 + 1));

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
                         ? ToUpper (remainder)
                         : ToUpper (remainder.substr (0, spacePos));

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
            result.directiveArg = Trim (remainder.substr (spacePos + 1));
        }
    }

    BAIL_OUT_IF (isBareDirective, S_OK);

    // Plain instruction. The operand is whatever follows the mnemonic, if
    // anything -- an implied-mode instruction has none.
    result.mnemonic        = firstWordUpper;
    result.startsAtColumn0 = startsAtColumn0;

    if (spacePos != std::string::npos)
    {
        result.operand = Trim (remainder.substr (spacePos + 1));
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrimOperand
//
//  Was a byte-for-byte copy of Trim. Kept as a name because the operand
//  parsing below reads better saying what it is trimming, but there is one
//  implementation now.
//
////////////////////////////////////////////////////////////////////////////////

static std::string TrimOperand (const std::string & s)
{
    return Trim (s);
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
//  Depth-counted, so `((a+b)*c)` finds the OUTER close rather than the first
//  one. That is what lets an indirect operand hold a parenthesized expression:
//  `LDA ((base+2),X)` cannot be read by scanning for the next ')'.
//
//  A character literal is skipped whole, because `')'` contains a close
//  paren that is data. The `i + 2 < size` test is what identifies one: a
//  quote, any character, then a matching quote.
//
////////////////////////////////////////////////////////////////////////////////

static size_t FindMatchingClose (const std::string & s, size_t openPos)
{
    char    openChar  = s[openPos];
    char    closeChar = (openChar == '(') ? ')' : ']';
    int     depth     = 1;
    size_t  i         = 0;
    size_t  found     = std::string::npos;



    for (i = openPos + 1; i < s.size() && found == std::string::npos; i++)
    {
        char c = s[i];

        if (c == '\'' && i + 2 < s.size() && s[i + 2] == '\'')
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
                found = i;
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindTopLevelComma — find ',' not inside() [] or ''
//
//  The comma that separates operands, as opposed to one inside a bracketed
//  subexpression or a character literal. `LDA (addr,X)` has no top-level comma
//  at all, while `BBS0 $12,target` has one -- and the addressing mode turns on
//  telling those apart.
//
//  Brackets and parens share one depth counter rather than being tracked
//  separately, which means `([)]` would be accepted. Assembly syntax never
//  interleaves them, and one counter cannot misread a correctly-formed
//  operand.
//
////////////////////////////////////////////////////////////////////////////////

static size_t FindTopLevelComma (const std::string & s, size_t start = 0)
{
    int     depth = 0;
    size_t  i     = 0;
    size_t  found = std::string::npos;



    for (i = start; i < s.size() && found == std::string::npos; i++)
    {
        char c = s[i];

        if (c == '\'' && i + 2 < s.size() && s[i + 2] == '\'')
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
            found = i;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyOperand — syntax detection only, no value parsing
//
//  Reads an operand's SHAPE and nothing else: `#expr` is Immediate, `(e,X)` is
//  IndirectX, `(e),Y` is IndirectY. The expression inside is handed back
//  untouched for the evaluator.
//
//  Shape alone cannot finish the job -- `LDA $12` and `LDA $1234` are the same
//  shape and different modes -- so the VALUE decides zero-page against
//  absolute, later, once symbols exist. Splitting it here is what lets a
//  forward reference be classified before its value is known.
//
//  The `(e,X)` / `(e),Y` distinction is entirely about which side of the close
//  paren the comma falls on, which is why the inner text and the text after
//  are examined separately.
//
//  An unmatched paren degrades to a bare expression rather than erroring: it
//  may be a parenthesized arithmetic expression the evaluator will handle, and
//  if it really is malformed the evaluator produces the better message.
//
////////////////////////////////////////////////////////////////////////////////

ClassifiedOperand Parser::ClassifyOperand (const std::string & operand)
{
    ClassifiedOperand  result   = {};
    std::string        op       = TrimOperand (operand);
    std::string        inner;
    std::string        after;
    std::string        exprPart;
    std::string        reg;
    size_t             closePos = std::string::npos;
    size_t             commaPos = std::string::npos;



    // An empty operand keeps this.
    result.syntax = OperandSyntax::None;

    if (op.empty())
    {
        // Nothing to classify.
    }

    // Immediate: #expr
    else if (op[0] == '#')
    {
        result.syntax     = OperandSyntax::Immediate;
        result.expression = TrimOperand (op.substr (1));
    }

    // Accumulator: "A" (exact match, case-insensitive)
    else if (ToUpperStr (op) == "A")
    {
        result.syntax = OperandSyntax::Accumulator;
    }

    // Indirect modes: starts with '('
    else if (op[0] == '(')
    {
        closePos = FindMatchingClose (op, 0);

        if (closePos == std::string::npos)
        {
            // Unmatched paren — treat as bare expression
            result.syntax     = OperandSyntax::Bare;
            result.expression = op;
        }
        else
        {
            inner    = TrimOperand (op.substr (1, closePos - 1));
            after    = TrimOperand (op.substr (closePos + 1));
            commaPos = FindTopLevelComma (inner);
            reg      = (commaPos != std::string::npos)
                           ? ToUpperStr (TrimOperand (inner.substr (commaPos + 1)))
                           : std::string();

            // (expr,X) — IndirectX
            if (reg == "X")
            {
                result.syntax     = OperandSyntax::IndirectX;
                result.expression = TrimOperand (inner.substr (0, commaPos));
            }

            // (expr),Y — IndirectY
            else if (!after.empty() && after[0] == ',' &&
                     ToUpperStr (TrimOperand (after.substr (1))) == "Y")
            {
                result.syntax     = OperandSyntax::IndirectY;
                result.expression = inner;
            }

            // Plain (expr) — Indirect (for JMP)
            else
            {
                result.syntax     = OperandSyntax::Indirect;
                result.expression = inner;
            }
        }
    }
    else
    {
        // Top-level ,X or ,Y suffix, else a bare expression.
        commaPos = FindTopLevelComma (op);

        if (commaPos == std::string::npos)
        {
            result.syntax     = OperandSyntax::Bare;
            result.expression = op;
        }
        else
        {
            exprPart = TrimOperand (op.substr (0, commaPos));
            reg      = ToUpperStr (TrimOperand (op.substr (commaPos + 1)));

            if (reg == "X")
            {
                result.syntax     = OperandSyntax::IndexedX;
                result.expression = exprPart;
            }
            else if (reg == "Y")
            {
                result.syntax     = OperandSyntax::IndexedY;
                result.expression = exprPart;
            }
            else
            {
                // Comma but neither ,X nor ,Y — the two-operand zero-page,relative
                // form used by the 65C02 BBRn/BBSn bit-branch instructions
                // (zp,target). Not a valid 6502 form; ResolveAddressingMode maps it
                // to ZeroPageRelative and a mnemonic without that mode (i.e. any
                // 6502 mnemonic) fails the lookup.
                result.syntax           = OperandSyntax::ZeroPageRelative;
                result.expression       = exprPart;
                result.secondExpression = TrimOperand (op.substr (commaPos + 1));
            }
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseValue
//
//  A numeric literal in any of the assembler's three radices. Deliberately
//  strict: strtol must consume the WHOLE string, so `$FFg` is rejected rather
//  than quietly read as $FF -- a typo in a table of constants should fail
//  loudly, not assemble to the wrong byte.
//
//  This handles literals only, not expressions. Anything with an operator in
//  it belongs to ExpressionEvaluator; this is the fast path for the common
//  case and the one place a bare number is interpreted.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Parser::ParseValue (const std::string & text, int & value)
{
    // The three radices differ only in their prefix character and base:
    // '$' hex, '%' binary, and bare decimal with nothing to strip. All three
    // then require strtol to consume the whole string, so "$FFg" is rejected
    // rather than silently read as $FF.
    HRESULT       hr        = S_OK;
    std::string   digits;
    char        * endPtr    = nullptr;
    long          parsed    = 0;
    int           base      = 10;
    bool          hasText   = false;
    bool          hasDigits = false;
    bool          consumed  = false;



    hasText = !text.empty();
    CBREx (hasText, E_INVALIDARG);

    if      (text[0] == '$') { base = 16; digits = text.substr (1); }
    else if (text[0] == '%') { base =  2; digits = text.substr (1); }
    else                     { base = 10; digits = text;            }

    hasDigits = !digits.empty();
    CBREx (hasDigits, E_INVALIDARG);

    parsed   = strtol (digits.c_str(), &endPtr, base);
    consumed = (*endPtr == '\0');
    CBREx (consumed, E_INVALIDARG);

    value = (int) parsed;

Error:
    return hr;
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
//  Whether a name may be used as a label -- S_OK when it may, E_INVALIDARG
//  plus the reason in errorMessage when it may not.
//
//  All five conditions are evaluated before any is reported, so the message
//  names the FIRST rule broken rather than whichever test happened to run --
//  a label that is both malformed and a mnemonic reports the malformation,
//  which is what the author most likely meant to fix.
//
//  Mnemonic rejection is EXACT-CASE: `LDA` is refused outright while `lda` is
//  allowed and merely warned about at the definition site. That split is
//  deliberate -- a lowercase label matching a mnemonic is legal in period
//  sources and occasionally intentional, so it must not be a hard error here.
//
//  errorMessage is left untouched on success, so a caller may reuse one string
//  across many labels without clearing it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Parser::ValidateLabel (const std::string & label, const OpcodeTable & opcodeTable, std::string & errorMessage)
{
    HRESULT      hr         = S_OK;
    std::string  upper;
    char         first      = label.empty() ? '\0' : label[0];
    bool         isEmpty    = label.empty();
    bool         badFirst   = false;
    bool         badChar    = false;
    bool         isRegister = false;
    bool         isMnemonic = false;
    bool         valid      = false;



    if (!isEmpty)
    {
        // Must start with letter or underscore.
        badFirst = !isalpha ((unsigned char) first) && first != '_';

        // Must contain only alphanumeric + underscore.
        for (char c : label)
        {
            badChar = badChar || (!isalnum ((unsigned char) c) && c != '_');
        }

        // Must not be a register name (case-insensitive), nor an exact
        // mnemonic ("LDA" is rejected; "lda" is only a warning elsewhere).
        upper      = ToUpperValidate (label);
        isRegister = upper == "A" || upper == "X" || upper == "Y" || upper == "S";
        isMnemonic = opcodeTable.IsMnemonic (label);
    }

    // Reported in the same precedence order the guard chain used, so a label
    // failing several rules still names the first one. `errorMessage` is left
    // untouched when the label is good.
    if      (isEmpty)    { errorMessage = "Empty label name"; }
    else if (badFirst)   { errorMessage = "Label must start with a letter or underscore: " + label; }
    else if (badChar)    { errorMessage = "Label contains invalid character: " + label; }
    else if (isRegister) { errorMessage = "Label name conflicts with register name: " + label; }
    else if (isMnemonic) { errorMessage = "Label name conflicts with mnemonic: " + label; }
    else                 { valid = true; }

    hr = valid ? S_OK : E_INVALIDARG;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SplitArgList — split comma-separated list respecting() [] '' nesting
//
//  Splits on TOP-LEVEL commas only, so a bracketed subexpression or a
//  character literal containing a comma stays one argument -- `.byte (1,2)`
//  and `.byte ','` are each a single item.
//
//  Empty items are dropped rather than preserved, so `1,,2` yields two
//  arguments and a trailing comma is harmless. Every caller counts items to
//  size or emit data, and none has a use for a positional blank.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Parser::SplitArgList (const std::string & text)
{
    std::vector<std::string> args;
    size_t                   start = 0;



    while (start <= text.size())
    {
        size_t commaPos = FindTopLevelComma (text, start);

        if (commaPos == std::string::npos)
        {
            std::string arg = TrimOperand (text.substr (start));

            if (!arg.empty())
            {
                args.push_back (arg);
            }

            break;
        }

        std::string arg = TrimOperand (text.substr (start, commaPos - start));

        if (!arg.empty())
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
    std::string  trimmed = TrimOperand (text);
    std::string  inner;
    bool         isQuoted = trimmed.size() >= 2 &&
                            trimmed.front() == '"' && trimmed.back() == '"';



    // Anything not wrapped in a matched pair of double quotes yields empty.
    if (isQuoted)
    {
        inner = trimmed.substr (1, trimmed.size() - 2);
    }

    return inner;
}
