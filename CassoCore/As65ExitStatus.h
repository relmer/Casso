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
//  FOUR OF THE FIVE ARE PRODUCED HERE. Status 4 is not, and cannot honestly be:
//  it belongs to a 16-bit tool that allocated its symbol table out of a 640 KB
//  real-mode heap and could genuinely run out assembling a large source. A
//  64 KB image on a modern virtual-memory host does not reach that condition --
//  the allocation that failed would have taken the process down long before a
//  status could be returned. It is named in the help as as65's rather than
//  quietly omitted, so a script porting from as65 can see the difference was
//  noticed instead of wondering which status replaced it.
//
//  Status 1 also diverges, deliberately and visibly: as65 spends it on a bad
//  command line, and this tool spends it on "assembled, and had something to
//  say" -- an assembler warning, or a flag that was dropped. The help states
//  that under the assemble mode rather than leaving the as65 wording to imply
//  otherwise.
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
    static constexpr int  kWarned         = 1;
    static constexpr int  kNoOutput       = 2;
    static constexpr int  kAssemblyErrors = 3;

    static int  ForAssembly (bool inputWasRead, bool assembled);
};
