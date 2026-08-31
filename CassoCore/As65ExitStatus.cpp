#include "Pch.h"

#include "As65ExitStatus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetAssemblyStatus
//
//  The status for one attempt to assemble one source file.
//
//  THE TWO FAILURES ARE DIFFERENT FAILURES, which is the whole reason this
//  takes two arguments rather than one. A source file that could not be opened
//  and a source file full of errors both leave the assembler with nothing to
//  write, so the code that ran them reduced both to "no output" and returned
//  as65's status 2 for each. as65 does not: 2 is "unable to open input or
//  output file" and 3 is "assembly gave errors", and a build script telling
//  those apart is deciding between "your path is wrong" and "your code is
//  wrong". Reporting 2 for a syntax error sent every such script down the first
//  branch.
//
//  The caller is expected to have already dealt with there being no input file
//  named at all -- that is a command line question, answered before anything is
//  opened, and it shares status 2 for the reason as65 gives it: nothing could
//  be read.
//
////////////////////////////////////////////////////////////////////////////////

int As65ExitStatus::GetAssemblyStatus (bool inputWasRead, bool assembled, bool warned)
{
    int  status = kClean;



    if (!inputWasRead)
    {
        status = kNoOutput;
    }
    else if (!assembled)
    {
        status = kAssemblyErrors;
    }
    else if (warned)
    {
        status = kWarned;
    }

    return status;
}
