#include "Pch.h"

#include "OpcodeTable.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetOperandSize
//
//  Operand bytes that follow the opcode, by addressing mode. Grouped by
//  size rather than listed one mode per line: the whole point of the
//  function is that these modes agree on a width, and the old one-return-
//  per-mode form buried that under nineteen near-identical lines.
//
////////////////////////////////////////////////////////////////////////////////

Byte OpcodeTable::GetOperandSize (GlobalAddressingMode::AddressingMode mode)
{
    Byte  size = 0;



    switch (mode)
    {
    // One operand byte: a zero-page address, an immediate value, or a
    // signed branch displacement.
    case GlobalAddressingMode::Immediate:
    case GlobalAddressingMode::ZeroPage:
    case GlobalAddressingMode::ZeroPageX:
    case GlobalAddressingMode::ZeroPageY:
    case GlobalAddressingMode::ZeroPageXIndirect:
    case GlobalAddressingMode::ZeroPageIndirectY:
    case GlobalAddressingMode::ZeroPageIndirect:      // 65C02 (zp)
    case GlobalAddressingMode::Relative:
        size = 1;
        break;

    // Two operand bytes: a full 16-bit address, or the 65C02 bit-branch
    // pair (zp byte + rel byte).
    case GlobalAddressingMode::Absolute:
    case GlobalAddressingMode::AbsoluteX:
    case GlobalAddressingMode::AbsoluteY:
    case GlobalAddressingMode::JumpAbsolute:
    case GlobalAddressingMode::JumpIndirect:
    case GlobalAddressingMode::JumpIndirectCmos:      // 65C02 (abs) page-fixed JMP
    case GlobalAddressingMode::AbsoluteXIndirect:     // 65C02 (abs,X) JMP
    case GlobalAddressingMode::ZeroPageRelative:      // 65C02 BBRn/BBSn
        size = 2;
        break;

    // No operand byte: the opcode is the whole instruction.
    case GlobalAddressingMode::Accumulator:
    case GlobalAddressingMode::SingleByteNoOperand:
    default:
        break;
    }

    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUpperCase
//
////////////////////////////////////////////////////////////////////////////////

static std::string ToUpperCase (const char * name)
{
    std::string result (name);



    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpcodeTable
//
////////////////////////////////////////////////////////////////////////////////

OpcodeTable::OpcodeTable()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCycleCounts — an instruction's base cycle count, for the -c listing
//
//  Read off the instruction ITSELF rather than out of a table keyed by opcode.
//  A keyed table can only describe one CPU, and this function is asked about
//  two: the 65C02 reuses dozens of slots the NMOS map leaves illegal, so an
//  NMOS-keyed table answered 0 for TSB, TRB, STZ, the (zp) modes, INC A / DEC A,
//  the extra BIT forms, PHX/PHY/PLX/PLY, JMP (abs,X) and the whole Rockwell
//  bit-op set -- and the listing then printed no count at all. Worse, where the
//  two CPUs share a slot but not a timing, the NMOS number was printed as if it
//  were the CMOS one: JMP (abs) at $6C listed as five where the 65C02 takes six.
//  Patching the table in place cannot fix either, because a slot legitimately
//  holds a different value per CPU -- $80 is the undocumented two-cycle NOP #imm
//  on NMOS and the three-cycle BRA on CMOS.
//
//  Microcode::baseCycles already carries the per-instruction count the emulator
//  bills for executing that very byte, and it is filled in by the same tables
//  this class inverts. Reading it is what keeps the listing and the emulator
//  from disagreeing about an instruction, the same reason the opcode mapping is
//  inverted from the Microcode table instead of being written out again.
//
//  What baseCycles deliberately excludes is exactly what a static listing
//  cannot know: the page-crossing cycle on an indexed read, the taken-branch
//  cycle, and the 65C02's extra cycle for ADC/SBC while the decimal flag is set.
//  All three are added at run time by StepOne and the operations, so the number
//  here is the base every execution pays and never an average.
//
//  An always-taken branch is the one instruction whose base is not its stored
//  count. BRA is stored as two so StepOne can add the taken cycle like any other
//  branch, but BRA has no not-taken case, so the listing states three.
//
////////////////////////////////////////////////////////////////////////////////

static Byte GetCycleCounts (const Microcode & instruction)
{
    constexpr Byte  kBranchAlwaysCycles = 3;



    return (instruction.operation == Microcode::BranchAlways) ? kBranchAlwaysCycles
                                                              : instruction.baseCycles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpcodeTable
//
//  Inverts the instruction set: builds a mnemonic-and-addressing-mode lookup
//  so the ASSEMBLER can find the opcode for what a source line says.
//
//  The CPU indexes by opcode; the assembler needs the opposite direction, and
//  building it from the same Microcode table is what keeps the two from ever
//  disagreeing about what an instruction does.
//
//  Two classes of opcode are deliberately EXCLUDED. Illegal opcodes and
//  assembler-hidden fills -- the 65C02's reserved-NOP slots, for instance --
//  still execute and still disassemble, but must not be selectable by
//  mnemonic. Including them would let a filler NOP shadow the real $EA, so a
//  plain `NOP` could assemble to the wrong byte.
//
//  Synonyms are aliased to an existing mnemonic's entries rather than given
//  their own, so an alias cannot drift from the instruction it names. They
//  exist because period assemblers accepted these spellings and period sources
//  still use them.
//
//  Mnemonics are uppercased on the way in, so lookup is case-insensitive
//  without a comparator.
//
////////////////////////////////////////////////////////////////////////////////

OpcodeTable::OpcodeTable (const Microcode instructionSet[256])
{
    for (int i = 0; i < 256; i++)
    {
        const Microcode & mc = instructionSet[i];

        // Skip illegal opcodes and assembler-hidden fills (e.g. the 65C02
        // reserved-NOP slots): they execute and disassemble, but must not be
        // selectable by mnemonic -- otherwise a filler NOP would shadow $EA.
        if (!mc.isLegal || mc.assemblerHidden)
        {
            continue;
        }

        std::string mnemonic = ToUpperCase (mc.instructionName);
        int         mode     = (int) mc.globalAddressingMode;

        OpcodeEntry entry = {};
        entry.opcode      = (Byte) i;
        entry.operandSize = GetOperandSize (mc.globalAddressingMode);
        entry.cycleCounts = GetCycleCounts (mc);

        m_table[mnemonic][mode] = entry;
    }

    // Instruction synonyms: alias → existing mnemonic's entries
    struct Synonym { const char * alias; const char * target; };
    Synonym synonyms[] =
    {
        { "DISABLE", "SEI" },
        { "ENABLE",  "CLI" },
        { "STC",     "SEC" },
        { "STI",     "SEI" },
        { "STD",     "SED" },
    };

    for (const auto & syn : synonyms)
    {
        auto it = m_table.find (syn.target);

        if (it != m_table.end())
        {
            m_table[syn.alias] = it->second;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindIgnoringCase
//
//  One instruction, however the source wrote its name.
//
//  The exact match is tried first and answers almost every call, so the
//  upper-casing is paid for only by a source that actually writes its opcodes
//  in lower case. The table's own keys are upper-cased when it is built, so the
//  second try can only match an entry the first would have matched given a
//  differently-cased source.
//
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, std::unordered_map<int, OpcodeEntry>>::const_iterator
OpcodeTable::FindIgnoringCase (const std::string & mnemonic) const
{
    auto  found = m_table.find (mnemonic);



    if (found == m_table.end())
    {
        found = m_table.find (ToUpperCase (mnemonic.c_str()));
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NamesAnInstruction
//
//  Whether an OPCODE FIELD names an instruction, in any case.
//
//  Deliberately not IsMnemonic, which answers the label question and must stay
//  exact-case. Two callers wanting opposite answers from one function is how a
//  legal lower-case label becomes a hard error.
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::NamesAnInstruction (const std::string & mnemonic) const
{
    return FindIgnoringCase (mnemonic) != m_table.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryLookup
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::TryLookup (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode, OpcodeEntry & result) const
{
    auto  mnemonicIt = FindIgnoringCase (mnemonic);
    bool  found      = false;



    // Two levels: an unknown mnemonic and a known one that lacks this
    // addressing mode both mean "no encoding", and `result` is untouched.
    if (mnemonicIt != m_table.end())
    {
        auto  modeIt = mnemonicIt->second.find ((int) mode);

        if (modeIt != mnemonicIt->second.end())
        {
            result = modeIt->second;
            found  = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMnemonic
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::IsMnemonic (const std::string & name) const
{
    return m_table.find (name) != m_table.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAllMnemonics
//
//  Every spelling the table answers to, synonyms included. Unordered, because
//  the map is: a caller wanting an order sorts, and pretending to one here would
//  be a promise the container does not keep.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> OpcodeTable::GetAllMnemonics() const
{
    std::vector<std::string>  names;



    names.reserve (m_table.size());

    for (const auto & entry : m_table)
    {
        names.push_back (entry.first);
    }

    return names;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasMode
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::HasMode (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode) const
{
    auto  mnemonicIt = FindIgnoringCase (mnemonic);



    // Short-circuit order guards the inner find: an unknown mnemonic has no
    // per-mode map to search.
    return mnemonicIt != m_table.end()
        && mnemonicIt->second.find ((int) mode) != mnemonicIt->second.end();
}
