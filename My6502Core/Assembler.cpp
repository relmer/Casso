#include "Pch.h"

#include "Assembler.h"
#include "Parser.h"



Assembler::Assembler (const Microcode instructionSet[256], AssemblerOptions options) :
    m_opcodeTable (instructionSet),
    m_options     (options)
{
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
    };

    std::vector<LineInfo>                     lineInfos;
    std::unordered_map<std::string, Word>     symbols;
    Word                                      pc = result.startAddress;

    for (int i = 0; i < (int) lines.size (); i++)
    {
        LineInfo info  = {};
        info.parsed    = Parser::ParseLine (lines[i], i + 1);
        info.pc        = pc;
        info.isInstruction = false;

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
            }
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
                error.message    = "Invalid instruction: " + info.parsed.mnemonic +
                                   " with addressing mode " + std::to_string ((int) info.classified.mode);
                result.errors.push_back (error);
                result.success = false;
            }
        }

        lineInfos.push_back (info);
    }

    if (!result.success)
    {
        result.symbols = symbols;
        return result;
    }

    // ---- Pass 2: Emit bytes with label resolution ----
    std::vector<Byte> output;

    for (const auto & info : lineInfos)
    {
        if (!info.isInstruction)
        {
            continue;
        }

        GlobalAddressingMode::AddressingMode mode  = info.classified.mode;
        int                                  value  = info.classified.value;
        bool                                 resolved = true;

        // Resolve label references
        if (info.classified.isLabel)
        {
            auto it = symbols.find (info.classified.labelName);

            if (it == symbols.end ())
            {
                AssemblyError error = {};
                error.lineNumber = info.parsed.lineNumber;
                error.message    = "Undefined label: " + info.classified.labelName;
                result.errors.push_back (error);
                result.success = false;
                resolved = false;

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

                continue;
            }

            value = (int) it->second;

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

        if (!resolved)
        {
            continue;
        }

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

        OpcodeEntry entry = {};

        if (!m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
        {
            AssemblyError error = {};
            error.lineNumber = info.parsed.lineNumber;
            error.message    = "Cannot encode: " + info.parsed.mnemonic;
            result.errors.push_back (error);
            result.success = false;
            continue;
        }

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

    result.bytes      = output;
    result.symbols    = symbols;
    result.endAddress = result.startAddress + (Word) output.size ();

    return result;
}
