#include "Pch.h"

#include "CycleReference.h"
#include "CountedNoun.h"

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
    text += "   from the base. Stores and read-modify-writes normally do not: the part\n";
    text += "   cannot know whether the page crossed until it has read, and it must write\n";
    text += "   either way, so that cycle is already inside their `baseCycles`. The 65C02's\n";
    text += "   `abs,X` shifts and rotates are the exception -- there the cost really is\n";
    text += "   conditional, and the table marks them with a trailing `+`.\n";
    text += "2. **Branch taken, +1; taken across a page, +1 more.** A conditional branch\n";
    text += "   not taken costs the 2 in the column, and the page is measured against the\n";
    text += "   instruction after the branch, so a displacement of zero is still a taken\n";
    text += "   branch and still costs the extra cycle. `BRA` is unconditional, so its real\n";
    text += "   minimum is 3 rather than the 2 stored for it here; `BBRn` and `BBSn` branch\n";
    text += "   as well, costing 5, 6 taken, and 7 taken across a page.\n";
    text += "3. **Decimal arithmetic on the 65C02, +1.** `ADC` and `SBC` cost one extra\n";
    text += "   cycle while the decimal flag is set, which is what buys the CMOS part its\n";
    text += "   correct N, V and Z in decimal mode. The NMOS core pays no such cycle.\n";
    text += "\n";
    text += "Those three are the whole of it, so two cycle counts on the same row differ\n";
    text += "only because the two cores genuinely bill that opcode differently.\n";
    text += "\n";
    text += "## Reading the tables\n";
    text += "\n";
    text += "- `Len` is the whole instruction in bytes, opcode included.\n";
    text += "- `---` means the core does not implement that opcode. Only the NMOS column\n";
    text += "  ever shows it: the 65C02 defines all 256. Casso executes an unimplemented\n";
    text += "  opcode as a one-byte, two-cycle NOP and keeps running rather than trapping,\n";
    text += "  because period software does execute them.\n";
    text += "- A trailing `+` on a cycle count marks an instruction that pays one more\n";
    text += "  cycle only when the indexed address crosses a page. Ordinary indexed reads\n";
    text += "  pay that cycle too and are not marked; the marker is for the read-modify-\n";
    text += "  writes, where the same opcode is conditional on one core and flat on the\n";
    text += "  other.\n";
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

            text += std::format ("{:>3}", entry.isLegal ? DescribeCycles (entry) : std::string ("--"));
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

        text += std::format ("| ${:02X} | {} | {} | {} | {} | {} | {} | {} | {} |\n",
                             opcode,
                             PadRight (DescribeMnemonic (nmosEntry), kMnemonicCell),
                             PadRight (DescribeMode     (nmosEntry), kModeCell),
                             PadRight (DescribeLength   (nmosEntry), kNumberCell),
                             PadRight (DescribeCycles   (nmosEntry), kNumberCell),
                             PadRight (DescribeMnemonic (cmosEntry), kMnemonicCell),
                             PadRight (DescribeMode     (cmosEntry), kModeCell),
                             PadRight (DescribeLength   (cmosEntry), kNumberCell),
                             PadRight (DescribeCycles   (cmosEntry), kNumberCell));
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

        rows += std::format ("| ${:02X} | {} | {} | {}   | {}   |\n",
                             opcode,
                             PadRight (DescribeMnemonic (nmosEntry), kMnemonicCell),
                             PadRight (DescribeMode     (nmosEntry), kModeCell),
                             PadRight (DescribeCycles   (nmosEntry), kNumberCell),
                             PadRight (DescribeCycles   (cmosEntry), kNumberCell));
        ++count;
    }

    text += "## Where the two cores bill the same instruction differently\n";
    text += "\n";
    text += std::format ("{}, found by comparing the two tables rather than by hand.\n",
                         CountedNoun::Of (count, "opcode"));
    text += "\n";
    text += "| Op  | Mnem   | Mode     | NMOS  | 65C02 |\n";
    text += "| --- | ------ | -------- | ----- | ----- |\n";
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

        rows += std::format ("| ${:02X} | {} | {} | {} | {} |\n",
                             opcode,
                             PadRight (DescribeMnemonic (cmosEntry), kMnemonicCell),
                             PadRight (DescribeMode     (cmosEntry), kModeCell),
                             PadRight (DescribeLength   (cmosEntry), kNumberCell),
                             PadRight (DescribeCycles   (cmosEntry), kNumberCell));
        ++added;
    }

    text += "## Instructions the 65C02 adds\n";
    text += "\n";
    text += std::format ("{} the assembler can write for the 65C02 and not for the NMOS\n",
                         CountedNoun::Of (added, "opcode"));
    text += std::format ("core. Another {} are opcode-map fill that executes as a NOP,\n",
                         CountedNoun::Of (fillers, "CMOS slot"));
    text += std::format ("marked `*` in the main table, and {} the NMOS assembler can write\n",
                         CountedNoun::Of (nmosOnly, "opcode"));
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



    text += "## How these numbers are checked\n";
    text += "\n";
    text += "Tom Harte's SingleStepTests vectors record what each instruction really cost\n";
    text += "for that vector's own operands, so every count here is compared against\n";
    text += "recorded hardware behavior with the conditional cycles already in it: the\n";
    text += "page crossing that happened, the branch that was taken, the decimal `ADC`.\n";
    text += "That runs at 200 vectors per opcode on every build and 10,000 on demand,\n";
    text += "across the documented, undocumented and Rockwell 65C02 tiers alike. What it\n";
    text += "does not pin is WHICH cycle a given bus access lands on; only the total is\n";
    text += "kept.\n";
    text += "\n";
    text += "## Where Casso differs from the upstream vectors\n";
    text += "\n";
    text += "Three undefined 65C02 opcodes, where the corpus and the published per-opcode\n";
    text += "tables disagree. Casso follows the tables, and the harness carries the\n";
    text += "exemption by name rather than quietly passing:\n";
    text += "\n";
    text += "- `$DB` -- a one-byte NOP here, two bytes upstream. Klaus Dormann's\n";
    text += "  functional test asserts one byte, so the whole opcode is skipped.\n";
    text += "- `$5C` -- 8 cycles here, 4 upstream. Only the cycle comparison is skipped.\n";
    text += "- `$CB` -- 1 cycle here, 2 upstream. Only the cycle comparison is skipped.\n";
    text += "\n";
    text += "Everything else about `$5C` and `$CB` -- registers, flags, memory, and how\n";
    text += "many bytes the opcode swallows -- is still compared. The reasoning is in the\n";
    text += "\"Disputed slots\" section of `docs/testing.md`.\n";
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
        mode = GetModeSyntax (entry.globalAddressingMode);
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

        if (entry.crossingAPageCostsACycle)
        {
            cycles += "+";
        }
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
//  GetModeSyntax
//
//  The operand form an assembler accepts, rather than the prose name in
//  GlobalAddressingMode. A cycle reference is read next to source, and
//  "Jump Absolute" appears in no source file; `abs` does.
//
////////////////////////////////////////////////////////////////////////////////

const char * CycleReference::GetModeSyntax (GlobalAddressingMode::AddressingMode mode)
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
