#pragma once

#include "AssemblerTypes.h"
#include "GlobalAddressingModes.h"





class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  OpcodeTable
//
////////////////////////////////////////////////////////////////////////////////

class OpcodeTable
{
public:
    OpcodeTable ();
    OpcodeTable (const Microcode instructionSet[256]);

    //  THE OPCODE-FIELD LOOKUPS IGNORE CASE; the label question below does not.
    //
    //  Those are different questions and the difference is load-bearing. An
    //  opcode field asks "which instruction is this", and a source writing it in
    //  lower case is naming the same instruction -- as65 has always taken either
    //  case, and Merlin source written in a modern editor arrives that way.
    //
    //  A LABEL asks something else: is this word, exactly as the author wrote
    //  it, the name of an instruction. `lda` as a label is legal in period
    //  sources and occasionally deliberate, so IsMnemonic stays exact-case and
    //  Parser::ValidateLabel goes on admitting it with a warning. Making that
    //  one case-insensitive would turn a legal label into a hard error.
    bool TryLookup             (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode, OpcodeEntry & result) const;
    bool HasMode               (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode) const;
    bool NamesAnInstruction    (const std::string & mnemonic) const;

    //  Exact-case: for asking whether a word AS WRITTEN is an instruction name.
    bool IsMnemonic            (const std::string & name) const;

    // Operand bytes that follow the opcode, by addressing mode. Public and
    // static because it is a property of the MODE, not of a built table: the
    // cycle reference needs the length of opcodes this table deliberately
    // excludes (illegal slots and assembler-hidden fills), which no OpcodeEntry
    // exists to answer for.
    static Byte GetOperandSize (GlobalAddressingMode::AddressingMode mode);

    // Every spelling this table answers to, so a sweep can ask about all of them
    // rather than about the ones somebody listed -- the same reason
    // DirectiveTable::GetAllSpellings exists. Built from the instruction set, so
    // an opcode gaining a mnemonic joins it without anyone editing a test.
    std::vector<std::string> GetAllMnemonics () const;

private:
    //  The table keyed by mnemonic, tried as written and then upper-cased. The
    //  exact try comes first so an already-upper-case lookup -- which is every
    //  lookup as65 makes, and most that Merlin makes -- costs no allocation.
    std::unordered_map<std::string, std::unordered_map<int, OpcodeEntry>>::const_iterator
         FindIgnoringCase (const std::string & mnemonic) const;

    std::unordered_map<std::string, std::unordered_map<int, OpcodeEntry>> m_table;
};
