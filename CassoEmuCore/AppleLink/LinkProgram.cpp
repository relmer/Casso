#include "Pch.h"

#include "AppleLink/LinkProgram.h"
#include "Assembler.h"
#include "Cpu.h"
#include "InstructionSetProvider.h"

#include "Shim.s.h"
#include "Program.s.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleShim
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleShim (std::vector<Byte> & outBytes, std::string & outErrors)
{
    HRESULT  hr   = S_OK;
    bool     fits = false;



    hr = Assemble (GetShimSource(), kShimAddress, outBytes, outErrors);
    CHR (hr);

    fits = outBytes.size() <= kShimLimit;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleProgram (std::vector<Byte> & outBytes, std::string & outErrors)
{
    return Assemble (GetProgramSource(), kProgramAddress, outBytes, outErrors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetShimSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetShimSource()
{
    return std::string (reinterpret_cast<const char *> (s_kShimSource), sizeof (s_kShimSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetProgramSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetProgramSource()
{
    return std::string (reinterpret_cast<const char *> (s_kProgramSource), sizeof (s_kProgramSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::Assemble
//
//  The source starts with its own .org; the result must begin at `origin`,
//  so a source edited to load elsewhere fails here rather than on the Apple.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::Assemble (const std::string & source, Word origin, std::vector<Byte> & outBytes, std::string & outErrors)
{
    HRESULT          hr       = S_OK;
    static const Cpu s_cpu;
    Assembler        assembler (InstructionSetProvider (s_cpu.GetInstructionSet()));
    std::string      text     = std::format ("PROGVER = {}\nSHIMVER = {}\n", kProgramVersion, kShimVersion) + source;
    AssemblyResult   result   = assembler.Assemble (text);
    bool             placed   = false;



    outBytes.clear();
    outErrors.clear();

    for (const AssemblyError & error : result.errors)
    {
        outErrors += std::format ("line {}: {}\n", error.lineNumber - 2, error.message);
    }

    CBREx (result.success, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    placed = result.startAddress == origin;
    CBREx (placed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outBytes = result.bytes;

Error:
    return hr;
}
