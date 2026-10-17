#include "Pch.h"

#include "CassoLink/LinkProgram.h"
#include "Machines/Apple2/Common/DirectBootBuilder.h"
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
    HRESULT  hr   = S_OK;
    bool     fits = false;



    hr = Assemble (GetProgramSource(), kProgramAddress, outBytes, outErrors);
    CHR (hr);

    //  The program ends below the shim, which it returns to.
    fits = kProgramAddress + outBytes.size() <= kShimAddress;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildShim
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildShim (DWORD baud, std::vector<Byte> & outShim, const std::string & dial)
{
    HRESULT      hr      = S_OK;
    std::string  errors;
    std::string  text    = std::to_string (baud);
    Byte         control = 0;
    bool         known   = GetSpeedControl (baud, control);
    bool         fits    = dial.size() < kDialLength;



    CBREx (known, E_INVALIDARG);
    CBREx (fits, E_INVALIDARG);

    hr = AssembleShim (outShim, errors);
    CHR (hr);

    text.resize (kSpeedTextLength, ' ');

    outShim[kSpeedOffset] = control;
    std::copy (text.begin(), text.end(), outShim.begin() + kSpeedTextOffset);
    std::copy (dial.begin(), dial.end(), outShim.begin() + kDialOffset);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildBootDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildBootDisk (DWORD baud, std::vector<Byte> & outDisk, const std::string & dial)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  shim;
    DirectBootSpec     spec;
    std::string        refusal;



    hr = BuildShim (baud, shim, dial);
    CHR (hr);

    spec.loadAddress  = kShimAddress;
    spec.entryAddress = kShimAddress;

    hr = DirectBootBuilder::Build (shim, spec, outDisk, refusal);
    CHR (hr);

Error:
    return hr;
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
//  LinkProgram::GetSpeedControl
//
//  Rate 0 is the 6551's 16x external clock, 115,200 baud on the //c.
//
////////////////////////////////////////////////////////////////////////////////

bool LinkProgram::GetSpeedControl (DWORD baud, Byte & outControl)
{
    static constexpr std::pair<DWORD, Byte>  s_kRates[] =
    {
        { 115200, 0x00 }, { 19200, 0x0F }, { 9600, 0x0E }, { 4800, 0x0C }, { 2400, 0x0A }, { 1200, 0x08 },
    };
    static constexpr Byte  kEightBitsOneStop = 0x10;
    bool                   found             = false;



    for (const auto & [rate, bits] : s_kRates)
    {
        if (rate == baud)
        {
            outControl = (Byte) (kEightBitsOneStop | bits);
            found      = true;
        }
    }

    return found;
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
