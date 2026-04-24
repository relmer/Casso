#include "Pch.h"

#include "Assembler.h"
#include "Parser.h"



Assembler::Assembler (const Microcode instructionSet[256], AssemblerOptions options) :
    m_opcodeTable (instructionSet),
    m_options     (options)
{
}



void Assembler::RecordWarning (AssemblyResult & result, int lineNumber, const std::string & message)
{
    switch (m_options.warningMode)
    {
    case WarningMode::Warn:
    {
        AssemblyError warning = {};
        warning.lineNumber = lineNumber;
        warning.message    = message;
        result.warnings.push_back (warning);
        break;
    }

    case WarningMode::FatalWarnings:
    {
        AssemblyError error = {};
        error.lineNumber = lineNumber;
        error.message    = message;
        result.errors.push_back (error);
        result.success = false;
        break;
    }

    case WarningMode::NoWarn:
        // Discard silently
        break;
    }
}



static Byte GetInstructionSize (const OpcodeTable & table, const ParsedLine & parsed, const ClassifiedOperand & classified)
{
    OpcodeEntry entry = {};

    if (table.Lookup (parsed.mnemonic, classified.mode, entry))
    {
        return 1 + entry.operandSize;
    }

    return 0;
}



static Byte EstimateInstructionSize (const ClassifiedOperand & classified)
{
    switch (classified.mode)
    {
    case GlobalAddressingMode::SingleByteNoOperand:
    case GlobalAddressingMode::Accumulator:
        return 1;

    case GlobalAddressingMode::Immediate:
    case GlobalAddressingMode::ZeroPage:
    case GlobalAddressingMode::ZeroPageX:
    case GlobalAddressingMode::ZeroPageY:
    case GlobalAddressingMode::ZeroPageXIndirect:
    case GlobalAddressingMode::ZeroPageIndirectY:
    case GlobalAddressingMode::Relative:
        return 2;

    case GlobalAddressingMode::Absolute:
    case GlobalAddressingMode::AbsoluteX:
    case GlobalAddressingMode::AbsoluteY:
    case GlobalAddressingMode::JumpAbsolute:
    case GlobalAddressingMode::JumpIndirect:
        return 3;

    default:
        return 1;
    }
}



static bool IsBranchMnemonic (const std::string & mnemonic)
{
    return mnemonic == "BPL" || mnemonic == "BMI" ||
           mnemonic == "BVC" || mnemonic == "BVS" ||
           mnemonic == "BCC" || mnemonic == "BCS" ||
           mnemonic == "BNE" || mnemonic == "BEQ";
}



