#include "Pch.h"

#include "CassoLink/LinkProgram.h"
#include "Machines/Apple2/Common/DirectBootBuilder.h"
#include "Assembler.h"
#include "Cpu.h"
#include "InstructionSetProvider.h"

#include "Shim.s.h"
#include "Program.s.h"
#include "Universal.s.h"
#include "SlotFinder.s.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleShim
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleShim (std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip)
{
    HRESULT  hr   = S_OK;
    bool     fits = false;



    hr = Assemble (GetShimSource(), kShimAddress, chip, outBytes, outErrors);
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

HRESULT LinkProgram::AssembleProgram (std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip)
{
    HRESULT  hr   = S_OK;
    bool     fits = false;



    hr = Assemble (GetProgramSource(), kProgramAddress, chip, outBytes, outErrors);
    CHR (hr);

    //  The program ends below the shim's copy of the zero page, and the shim,
    //  which it returns to.
    fits = kProgramAddress + outBytes.size() <= kZeroPageSave;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildShim
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildShim (const LinkShimOptions & options, std::vector<Byte> & outShim)
{
    HRESULT                    hr      = S_OK;
    std::string                errors;
    std::string                text    = std::to_string (options.baud);
    Byte                       control = 0;
    bool                       acia    = options.chip == LinkChip::Acia;
    bool                       known   = !acia || GetSpeedControl (options.baud, control);
    bool                       fits    = options.dial.size() < kDialLength;
    bool                       slotted = options.slot >= 1 && options.slot <= 7;
    const LinkNetworkConfig  & network = options.network;



    CBREx (known, E_INVALIDARG);
    CBREx (fits, E_INVALIDARG);
    CBREx (slotted, E_INVALIDARG);

    hr = AssembleShim (outShim, errors, options.chip);
    CHR (hr);

    outShim[kSlotOffset] = (Byte) options.slot;

    if (acia)
    {
        text.resize (kSpeedTextLength, ' ');

        outShim[kSpeedOffset] = control;
        std::copy (text.begin(), text.end(), outShim.begin() + kSpeedTextOffset);
        std::copy (options.dial.begin(), options.dial.end(), outShim.begin() + kDialOffset);
    }
    else
    {
        std::copy (network.address.begin(), network.address.end(), outShim.begin() + kNetAddressOffset);
        std::copy (network.mask.begin(),    network.mask.end(),    outShim.begin() + kNetMaskOffset);
        std::copy (network.gateway.begin(), network.gateway.end(), outShim.begin() + kNetGatewayOffset);
        std::copy (network.mac.begin(),     network.mac.end(),     outShim.begin() + kNetMacOffset);

        outShim[kNetPortOffset]     = (Byte) (network.port >> 8);
        outShim[kNetPortOffset + 1] = (Byte) network.port;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildShim
//
//  A 6551 shim for the //c's port 2.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildShim (DWORD baud, std::vector<Byte> & outShim, const std::string & dial)
{
    LinkShimOptions  options;



    options.baud = baud;
    options.dial = dial;

    return BuildShim (options, outShim);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildBootDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildBootDisk (const LinkShimOptions & options, std::vector<Byte> & outDisk)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  shim;
    DirectBootSpec     spec;
    std::string        refusal;



    hr = BuildShim (options, shim);
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
//  LinkProgram::BuildBootDisk
//
//  A boot disk for the //c's port 2.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildBootDisk (DWORD baud, std::vector<Byte> & outDisk, const std::string & dial)
{
    LinkShimOptions  options;



    options.baud = baud;
    options.dial = dial;

    return BuildBootDisk (options, outDisk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleUniversal
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleUniversal (std::vector<Byte> & outBytes, std::string & outErrors)
{
    HRESULT  hr   = S_OK;
    bool     fits = false;



    hr = Assemble (GetUniversalSource(), kUniversalAddress, LinkChip::Acia, outBytes, outErrors);
    CHR (hr);

    fits = outBytes.size() <= kUniversalLimit;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildUniversalBootDisk
//
//  The stage, the 6551 shim and the W5100 shim, each padded to the next's
//  address, loaded together at the stage's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildUniversalBootDisk (const LinkShimOptions & options, std::vector<Byte> & outDisk)
{
    HRESULT            hr      = S_OK;
    LinkShimOptions    acia    = options;
    LinkShimOptions    w5100   = options;
    std::vector<Byte>  payload;
    std::vector<Byte>  shim;
    std::string        errors;
    std::string        refusal;
    DirectBootSpec     spec;



    acia.chip  = LinkChip::Acia;
    acia.slot  = 2;
    w5100.chip = LinkChip::W5100;
    w5100.slot = 3;

    hr = AssembleUniversal (payload, errors);
    CHR (hr);

    payload.resize (kUniversalAcia - kUniversalAddress, 0);

    hr = BuildShim (acia, shim);
    CHR (hr);

    payload.insert (payload.end(), shim.begin(), shim.end());
    payload.resize (kUniversalW5100 - kUniversalAddress, 0);

    hr = BuildShim (w5100, shim);
    CHR (hr);

    payload.insert (payload.end(), shim.begin(), shim.end());
    payload.resize (kUniversalW5100 - kUniversalAddress + kShimLimit, 0);

    spec.loadAddress  = kUniversalAddress;
    spec.entryAddress = kUniversalAddress;

    hr = DirectBootBuilder::Build (payload, spec, outDisk, refusal);
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
//  LinkProgram::GetUniversalSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetUniversalSource()
{
    return std::string (reinterpret_cast<const char *> (s_kUniversalSource), sizeof (s_kUniversalSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetSlotFinderSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetSlotFinderSource()
{
    return std::string (reinterpret_cast<const char *> (s_kSlotFinderSource), sizeof (s_kSlotFinderSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleSlotFinder
//
//  The finder must end before the shim starts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleSlotFinder (std::vector<Byte> & outBytes, std::string & outErrors)
{
    HRESULT  hr   = S_OK;
    bool     fits = false;



    hr = Assemble (GetSlotFinderSource(), kSlotFinderAddress, LinkChip::Acia, outBytes, outErrors);
    CHR (hr);

    fits = outBytes.size() <= (size_t) (kShimAddress - kSlotFinderAddress);
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleSerialLoader
//
//  The loader must end before the shim starts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleSerialLoader (std::vector<Byte> & outBytes, std::string & outErrors)
{
    HRESULT  hr   = S_OK;
    bool     fits = false;



    hr = Assemble ("SERIALLOAD = 1\n" + GetSlotFinderSource(), kSerialLoaderAddress, LinkChip::Acia, outBytes, outErrors);
    CHR (hr);

    fits = outBytes.size() <= (size_t) (kShimAddress - kSerialLoaderAddress);
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildBootstrapImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildBootstrapImage (const LinkShimOptions & options, std::vector<Byte> & outImage, Word & outStart)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  shim;
    std::string        errors;



    outImage.clear();
    outStart = kShimAddress;

    hr = BuildShim (options, shim);
    CHR (hr);

    if (options.chip == LinkChip::Acia)
    {
        hr = AssembleSlotFinder (outImage, errors);
        CHR (hr);

        outImage.resize (kShimAddress - kSlotFinderAddress, 0);
        outStart = kSlotFinderAddress;
    }

    outImage.insert (outImage.end(), shim.begin(), shim.end());

Error:
    return hr;
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
//  A W5100 build is given W5100 ahead of the source, and its source selects
//  its own code with ifdef.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::Assemble (const std::string & source, Word origin, LinkChip chip, std::vector<Byte> & outBytes, std::string & outErrors)
{
    HRESULT          hr       = S_OK;
    static const Cpu s_cpu;
    Assembler        assembler (InstructionSetProvider (s_cpu.GetInstructionSet()));
    std::string      chipLine = (chip == LinkChip::W5100) ? "W5100 = 1\n" : "ACIA = 1\n";
    std::string      text     = std::format ("PROGVER = {}\nSHIMVER = {}\n", kProgramVersion, kShimVersion) + chipLine + source;
    AssemblyResult   result   = assembler.Assemble (text);
    bool             placed   = false;



    outBytes.clear();
    outErrors.clear();

    for (const AssemblyError & error : result.errors)
    {
        outErrors += std::format ("line {}: {}\n", error.lineNumber - 3, error.message);
    }

    CBREx (result.success, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    placed = result.startAddress == origin;
    CBREx (placed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outBytes = result.bytes;

Error:
    return hr;
}
