#pragma once

struct DisassembledInstruction;
class Disassembler;

class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  TraceLookahead
//
//  The instructions a stopped machine will run next, from the PC, as far as
//  the current registers decide them: a branch whose flags are known goes the
//  way it will go, and the walk ends at one whose flags an instruction on the
//  way may have changed, at a return, and at a jump through memory.
//
////////////////////////////////////////////////////////////////////////////////

class TraceLookahead
{
public:
    using PeekFn = std::function<bool (Word address, Byte & value)>;

    static std::vector<DisassembledInstruction>  FindNext (const Microcode * instructionSet,
                                                           Word              pc,
                                                           Byte              p,
                                                           const PeekFn    & peek,
                                                           size_t            count);

private:
    //  Whether a flag branch is taken with p; nothing for any other instruction.
    static std::optional<bool>  IsBranchTaken (const std::string & mnemonic, Byte p);

    //  Whether the flags are still known after the instruction, updating p for
    //  one that only sets or clears a flag.
    static bool  DoesKeepFlags (const std::string & mnemonic, Byte & p);

    //  Whether the walk ends after the instruction.
    static bool  IsEnd (const std::string & mnemonic);
};
