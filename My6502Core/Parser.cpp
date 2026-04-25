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

    // Strip comments first
    std::string stripped = StripComments (line);
    std::string trimmed  = Trim (stripped);

    if (trimmed.empty ())
    {
        return result;
    }

    result.isEmpty = false;
    std::string remainder = trimmed;

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

    // Extract mnemonic (first word)
    size_t spacePos = remainder.find_first_of (" \t");

    if (spacePos == std::string::npos)
    {
        result.mnemonic = ToUpper (remainder);
        return result;
    }

    result.mnemonic = ToUpper (remainder.substr (0, spacePos));
    result.operand  = Trim (remainder.substr (spacePos + 1));

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBranchMnemonic
//
////////////////////////////////////////////////////////////////////////////////

static bool IsBranchMnemonic (const std::string & mnemonic)
{
    return mnemonic == "BPL" || mnemonic == "BMI" ||
           mnemonic == "BVC" || mnemonic == "BVS" ||
           mnemonic == "BCC" || mnemonic == "BCS" ||
           mnemonic == "BNE" || mnemonic == "BEQ";
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
//  ParseLabelWithOffset
//
////////////////////////////////////////////////////////////////////////////////

static void ParseLabelWithOffset (const std::string & text, ClassifiedOperand & result)
{
    result.isLabel = true;

    size_t plusPos = text.find ('+');

    if (plusPos != std::string::npos)
    {
        result.labelName = TrimOperand (text.substr (0, plusPos));

        std::string offsetStr = TrimOperand (text.substr (plusPos + 1));
        int         offset    = 0;

        if (Parser::ParseValue (offsetStr, offset))
        {
            result.labelOffset = offset;
        }
    }
    else
    {
        result.labelName = text;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyOperand
//
////////////////////////////////////////////////////////////////////////////////

ClassifiedOperand Parser::ClassifyOperand (const std::string & operand, const std::string & mnemonic)
{
    ClassifiedOperand result = {};
    result.mode        = GlobalAddressingMode::SingleByteNoOperand;
    result.value       = 0;
    result.isLabel     = false;
    result.labelOffset = 0;
    result.lowByteOp   = false;
    result.highByteOp  = false;

    std::string op = TrimOperand (operand);

    // No operand — implied / single byte
    if (op.empty ())
    {
        result.mode = GlobalAddressingMode::SingleByteNoOperand;
        return result;
    }

    // Immediate: #value
    if (op[0] == '#')
    {
        std::string valueStr = op.substr (1);
        result.mode = GlobalAddressingMode::Immediate;

        // Check for < (low byte) or > (high byte) operator
        if (!valueStr.empty () && valueStr[0] == '<')
        {
            result.lowByteOp = true;
            valueStr = valueStr.substr (1);
        }
        else if (!valueStr.empty () && valueStr[0] == '>')
        {
            result.highByteOp = true;
            valueStr = valueStr.substr (1);
        }

        if (!ParseValue (valueStr, result.value))
        {
            ParseLabelWithOffset (valueStr, result);
        }

        return result;
    }

    // Accumulator: "A"
    if (ToUpperStr (op) == "A")
    {
        result.mode = GlobalAddressingMode::Accumulator;
        return result;
    }

    // Indirect modes: (...)
    if (op[0] == '(')
    {
        size_t closeParen = op.find (')');

        if (closeParen == std::string::npos)
        {
            return result;
        }

        std::string inner = op.substr (1, closeParen - 1);

        // ($ZZ,X) — ZeroPageXIndirect
        size_t commaPos = inner.find (',');

        if (commaPos != std::string::npos)
        {
            std::string valueStr = TrimOperand (inner.substr (0, commaPos));
            std::string reg      = ToUpperStr (TrimOperand (inner.substr (commaPos + 1)));

            if (reg == "X")
            {
                result.mode = GlobalAddressingMode::ZeroPageXIndirect;
                ParseValue (valueStr, result.value);
                return result;
            }
        }

        // ($ZZ),Y — ZeroPageIndirectY
        // ($HHHH) — JumpIndirect
        std::string afterParen = TrimOperand (op.substr (closeParen + 1));

        if (!afterParen.empty () && afterParen[0] == ',')
        {
            std::string reg = ToUpperStr (TrimOperand (afterParen.substr (1)));

            if (reg == "Y")
            {
                result.mode = GlobalAddressingMode::ZeroPageIndirectY;
                ParseValue (TrimOperand (inner), result.value);
                return result;
            }
        }

        // JMP ($HHHH) — JumpIndirect
        result.mode = GlobalAddressingMode::JumpIndirect;
        ParseValue (TrimOperand (inner), result.value);
        return result;
    }

    // Check for ,X or ,Y suffix
    size_t commaPos = op.find (',');

    if (commaPos != std::string::npos)
    {
        std::string valueStr = TrimOperand (op.substr (0, commaPos));
        std::string reg      = ToUpperStr (TrimOperand (op.substr (commaPos + 1)));
        int         value    = 0;
        bool        isNumeric = ParseValue (valueStr, value);

        if (reg == "X")
        {
            if (isNumeric && value >= 0 && value <= 0xFF)
            {
                result.mode  = GlobalAddressingMode::ZeroPageX;
                result.value = value;
            }
            else
            {
                result.mode = GlobalAddressingMode::AbsoluteX;

                if (isNumeric)
                {
                    result.value = value;
                }
                else
                {
                    ParseLabelWithOffset (valueStr, result);
                }
            }

            return result;
        }

        if (reg == "Y")
        {
            if (isNumeric && value >= 0 && value <= 0xFF)
            {
                result.mode  = GlobalAddressingMode::ZeroPageY;
                result.value = value;
            }
            else
            {
                result.mode = GlobalAddressingMode::AbsoluteY;

                if (isNumeric)
                {
                    result.value = value;
                }
                else
                {
                    ParseLabelWithOffset (valueStr, result);
                }
            }

            return result;
        }

        return result;
    }

    // Plain value or label — determine mode from mnemonic and value
    int  value     = 0;
    bool isNumeric = ParseValue (op, value);

    // Branch instructions always use Relative mode
    if (IsBranchMnemonic (mnemonic))
    {
        result.mode = GlobalAddressingMode::Relative;

        if (isNumeric)
        {
            result.value = value;
        }
        else
        {
            ParseLabelWithOffset (op, result);
        }

        return result;
    }

    // JMP uses JumpAbsolute (not Absolute)
    if (mnemonic == "JMP")
    {
        result.mode = GlobalAddressingMode::JumpAbsolute;

        if (isNumeric)
        {
            result.value = value;
        }
        else
        {
            ParseLabelWithOffset (op, result);
        }

        return result;
    }

    // JSR uses JumpAbsolute
    if (mnemonic == "JSR")
    {
        result.mode = GlobalAddressingMode::JumpAbsolute;

        if (isNumeric)
        {
            result.value = value;
        }
        else
        {
            ParseLabelWithOffset (op, result);
        }

        return result;
    }

    // Numeric literal — ZeroPage or Absolute based on value size
    if (isNumeric)
    {
        if (value >= 0 && value <= 0xFF)
        {
            result.mode  = GlobalAddressingMode::ZeroPage;
            result.value = value;
        }
        else
        {
            result.mode  = GlobalAddressingMode::Absolute;
            result.value = value;
        }

        return result;
    }

    // Must be a label — default to Absolute (will be resolved in Pass 2)
    result.mode = GlobalAddressingMode::Absolute;
    ParseLabelWithOffset (op, result);
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
//  ParseValueList
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> Parser::ParseValueList (const std::string & text)
{
    std::vector<int> values;
    std::string      current;

    for (size_t i = 0; i <= text.size (); i++)
    {
        if (i == text.size () || text[i] == ',')
        {
            std::string trimmed = TrimOperand (current);

            if (!trimmed.empty ())
            {
                int value = 0;

                if (ParseValue (trimmed, value))
                {
                    values.push_back (value);
                }
            }

            current.clear ();
        }
        else
        {
            current += text[i];
        }
    }

    return values;
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
