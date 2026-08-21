#include "Pch.h"

#include "AssemblerExitCode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerExitCode::FromResult
//
//  Maps one assembly's outcome onto the three codes.
//
//  The failure test is `success` rather than "are there errors", because those
//  two are the same fact recorded twice and only one of them is authoritative:
//  every recorded error clears the flag, and warnings-as-errors clears it while
//  filing a diagnostic that was raised as a warning. Reading the error list
//  instead would answer correctly today and diverge the moment a failure is
//  recorded without a message.
//
//  Warnings are consulted ONLY on the success path. A failed assembly that also
//  warned is still a failed assembly, and reporting the warning code would tell
//  a script an output file exists when none does.
//
////////////////////////////////////////////////////////////////////////////////

AssemblyExitCode AssemblerExitCode::FromResult (const AssemblyResult & result)
{
    bool  assembled   = result.success;
    bool  hasWarnings = !result.warnings.empty();



    if (!assembled)
    {
        return AssemblyExitCode::NoOutput;
    }

    return hasWarnings ? AssemblyExitCode::AssembledWithWarnings : AssemblyExitCode::Clean;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerExitCode::ToProcessCode
//
//  The enumerators hold the numbers, so this is the cast and nothing more. It
//  exists so the executable states what it is returning instead of casting an
//  enum, and so the numbers stay a fact a test can pin in core.
//
////////////////////////////////////////////////////////////////////////////////

int AssemblerExitCode::ToProcessCode (AssemblyExitCode code)
{
    return (int) code;
}
