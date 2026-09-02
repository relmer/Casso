#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////

struct DiskCommandResult
{
    //  Exit-status vocabulary, named so call sites do not write the numbers.
    //  The same meanings the assembler and run subcommands already assign.
    static constexpr int  kClean          = 0;
    static constexpr int  kWithComplaints = 1;
    static constexpr int  kNoOutput       = 2;

    int           exitStatus  = kClean;

    //  Set when what was WRONG was the command line rather than the disk:
    //  a command this grammar does not have, an operand it needed and did not
    //  get, an encoding nobody offers. The edge prints the disk page ahead
    //  of the diagnostic for these and not for the rest, because "PROG is
    //  not on this volume" is answered by a listing and not by a grammar.
    bool          badCommandLine = false;

    //  Whether the runner already put usage in `output`.
    //
    //  A MISSING OPERAND ANSWERS WITH ONE COMMAND'S BLOCK, and the edge prints
    //  the whole page for any bad command line. Without this the reader gets
    //  both: the block they wanted, and then eight commands they did not ask
    //  about, with the block scrolled off the top.
    bool          usageShown     = false;

    std::string        output;   // stdout, text
    std::string        diagnostics;   // stderr, always
    std::vector<Byte>  payload;   // stdout, binary
    bool               hasPayload  = false;

    //  The failure routine the F-suffixed macros call: the sentence goes
    //  to the diagnostics and the status says nothing was done.
    void  Fail (const std::string  & imagePath,
                const std::string  & name,
                const std::string  & sentence);

    //  Image, file, reason, in that order: a script's user needs to know
    //  which disk before anything else.
    static std::string  FormatFailure (const std::string & imagePath,
                                       const std::string & fileName,
                                       const std::string & reason);
};