AssemblyResult Assembler::Assemble (const std::string & sourceText)
{
    AssemblyResult result = {};
    result.success      = true;
    result.startAddress = 0x8000;

    auto lines = Parser::SplitLines (sourceText);

    // ---- Pass 1: Parse lines, collect labels, compute PC values ----
    struct LineInfo
    {
        ParsedLine        parsed;
        ClassifiedOperand classified;
        Word              pc;
        bool              isInstruction;
        bool              isDirective;
        bool              hasError;
    };

    std::vector<LineInfo>                     lineInfos;
    std::unordered_map<std::string, Word>     symbols;
    Word                                      pc        = result.startAddress;
    bool                                      originSet = false;

    for (int i = 0; i < (int) lines.size (); i++)
    {
        LineInfo info    = {};
        info.parsed      = Parser::ParseLine (lines[i], i + 1);
        info.pc          = pc;
        info.isInstruction = false;
        info.isDirective   = false;

        // Handle .org BEFORE recording label (so label gets the new address)
        if (info.parsed.isDirective && info.parsed.directive == ".ORG")
        {
            int orgAddr = 0;

            if (Parser::ParseValue (info.parsed.directiveArg, orgAddr))
            {
                Word newAddr = (Word) orgAddr;

                if (originSet && newAddr < pc)
                {
                    AssemblyError error = {};
                    error.lineNumber = i + 1;
                    error.message    = ".org address is backward from current position";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    if (newAddr == pc)
                    {
                        RecordWarning (result, i + 1, "Redundant .org to current address");
                    }

                    pc = newAddr;
                    info.pc = pc;

                    if (!originSet)
                    {
                        result.startAddress = newAddr;
                        originSet = true;
                    }
                }
            }

            info.isDirective = true;
            lineInfos.push_back (info);
            continue;
        }

        // Record label with validation
        if (!info.parsed.label.empty ())
        {
            std::string labelError;

            if (!Parser::ValidateLabel (info.parsed.label, m_opcodeTable, labelError))
            {
                AssemblyError error = {};
                error.lineNumber = i + 1;
                error.message    = labelError;
                result.errors.push_back (error);
                result.success = false;
            }
            else if (symbols.count (info.parsed.label) > 0)
            {
                AssemblyError error = {};
                error.lineNumber = i + 1;
                error.message    = "Duplicate label: " + info.parsed.label;
                result.errors.push_back (error);
                result.success = false;
            }
            else
            {
                symbols[info.parsed.label] = pc;

                // Warn if label differs from a mnemonic only by case (FR-033a)
                std::string upper = info.parsed.label;

                for (auto & c : upper)
                {
                    c = (char) toupper ((unsigned char) c);
                }

                if (upper != info.parsed.label && m_opcodeTable.IsMnemonic (upper))
                {
                    RecordWarning (result, i + 1, "Label name resembles mnemonic: " + info.parsed.label);
                }
            }
        }

        // Handle data directives (.byte, .word, .text)
        if (info.parsed.isDirective)
        {
            info.isDirective = true;

            if (info.parsed.directive == ".BYTE")
            {
                auto values = Parser::ParseValueList (info.parsed.directiveArg);
                pc += (Word) values.size ();
            }
            else if (info.parsed.directive == ".WORD")
            {
                auto values = Parser::ParseValueList (info.parsed.directiveArg);
                pc += (Word) (values.size () * 2);
            }
            else if (info.parsed.directive == ".TEXT")
            {
                std::string text = Parser::ParseQuotedString (info.parsed.directiveArg);
                pc += (Word) text.size ();
            }

            lineInfos.push_back (info);
            continue;
        }

        // Skip empty lines (comments, blanks)
        if (info.parsed.mnemonic.empty ())
        {
            lineInfos.push_back (info);
            continue;
        }

        // Classify operand
        info.classified    = Parser::ClassifyOperand (info.parsed.operand, info.parsed.mnemonic);
        info.isInstruction = true;

        // For labels used as operands, default to Absolute size (2-byte operand)
        // in Pass 1 so PC advances correctly
        GlobalAddressingMode::AddressingMode mode = info.classified.mode;

        if (info.classified.isLabel)
        {
            // Branches are always 1-byte relative offset
            if (mode == GlobalAddressingMode::Relative)
            {
                // Relative mode — 1 byte operand
            }
            else if (mode == GlobalAddressingMode::ZeroPage)
            {
                // Label operand defaults to Absolute (2 bytes)
                mode = GlobalAddressingMode::Absolute;
                info.classified.mode = mode;
            }
        }

        // Compute instruction size for PC advancement
        OpcodeEntry entry = {};

        if (m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
        {
            pc += 1 + entry.operandSize;
        }
        else
        {
            // Zero-page preference fallback — try alternate mode
            GlobalAddressingMode::AddressingMode altMode = mode;

            if (altMode == GlobalAddressingMode::ZeroPage)
            {
                altMode = GlobalAddressingMode::Absolute;
            }
            else if (altMode == GlobalAddressingMode::ZeroPageX)
            {
                altMode = GlobalAddressingMode::AbsoluteX;
            }
            else if (altMode == GlobalAddressingMode::ZeroPageY)
            {
                altMode = GlobalAddressingMode::AbsoluteY;
            }

            if (altMode != mode && m_opcodeTable.Lookup (info.parsed.mnemonic, altMode, entry))
            {
                info.classified.mode = altMode;
                pc += 1 + entry.operandSize;
            }
            else
            {
                AssemblyError error = {};
                error.lineNumber = i + 1;

                if (!m_opcodeTable.IsMnemonic (info.parsed.mnemonic))
                {
                    error.message = "Invalid mnemonic: " + info.parsed.mnemonic;
                }
                else if (info.classified.mode == GlobalAddressingMode::SingleByteNoOperand)
                {
                    error.message = "Missing operand for: " + info.parsed.mnemonic;
                }
                else
                {
                    error.message = "Invalid addressing mode for: " + info.parsed.mnemonic;
                }

                result.errors.push_back (error);
                result.success = false;
                info.hasError  = true;

                // Best-effort PC estimation (T042)
                pc += EstimateInstructionSize (info.classified);
            }
        }

        lineInfos.push_back (info);
    }

    // ---- Pass 2: Emit bytes with label resolution ----
    std::vector<Byte>                  output;
    std::unordered_map<std::string, int> referencedLabels;  // label name → line number of first reference

    for (const auto & info : lineInfos)
    {
        size_t bytesStart     = output.size ();
        bool   lineHasAddress = false;

        if (info.hasError)
        {
            // Nothing to emit for error lines
        }
        else if (info.isDirective)
        {
            lineHasAddress = true;

            if (info.parsed.directive == ".BYTE")
            {
                auto values = Parser::ParseValueList (info.parsed.directiveArg);

                for (int v : values)
                {
                    output.push_back ((Byte) (v & 0xFF));
                }
            }
            else if (info.parsed.directive == ".WORD")
            {
                auto values = Parser::ParseValueList (info.parsed.directiveArg);

                for (int v : values)
                {
                    output.push_back ((Byte) (v & 0xFF));
                    output.push_back ((Byte) ((v >> 8) & 0xFF));
                }
            }
            else if (info.parsed.directive == ".TEXT")
            {
                std::string text = Parser::ParseQuotedString (info.parsed.directiveArg);

                for (char c : text)
                {
                    output.push_back ((Byte) c);
                }
            }
        }
        else if (info.isInstruction)
        {
            lineHasAddress = true;

            GlobalAddressingMode::AddressingMode mode  = info.classified.mode;
            int                                  value  = info.classified.value;
            bool                                 emit   = true;

            // Resolve label references
            if (info.classified.isLabel)
            {
                referencedLabels[info.classified.labelName] = info.parsed.lineNumber;

                auto it = symbols.find (info.classified.labelName);

                if (it == symbols.end ())
                {
                    AssemblyError error = {};
                    error.lineNumber = info.parsed.lineNumber;
                    error.message    = "Undefined label: " + info.classified.labelName;
                    result.errors.push_back (error);
                    result.success = false;
                    emit = false;

                    // Emit placeholder bytes
                    OpcodeEntry entry = {};

                    if (m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
                    {
                        output.push_back (entry.opcode);

                        for (Byte j = 0; j < entry.operandSize; j++)
                        {
                            output.push_back (0x00);
                        }
                    }
                }
                else
                {
                    value = (int) it->second;

                    // Apply label offset (label+N expressions)
                    value += info.classified.labelOffset;

                    // Apply low/high byte operators (<label, >label)
                    if (info.classified.lowByteOp)
                    {
                        value = value & 0xFF;
                    }
                    else if (info.classified.highByteOp)
                    {
                        value = (value >> 8) & 0xFF;
                    }

                    // Compute relative branch offset
                    if (mode == GlobalAddressingMode::Relative)
                    {
                        Word pcAfterInstruction = info.pc + 2; // branch instructions are 2 bytes
                        int  offset = value - (int) pcAfterInstruction;

                        if (offset < -128 || offset > 127)
                        {
                            AssemblyError error = {};
                            error.lineNumber = info.parsed.lineNumber;
                            error.message    = "Branch target out of range: " + info.classified.labelName;
                            result.errors.push_back (error);
                            result.success = false;
                        }

                        value = offset & 0xFF;
                    }
                }
            }

            if (emit)
            {
                // Zero-page preference for non-label operands
                if (!info.classified.isLabel)
                {
                    if (value >= 0 && value <= 0xFF)
                    {
                        GlobalAddressingMode::AddressingMode zpMode = GlobalAddressingMode::__Count;

                        if (mode == GlobalAddressingMode::Absolute)
                        {
                            zpMode = GlobalAddressingMode::ZeroPage;
                        }
                        else if (mode == GlobalAddressingMode::AbsoluteX)
                        {
                            zpMode = GlobalAddressingMode::ZeroPageX;
                        }
                        else if (mode == GlobalAddressingMode::AbsoluteY)
                        {
                            zpMode = GlobalAddressingMode::ZeroPageY;
                        }

                        if (zpMode != GlobalAddressingMode::__Count)
                        {
                            OpcodeEntry zpEntry = {};

                            if (m_opcodeTable.Lookup (info.parsed.mnemonic, zpMode, zpEntry))
                            {
                                mode = zpMode;
                            }
                        }
                    }
                }

                // Check operand value range
                if (mode == GlobalAddressingMode::Immediate && (value < -128 || value > 255))
                {
                    AssemblyError error = {};
                    error.lineNumber = info.parsed.lineNumber;
                    error.message    = "Operand value out of range for immediate mode: " + std::to_string (value);
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    OpcodeEntry entry = {};

                    if (!m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
                    {
                        AssemblyError error = {};
                        error.lineNumber = info.parsed.lineNumber;
                        error.message    = "Cannot encode: " + info.parsed.mnemonic;
                        result.errors.push_back (error);
                        result.success = false;
                    }
                    else
                    {
                        output.push_back (entry.opcode);

                        if (entry.operandSize == 1)
                        {
                            output.push_back ((Byte) (value & 0xFF));
                        }
                        else if (entry.operandSize == 2)
                        {
                            output.push_back ((Byte) (value & 0xFF));
                            output.push_back ((Byte) ((value >> 8) & 0xFF));
                        }
                    }
                }
            }
        }
        else if (!info.parsed.label.empty ())
        {
            // Label-only line — show address but no bytes
            lineHasAddress = true;
        }

        // Build listing entry
        if (m_options.generateListing)
        {
            AssemblyLine listLine = {};
            listLine.lineNumber = info.parsed.lineNumber;
            listLine.sourceText = lines[info.parsed.lineNumber - 1];
            listLine.hasAddress = lineHasAddress;
            listLine.address    = info.pc;

            for (size_t j = bytesStart; j < output.size (); j++)
            {
                listLine.bytes.push_back (output[j]);
            }

            result.listing.push_back (listLine);
        }
    }

    result.bytes      = output;
    result.symbols    = symbols;
    result.endAddress = result.startAddress + (Word) output.size ();

    // ---- Post-assembly warnings ----
    // Warn about unused labels (defined but never referenced)
    for (const auto & sym : symbols)
    {
        if (referencedLabels.find (sym.first) == referencedLabels.end ())
        {
            // Find the line number where the label was defined
            int defLine = 0;

            for (const auto & info : lineInfos)
            {
                if (info.parsed.label == sym.first)
                {
                    defLine = info.parsed.lineNumber;
                    break;
                }
            }

            RecordWarning (result, defLine, "Unused label: " + sym.first);
        }
    }

    return result;
}



std::string Assembler::FormatListingLine (const AssemblyLine & line)
{
    char addrBuf[8] = {};

    // Address column (5 chars)
    if (line.hasAddress)
    {
        snprintf (addrBuf, sizeof (addrBuf), "$%04X", line.address);
    }
    else
    {
        snprintf (addrBuf, sizeof (addrBuf), "     ");
    }

    // Bytes column (up to 3 hex bytes, padded to 10 chars)
    std::string bytesStr;

    for (size_t i = 0; i < line.bytes.size () && i < 3; i++)
    {
        char hexBuf[4];

        if (i > 0)
        {
            bytesStr += " ";
        }

        snprintf (hexBuf, sizeof (hexBuf), "%02X", line.bytes[i]);
        bytesStr += hexBuf;
    }

    while (bytesStr.size () < 10)
    {
        bytesStr += " ";
    }

    return std::string (addrBuf) + "  " + bytesStr + line.sourceText;
}
