#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  As65ExitStatus
//
//  The statuses an as65-compatible assembly reports to the shell, and the one
//  decision that picks between them.
//
//  as65 documents five, and the wording is worth quoting because build scripts
//  branch on it:
//
//      0 - Source file assembled without errors.
//      1 - Incorrect parameter specified on the commandline.
//      2 - Unable to open input or output file.
//      3 - Assembly gave errors.
//      4 - No memory could be allocated.
//
//  ALL FOUR MEANINGS ARE HONORED EXACTLY, and 0 through 3 mean here what they
//  mean there. A build script ported from as65 branches correctly without being
//  read again, which is the whole point of claiming compatibility.
//
//  STATUS 4 IS as65'S, IS DOCUMENTED, AND IS NEVER PRODUCED. It belongs to a
//  16-bit tool that allocated its symbol table out of a 640 KB real-mode heap
//  and could genuinely run out assembling a large source. A 64 KB image on a
//  modern virtual-memory host does not reach that condition: the allocation that
//  failed would have taken the process down long before a status could be
//  returned. It is left unused rather than reassigned, so a script that still
//  tests for it is testing for something that cannot happen rather than catching
//  a different failure -- and it is named in the help for that script's author,
//  who would otherwise have to work out whether 4 had been renumbered.
//
//  WARNINGS REPORT 5, WHICH IS THIS TOOL'S OWN. as65 has no status for "it
//  assembled and the assembler had something to say", and the obvious place for
//  it was 1 -- which is where this tool used to put it, at the cost of the one
//  meaning as65 assigns that a script is most likely to branch on. A bad command
//  line is 1 now, as65 says it is, and warnings moved to the first number as65
//  does not define. 4 was skipped rather than taken because it is spoken for.
//
//  THE OUTPUT IS STILL WRITTEN UNDER 5. A warning is not a failure, and a script
//  that stops on any non-zero status will stop; one that wants the artifact can
//  test for 5 specifically, which it could not do when 5 was 1 and 1 also meant
//  the command line was wrong.
//
//  This lives in the core library rather than beside the console executable's
//  main because the test assembly does not link that executable. A status
//  decided next to the printing code is a decision nothing can check, which is
//  exactly how an assembly error came to report "could not open a file" for as
//  long as it did.
//
////////////////////////////////////////////////////////////////////////////////

class As65ExitStatus
{
public:
    static constexpr int  kClean          = 0;
    static constexpr int  kBadCommandLine = 1;
    static constexpr int  kNoOutput       = 2;
    static constexpr int  kAssemblyErrors = 3;
    static constexpr int  kWarned         = 5;

    //  `warned` is consulted only on the success path: an assembly that
    //  failed AND warned is a failed assembly, and reporting 5 would tell a
    //  script an output file exists when none does.
    static int  GetAssemblyStatus (bool inputWasRead, bool assembled, bool warned = false);
};
