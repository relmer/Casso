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

    // Zero-page preference: if we classified as ZeroPage but lookup failed, try Absolute
    // (or vice versa, for forward references that default to Absolute)
    return 0;
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

        // Record label
        if (!info.parsed.label.empty ())
        {
            symbols[info.parsed.label] = pc;
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

        // Compute instruction size for PC advancement
        OpcodeEntry entry = {};

        if (m_opcodeTable.Lookup (info.parsed.mnemonic, info.classified.mode, entry))
        {
            pc += 1 + entry.operandSize;
        }
        else
        {
            // T016: Zero-page preference — if classified as ZeroPage but no match,
            // try Absolute mode (needed for forward references where value unknown)
            GlobalAddressingMode::AddressingMode altMode = info.classified.mode;

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

            if (altMode != info.classified.mode && m_opcodeTable.Lookup (info.parsed.mnemonic, altMode, entry))
            {
                info.classified.mode = altMode;
                pc += 1 + entry.operandSize;
            }
            else
            {
                // Unknown instruction
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
        return result;
    }

    // ---- Pass 2: Emit bytes ----
    std::vector<Byte> output;

    for (const auto & info : lineInfos)
    {
        if (!info.isInstruction)
        {
            continue;
        }

        // T016: Zero-page preference — if value fits in byte and mnemonic supports
        // ZeroPage mode, use the shorter encoding
        GlobalAddressingMode::AddressingMode mode = info.classified.mode;
        int                                  value = info.classified.value;

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
