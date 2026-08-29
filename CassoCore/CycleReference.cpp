#include "Pch.h"

#include "CycleReference.h"

#include "Microcode.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Format
//
//  Assembles the document: prose the tables cannot state, two at-a-glance
//  grids, the full per-opcode listing, and the generated comparisons.
//
//  Ordered BY OPCODE rather than by mnemonic because of where a reader
//  arrives from. The question this answers is nearly always "I am looking at
//  $7D in a disassembly or a hex dump, what does it cost", and opcode order is
//  the only ordering that can be scanned by eye for a byte. It is also the only
//  one in which both cores fit on a single row: they agree on the opcode and on
//  very little else, so a mnemonic-ordered document would have to be written
//  twice and compared by hand.
//
//  The grids come first anyway, because "how many cycles" on its own is
//  answered faster by a 16x16 block than by finding a row in a 256-row table.
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::Format (const Microcode nmos[kOpcodeCount], const Microcode cmos[kOpcodeCount])
{
    std::string  document;



    document += FormatPreamble();
    document += FormatGrid        ("NMOS 6502", nmos);
    document += FormatGrid        ("65C02 (Rockwell R65C02)", cmos);
    document += FormatOpcodeTable (nmos, cmos);
    document += FormatTimingDiffs (nmos, cmos);
    document += FormatCmosOnly    (nmos, cmos);
    document += FormatFooter();

    return document;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatPreamble
//
//  The part of the document no table can supply: what the numbers are, and the
//  three costs that are deliberately absent from them.
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::FormatPreamble()
{
    std::string  text;



    text += "# Cycle Reference: 6502 and 65C02\n";
    text += "\n";
    text += "GENERATED FILE -- do not edit by hand. Regenerate with\n";
    text += "`scripts/UpdateCycleReference.ps1`; a unit test fails when this file and the\n";
    text += "emulator's instruction tables disagree.\n";
    text += "\n";
    text += "Every number here is read out of Casso's own `Microcode` tables -- the\n";
    text += "`baseCycles` the emulator actually bills for each instruction -- so the\n";
    text += "document cannot describe a machine other than the one this build emulates.\n";
    text += "\n";
    text += "## What the cycle column is\n";
    text += "\n";
    text += "`baseCycles` is the *unconditional* cost. Three costs are added at run time\n";
    text += "and are therefore NOT in the column; add them yourself for the real figure:\n";
    text += "\n";
    text += "1. **Page crossing, +1.** An indexed READ through `abs,X`, `abs,Y` or `(zp),Y`\n";
    text += "   pays one extra cycle when the effective address lands in a different page\n";
    text += "   from the base. Stores and read-modify-write instructions do not: the part\n";
    text += "   cannot know whether the page crossed until it has read, and it must write\n";
    text += "   either way, so that cycle is already inside their `baseCycles`.\n";
    text += "2. **Branch taken, +1; branch taken across a page, +1 more.** A conditional\n";
    text += "   branch not taken costs the 2 in the column. The 65C02's `BRA` is\n";
    text += "   unconditional and so always pays the taken cycle.\n";
    text += "3. **Decimal arithmetic on the 65C02, +1.** `ADC` and `SBC` cost one extra\n";
    text += "   cycle while the decimal flag is set, which is what buys the CMOS part its\n";
    text += "   correct N, V and Z in decimal mode. The NMOS core pays no such cycle.\n";
    text += "\n";
    text += "Those three are the whole of it in Casso's model, so two cycle counts on the\n";
    text += "same row differ only because the two cores bill that opcode differently. The\n";
    text += "real CMOS part has one more conditional cost that Casso does not yet model;\n";
    text += "see the closing section.\n";
    text += "\n";
    text += "## Reading the tables\n";
    text += "\n";
    text += "- `Len` is the whole instruction in bytes, opcode included.\n";
    text += "- `---` means the core does not implement that opcode. Only the NMOS column\n";
    text += "  ever shows it: the 65C02 defines all 256. Casso executes an unimplemented\n";
    text += "  opcode as a one-byte, two-cycle NOP and keeps running rather than trapping,\n";
    text += "  because period software does execute them.\n";
    text += "- A trailing `*` marks a slot the assembler will not emit: the NMOS\n";
    text += "  undocumented opcodes, and the 65C02's reserved opcode-map fill. They\n";
    text += "  execute and disassemble normally; they simply cannot be written by\n";
    text += "  mnemonic, so that a filler `NOP` can never shadow the canonical `$EA`.\n";
    text += "- Addressing modes are written as the operand syntax an assembler accepts.\n";
    text += "  `(abs)` appears for both cores at `$6C` even though the 65C02 fixes the\n";
    text += "  page-boundary bug there: the syntax is the same, the behavior is not.\n";
    text += "- The 65C02's fill slots are one byte and one cycle apart from the handful\n";
    text += "  that read operand bytes. `$5C` is the outlier worth knowing about: three\n";
    text += "  bytes and eight cycles, the most expensive NOP either part has.\n";
    text += "\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatGrid
//
//  A 16x16 block of base cycle counts, high nibble down the side and low nibble
//  across the top. This is the shape the question "what does $7D cost" is
//  actually asked in, and it fits on one screen where the detailed table does
//  not.
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::FormatGrid (const char * heading, const Microcode table[kOpcodeCount])
{
    std::string  text;
    int          high = 0;
    int          low  = 0;



    text += std::format ("## Base cycles at a glance: {}\n", heading);
    text += "\n";
    text += "```text\n";
    text += "     x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF\n";

    for (high = 0; high < kGridColumns; ++high)
    {
        text += std::format ("{:X}x  ", high);

        for (low = 0; low < kGridColumns; ++low)
        {
            const Microcode & entry = table[high * kGridColumns + low];

            text += entry.isLegal ? std::format (" {:2}", (int) entry.baseCycles) : std::string (" --");
        }

        text += "\n";
    }

    text += "```\n";
    text += "\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatOpcodeTable
//
//  The full listing, both cores on one row so the comparison needs no page
//  turning.
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::FormatOpcodeTable (const Microcode nmos[kOpcodeCount], const Microcode cmos[kOpcodeCount])
{
    std::string  text;
    int          opcode = 0;



    text += "## Every opcode\n";
    text += "\n";
    text += "| Op  | NMOS   | Mode     | Len | Cyc | 65C02  | Mode     | Len | Cyc |\n";
    text += "| --- | ------ | -------- | --- | --- | ------ | -------- | --- | --- |\n";

    for (opcode = 0; opcode < kOpcodeCount; ++opcode)
    {
        const Microcode & nmosEntry = nmos[opcode];
        const Microcode & cmosEntry = cmos[opcode];

        text += std::format ("| ${:02X} | {} | {} | {}   | {}   | {} | {} | {}   | {}   |\n",
                             opcode,
                             PadRight (DescribeMnemonic (nmosEntry), kMnemonicCell),
                             PadRight (DescribeMode     (nmosEntry), kModeCell),
                             DescribeLength (nmosEntry),
                             DescribeCycles (nmosEntry),
                             PadRight (DescribeMnemonic (cmosEntry), kMnemonicCell),
                             PadRight (DescribeMode     (cmosEntry), kModeCell),
                             DescribeLength (cmosEntry),
                             DescribeCycles (cmosEntry));
    }

    text += "\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatTimingDiffs
//
//  Opcodes both cores implement under the same mnemonic and charge differently
//  for. This is the short, interesting list -- the one a reader who already
//  knows the 6502 comes here for -- and it is derived rather than curated, so
//  an edit to either table shows up here whether or not anyone remembered to
//  write it down.
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::FormatTimingDiffs (const Microcode nmos[kOpcodeCount], const Microcode cmos[kOpcodeCount])
{
    std::string  text;
    std::string  rows;
    int          opcode = 0;
    int          count  = 0;



    for (opcode = 0; opcode < kOpcodeCount; ++opcode)
    {
        const Microcode & nmosEntry = nmos[opcode];
        const Microcode & cmosEntry = cmos[opcode];
        bool              bothReal  = IsAssemblable (nmosEntry) && IsAssemblable (cmosEntry);
        bool              sameName  = bothReal && DescribeMnemonic (nmosEntry) == DescribeMnemonic (cmosEntry);
        bool              sameCost  = nmosEntry.baseCycles == cmosEntry.baseCycles;

        if (!sameName || sameCost)
        {
            continue;
        }

        rows += std::format ("| ${:02X} | {} | {} | {}    | {}     |\n",
                             opcode,
                             PadRight (DescribeMnemonic (nmosEntry), kMnemonicCell),
                             PadRight (DescribeMode     (nmosEntry), kModeCell),
                             DescribeCycles (nmosEntry),
                             DescribeCycles (cmosEntry));
        ++count;
    }

    text += "## Where the two cores bill the same instruction differently\n";
    text += "\n";
    text += std::format ("{} opcode(s), found by comparing the two tables rather than by hand.\n", count);
    text += "\n";
    text += "| Op  | Mnem   | Mode     | NMOS | 65C02 |\n";
    text += "| --- | ------ | -------- | ---- | ----- |\n";
    text += rows;
    text += "\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCmosOnly
//
//  What the 65C02 does with the NMOS opcode map's holes. Kept apart from the
//  timing comparison because it answers a different question -- "can I write
//  this instruction" rather than "what does it cost" -- and because a counted
//  summary is the honest way to report the filler slots, which are numerous and
//  individually uninteresting.
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::FormatCmosOnly (const Microcode nmos[kOpcodeCount], const Microcode cmos[kOpcodeCount])
{
    std::string  text;
    std::string  rows;
    int          opcode   = 0;
    int          added    = 0;
    int          fillers  = 0;
    int          nmosOnly = 0;



    for (opcode = 0; opcode < kOpcodeCount; ++opcode)
    {
        const Microcode & nmosEntry = nmos[opcode];
        const Microcode & cmosEntry = cmos[opcode];
        bool              nmosReal  = IsAssemblable (nmosEntry);
        bool              cmosReal  = IsAssemblable (cmosEntry);

        if (!cmosReal)
        {
            ++fillers;
        }

        if (nmosReal && !cmosReal)
        {
            ++nmosOnly;
        }

        if (nmosReal || !cmosReal)
        {
            continue;
        }

        rows += std::format ("| ${:02X} | {} | {} | {}   | {}   |\n",
                             opcode,
                             PadRight (DescribeMnemonic (cmosEntry), kMnemonicCell),
                             PadRight (DescribeMode     (cmosEntry), kModeCell),
                             DescribeLength (cmosEntry),
                             DescribeCycles (cmosEntry));
        ++added;
    }

    text += "## Instructions the 65C02 adds\n";
    text += "\n";
    text += std::format ("{} opcode(s) the assembler can write for the 65C02 and not for the NMOS\n", added);
    text += std::format ("core. Another {} CMOS slot(s) are opcode-map fill that executes as a NOP,\n", fillers);
    text += std::format ("marked `*` in the main table, and {} opcode(s) the NMOS assembler can write\n", nmosOnly);
    text += "have no writable 65C02 equivalent.\n";
    text += "\n";
    text += "| Op  | Mnem   | Mode     | Len | Cyc |\n";
    text += "| --- | ------ | -------- | --- | --- |\n";
    text += rows;
    text += "\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatFooter
//
//  Provenance, and the one place Casso knowingly parts company with an oracle.
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::FormatFooter()
{
    std::string  text;



    text += "## Where Casso knowingly differs\n";
    text += "\n";
    text += "`$DB` is the only opcode whose modeling is a deliberate choice against an\n";
    text += "oracle. Casso decodes it as a one-byte NOP, following Klaus Dormann's\n";
    text += "functional test; Tom Harte's silicon-derived vectors model it as a two-byte\n";
    text += "NOP. The two oracles disagree about an undefined opcode no real software\n";
    text += "depends on, and the Harte harness skips that one slot as a result -- see\n";
    text += "`IsSkippedSlot` in `UnitTest/HarteTestRunner.cpp`. Everything else in the\n";
    text += "65C02 column, `$CB` and the 32 Rockwell bit-op slots included, does run\n";
    text += "against the Rockwell vectors.\n";
    text += "\n";
    text += "Note also that those vectors pin final machine STATE, not cycle counts, so\n";
    text += "the numbers in this document are not covered by that suite. The tables and\n";
    text += "this document are checked against each other; neither is checked against\n";
    text += "silicon.\n";
    text += "\n";
    text += "One gap follows from that, and the table above is what makes it visible.\n";
    text += "`ASL`, `LSR`, `ROL` and `ROR` in `abs,X` read 7 in both columns. The CMOS\n";
    text += "part is documented to bill 6 for those four unless the index carries the\n";
    text += "address into another page, in which case it bills 7 -- while `INC` and `DEC`\n";
    text += "in `abs,X` bill 7 either way, which is why they are not in the same group.\n";
    text += "Casso bills a flat 7 for all six, so those four opcodes run one cycle slow\n";
    text += "when no page boundary is crossed. Nothing in the suite catches it, since\n";
    text += "cycle counts are not what the vectors pin.\n";
    text += "\n";
    text += "## References consulted\n";
    text += "\n";
    text += "The timing above was verified against two public references. Neither is\n";
    text += "reproduced here, in whole or in part: this document is generated from Casso's\n";
    text += "own instruction tables, and the references were read only to check the\n";
    text += "result.\n";
    text += "\n";
    text += "- Bruce Clark, \"65C02 Opcodes\" -- <https://6502.org/tutorials/65c02opcodes.html>\n";
    text += "- Graham, 65C02 opcode matrix -- <http://www.oxyron.de/html/opcodes02.html>\n";
    text += "\n";
    text += "Both are published without stated reuse terms, so neither may be copied.\n";
    text += "That a given opcode takes a given number of cycles is a fact about the part\n";
    text += "and belongs to nobody; the tables above are Casso's own statement of it.\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeMnemonic
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::DescribeMnemonic (const Microcode & entry)
{
    std::string  name = "---";



    if (entry.isLegal)
    {
        name = entry.instructionName;

        if (entry.assemblerHidden)
        {
            name += "*";
        }
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeMode
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::DescribeMode (const Microcode & entry)
{
    std::string  mode = "---";



    if (entry.isLegal)
    {
        mode = ModeSyntax (entry.globalAddressingMode);
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeLength
//
//  The whole instruction, opcode byte included.
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::DescribeLength (const Microcode & entry)
{
    std::string  length = "-";



    if (entry.isLegal)
    {
        length = std::format ("{}", 1 + (int) OpcodeTable::GetOperandSize (entry.globalAddressingMode));
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeCycles
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::DescribeCycles (const Microcode & entry)
{
    std::string  cycles = "-";



    if (entry.isLegal)
    {
        cycles = std::format ("{}", (int) entry.baseCycles);
    }

    return cycles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PadRight
//
////////////////////////////////////////////////////////////////////////////////

std::string CycleReference::PadRight (const std::string & text, int width)
{
    std::string  padded = text;
    size_t       target = (size_t) width;



    if (padded.size() < target)
    {
        padded.resize (target, ' ');
    }

    return padded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ModeSyntax
//
//  The operand form an assembler accepts, rather than the prose name in
//  GlobalAddressingMode. A cycle reference is read next to source, and
//  "Jump Absolute" appears in no source file; `abs` does.
//
////////////////////////////////////////////////////////////////////////////////

const char * CycleReference::ModeSyntax (GlobalAddressingMode::AddressingMode mode)
{
    const char * syntax = "impl";



    switch (mode)
    {
    case GlobalAddressingMode::Immediate:           syntax = "#imm";    break;
    case GlobalAddressingMode::ZeroPage:            syntax = "zp";      break;
    case GlobalAddressingMode::ZeroPageX:           syntax = "zp,X";    break;
    case GlobalAddressingMode::ZeroPageY:           syntax = "zp,Y";    break;
    case GlobalAddressingMode::Absolute:            syntax = "abs";     break;
    case GlobalAddressingMode::AbsoluteX:           syntax = "abs,X";   break;
    case GlobalAddressingMode::AbsoluteY:           syntax = "abs,Y";   break;
    case GlobalAddressingMode::ZeroPageXIndirect:   syntax = "(zp,X)";  break;
    case GlobalAddressingMode::ZeroPageIndirectY:   syntax = "(zp),Y";  break;
    case GlobalAddressingMode::Accumulator:         syntax = "A";       break;
    case GlobalAddressingMode::JumpAbsolute:        syntax = "abs";     break;
    case GlobalAddressingMode::JumpIndirect:        syntax = "(abs)";   break;
    case GlobalAddressingMode::Relative:            syntax = "rel";     break;
    case GlobalAddressingMode::SingleByteNoOperand: syntax = "impl";    break;
    case GlobalAddressingMode::ZeroPageIndirect:    syntax = "(zp)";    break;
    case GlobalAddressingMode::AbsoluteXIndirect:   syntax = "(abs,X)"; break;
    case GlobalAddressingMode::ZeroPageRelative:    syntax = "zp,rel";  break;
    case GlobalAddressingMode::JumpIndirectCmos:    syntax = "(abs)";   break;
    default:                                                            break;
    }

    return syntax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAssemblable
//
//  Whether a source line can name this opcode. The hidden fills execute and
//  disassemble but are unreachable by mnemonic, which is exactly the
//  distinction the comparison sections need.
//
////////////////////////////////////////////////////////////////////////////////

bool CycleReference::IsAssemblable (const Microcode & entry)
{
    return entry.isLegal && !entry.assemblerHidden;
}
