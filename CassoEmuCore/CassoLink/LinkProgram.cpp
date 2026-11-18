#include "Pch.h"

#include "CassoLink/LinkProgram.h"
#include "CassoLink/CassoLinkCodec.h"
#include "Machines/Apple2/Common/DirectBootBuilder.h"
#include "Assembler.h"
#include "Cpu.h"
#include "InstructionSetProvider.h"

#include "Shim.s.h"
#include "Program.s.h"
#include "Universal.s.h"
#include "SlotFinder.s.h"
#include "Capture.s.h"
#include "Block.s.h"
#include "ThirteenSector.s.h"
#include "VirtualDrive.s.h"
#include "SosInterp.s.h"
#include "NetStack.s.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleShim
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleShim (std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip)
{
    HRESULT                                hr       = S_OK;
    std::unordered_map<std::string, Word>  symbols;
    size_t                                 resident = 0;
    bool                                   fits     = false;



    //  The network build is the shim with the stack after it, at $B000,
    //  below $C000.
    if (chip == LinkChip::Network)
    {
        hr = Assemble (GetShimSource() + GetNetStackSource(), kNetShimAddress, chip, outBytes, outErrors);
        CHR (hr);

        fits = outBytes.size() <= kNetShimLimit;
    }
    else
    {
        hr = Assemble (GetShimSource(), kShimAddress, chip, outBytes, outErrors, false, &symbols);
        CHR (hr);

        //  The resident code ends where the startup part begins, or with the
        //  shim for a build with no startup part.
        resident = symbols.contains ("startup") ? (size_t) (symbols["startup"] - kShimAddress) : outBytes.size();
        fits     = outBytes.size() <= kShimLimit && kShimAddress + resident <= kShimResidentEnd;
    }

    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleProgram (std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip, bool faults)
{
    HRESULT      hr     = S_OK;
    bool         sos    = chip == LinkChip::Sos;
    std::string  source = sos ? GetSosInterpSource() : GetProgramSource();
    Word         origin = sos ? kSosAddress : kProgramAddress;
    bool         fits   = false;



    hr = Assemble (source, origin, chip, outBytes, outErrors, faults);
    CHR (hr);

    //  The program ends below the shim's copy of the zero page, and the shim,
    //  which it returns to. The SOS build ends with room for its speed's byte
    //  below its buffers.
    fits = sos ? kSosAddress + outBytes.size() < kSosBufferBase
               : kProgramAddress + outBytes.size() <= kZeroPageSave;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildShimUpdate
//
//  A boot disk works after the shim changes: an older shim loads
//  this in place of the program, as it loads any program. The updater
//  copies the old shim's settings into the current shim's image, which are
//  the same bytes of the header in every version: the 6551 control byte at
//  +$02, the speed's text and the dial command from +$06, the saved state,
//  reset vector and slot to +$38, the firmware flag at +$39 and a IIgs's
//  speed register at +$43. It then copies the image over the old shim and
//  starts it. The saved state is copied too, so the new shim doesn't save
//  the zero page again, and the host sends the program after its Ready.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildShimUpdate (std::vector<Byte> & outBytes)
{
    static constexpr const char *  kSource =
        "SHIM     = $3A00\n"
        "IMAGE    = $0900\n"
        "        .org $0800\n"
        "        jmp entry\n"
        "        .word 0\n"
        "entry:  sei\n"
        "        cld\n"
        "        lda SHIM+$02\n"
        "        sta IMAGE+$02\n"
        "        ldx #$06\n"
        "keep:   lda SHIM,x\n"
        "        sta IMAGE,x\n"
        "        inx\n"
        "        cpx #$3A\n"
        "        bne keep\n"
        "        lda SHIM+$43\n"
        "        sta IMAGE+$43\n"
        "        ldx #0\n"
        "copy:   lda IMAGE,x\n"
        "        sta SHIM,x\n"
        "        lda IMAGE+$100,x\n"
        "        sta SHIM+$100,x\n"
        "        lda IMAGE+$200,x\n"
        "        sta SHIM+$200,x\n"
        "        lda IMAGE+$300,x\n"
        "        sta SHIM+$300,x\n"
        "        lda IMAGE+$400,x\n"
        "        sta SHIM+$400,x\n"
        "        lda IMAGE+$500,x\n"
        "        sta SHIM+$500,x\n"
        "        lda IMAGE+$600,x\n"
        "        sta SHIM+$600,x\n"
        "        lda IMAGE+$700,x\n"
        "        sta SHIM+$700,x\n"
        "        lda IMAGE+$800,x\n"
        "        sta SHIM+$800,x\n"
        "        lda IMAGE+$900,x\n"
        "        sta SHIM+$900,x\n"
        "        inx\n"
        "        bne copy\n"
        "        jmp SHIM\n";



    HRESULT            hr     = S_OK;
    std::vector<Byte>  shim;
    std::string        errors;
    LinkShimOptions    options;
    bool               fits   = false;



    hr = Assemble (kSource, kProgramAddress, LinkChip::Acia, outBytes, errors);
    CHR (hr);

    hr = BuildShim (options, shim);
    CHR (hr);

    fits = outBytes.size() <= (size_t) (kUpdateImage - kProgramAddress) && shim.size() <= kShimLimit;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_BUFFER_OVERFLOW));

    outBytes.resize (kUpdateImage - kProgramAddress, 0);
    outBytes.insert (outBytes.end(), shim.begin(), shim.end());

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

        if (options.chip == LinkChip::Network)
        {
            outShim[kNetChipOffset] = network.netChip;
        }
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
    LinkShimOptions    network = options;
    std::vector<Byte>  payload;
    std::vector<Byte>  shim;
    std::string        errors;
    std::string        refusal;
    DirectBootSpec     spec;



    acia.chip  = LinkChip::Acia;
    acia.slot  = 2;
    w5100.chip = LinkChip::W5100;
    w5100.slot = 3;

    //  The stage writes the network card's slot and chip when it finds one.
    network.chip            = LinkChip::Network;
    network.slot            = 1;
    network.network.netChip = 0;

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
    payload.resize (kUniversalNetwork - kUniversalAddress, 0);

    hr = BuildShim (network, shim);
    CHR (hr);

    payload.insert (payload.end(), shim.begin(), shim.end());
    payload.resize (kUniversalNetwork - kUniversalAddress + kNetShimLimit, 0);

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

std::string LinkProgram::GetNetStackSource()
{
    return std::string (reinterpret_cast<const char *> (s_kNetStackSource), sizeof (s_kNetStackSource) - 1);
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
//  LinkProgram::GetCaptureModuleSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetCaptureModuleSource()
{
    return std::string (reinterpret_cast<const char *> (s_kCaptureModuleSource), sizeof (s_kCaptureModuleSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildCaptureModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildCaptureModule (std::vector<Byte> & outModule, LinkChip chip, bool faults)
{
    std::string  errors;



    return AssembleModule (GetCaptureModuleSource(), outModule, errors, chip, faults);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetBlockModuleSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetBlockModuleSource()
{
    return std::string (reinterpret_cast<const char *> (s_kBlockModuleSource), sizeof (s_kBlockModuleSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildBlockModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildBlockModule (std::vector<Byte> & outModule, LinkChip chip, bool faults)
{
    HRESULT      hr = S_OK;
    std::string  errors;



    if (chip == LinkChip::Sos)
    {
        MakeSosBlockModule (outModule);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = AssembleModule (GetBlockModuleSource(), outModule, errors, chip, faults);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::MakeSosBlockModule
//
//  The SOS build has the block requests in it, so its block module is the
//  header alone, with no requests: the id and version the SOS build reports
//  as loaded, so the host never sends it.
//
////////////////////////////////////////////////////////////////////////////////

void LinkProgram::MakeSosBlockModule (std::vector<Byte> & outModule)
{
    static constexpr Byte  kMagic[] = { 'C', 'M' };
    Word                   crc      = 0;



    outModule.assign (kModuleCrcStart + 1, 0);

    outModule[0]                 = kMagic[0];
    outModule[1]                 = kMagic[1];
    outModule[2]                 = kBlockModuleId;
    outModule[3]                 = kBlockModuleVersion;
    outModule[kModuleLength - 2] = (Byte) (kProgramVersion & 0xFF);
    outModule[kModuleLength - 1] = (Byte) (kProgramVersion >> 8);
    outModule[kModuleLength]     = (Byte) outModule.size();

    crc = CassoLinkCodec::ComputeCrc (std::span<const Byte> (outModule).subspan (kModuleCrcStart));

    outModule[kModuleCrcOffset]     = (Byte) (crc & 0xFF);
    outModule[kModuleCrcOffset + 1] = (Byte) (crc >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetThirteenSectorModuleSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetThirteenSectorModuleSource()
{
    return std::string (reinterpret_cast<const char *> (s_kThirteenSectorModuleSource), sizeof (s_kThirteenSectorModuleSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildThirteenSectorModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildThirteenSectorModule (std::vector<Byte> & outModule, LinkChip chip, bool faults)
{
    std::string  errors;



    return AssembleModule (GetThirteenSectorModuleSource(), outModule, errors, chip, faults);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetVirtualDriveSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetVirtualDriveSource()
{
    return std::string (reinterpret_cast<const char *> (s_kVirtualDriveSource), sizeof (s_kVirtualDriveSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetSosInterpSource
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetSosInterpSource()
{
    return std::string (reinterpret_cast<const char *> (s_kSosInterpSource), sizeof (s_kSosInterpSource) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildSosInterpreter
//
//  The header is the label "SOS NTRP", no optional header, the loading
//  address and the code's length. The code is the SOS build, the control
//  byte after it, and zeros over the buffers, so SOS loads it all and the
//  memory is the interpreter's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildSosInterpreter (Byte speedControl, std::vector<Byte> & outFile, std::string & outErrors)
{
    static constexpr char  kLabel[]  = "SOS NTRP";
    static constexpr Byte  kRateMask = 0x0F;
    HRESULT                hr        = S_OK;
    std::vector<Byte>      program;
    Word                   length    = kSosEnd - kSosAddress;
    bool                   rated     = (speedControl & kRateMask) != 0;



    outFile.clear();
    outErrors.clear();

    CBREx (rated, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    hr = AssembleProgram (program, outErrors, LinkChip::Sos);
    CHR (hr);

    outFile.assign (kLabel, kLabel + sizeof (kLabel) - 1);
    outFile.insert (outFile.end(), { 0, 0,
                                     (Byte) (kSosAddress & 0xFF), (Byte) (kSosAddress >> 8),
                                     (Byte) (length & 0xFF),      (Byte) (length >> 8) });
    outFile.insert (outFile.end(), program.begin(), program.end());
    outFile.push_back (speedControl);
    outFile.resize (outFile.size() + length - program.size() - 1, 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::BuildVirtualDrive
//
//  The driver is assembled a page apart, and a byte that differs is a high
//  byte of one of its addresses, which the installer relocates.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::BuildVirtualDrive (Byte speedControl, std::vector<Byte> & outFile, std::string & outErrors)
{
    HRESULT                                 hr      = S_OK;
    std::string                             source  = GetVirtualDriveSource();
    std::vector<Byte>                       driver;
    std::vector<Byte>                       raised;
    std::vector<Byte>                       installer;
    std::vector<Byte>                       relocations;
    std::unordered_map<std::string, Word>   symbols;
    bool                                    sized   = false;
    bool                                    fits    = false;
    bool                                    shifted = true;
    Word                                    slot    = 0;



    outFile.clear();
    outErrors.clear();

    hr = Assemble (std::format ("DRIVER = 1\nDRVORG = ${:04X}\n", kDriverAddress) + source, kDriverAddress, LinkChip::Acia, driver, outErrors, false, &symbols);
    CHR (hr);

    hr = Assemble (std::format ("DRIVER = 1\nDRVORG = ${:04X}\n", kDriverAddress + 0x100) + source, (Word) (kDriverAddress + 0x100), LinkChip::Acia, raised, outErrors);
    CHR (hr);

    sized = driver.size() == raised.size();
    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    fits = driver.size() <= kDriverLimit && symbols.contains ("sltx");
    outErrors = fits ? "" : std::format ("The driver is {} bytes, past its {}, or has no sltx.", driver.size(), kDriverLimit);
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (size_t at = 0; at < driver.size(); at++)
    {
        if (driver[at] != raised[at])
        {
            shifted = shifted && raised[at] == (Byte) (driver[at] + 1);
            relocations.push_back ((Byte) (at & 0xFF));
            relocations.push_back ((Byte) (at >> 8));
        }
    }

    outErrors = shifted ? "" : "A byte of the driver assembled a page higher isn't one more.";
    CBREx (shifted, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    slot = (Word) (symbols["sltx"] - kDriverAddress);

    hr = Assemble (std::format ("DRVLEN = {}\nRELOCN = {}\nSLTXOFF = {}\nSPEED = ${:02X}\n", driver.size(), relocations.size() / 2, slot, speedControl) + source,
                   kInstallerAddress, LinkChip::Acia, installer, outErrors);
    CHR (hr);

    outFile = installer;
    outFile.insert (outFile.end(), driver.begin(), driver.end());
    outFile.insert (outFile.end(), relocations.begin(), relocations.end());

Error:
    return hr;
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

    //  The last byte before the shim is the tape's balance.
    fits = outBytes.size() < (size_t) (kShimAddress - kSlotFinderAddress);
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
    HRESULT            hr      = S_OK;
    std::vector<Byte>  shim;
    std::string        errors;
    Byte               balance = kTapeChecksumSeed;



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

    if (options.chip == LinkChip::Acia)
    {
        for (Byte value : outImage)
        {
            balance ^= value;
        }

        outImage[kShimAddress - kSlotFinderAddress - 1] = balance;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetSpeedControl
//
//  Rate 0 is the 6551's 16x external clock, 115,200 baud on the //c. Rate 1,
//  50 baud on a 6551, indicates 230,400 on a IIgs's port.
//
////////////////////////////////////////////////////////////////////////////////

bool LinkProgram::GetSpeedControl (DWORD baud, Byte & outControl)
{
    static constexpr std::pair<DWORD, Byte>  s_kRates[] =
    {
        { 230400, 0x01 }, { 115200, 0x00 }, { 19200, 0x0F }, { 9600, 0x0E }, { 4800, 0x0C }, { 2400, 0x0A }, { 1200, 0x08 },
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
//  LinkProgram::GetCommandLines
//
//  An assignment for each command of CassoLinkCodec::kCommands with an
//  assembler name, as CMDREADY = $D2.
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetCommandLines()
{
    std::string  lines;



    for (const CassoLinkCodec::Command & command : CassoLinkCodec::kCommands)
    {
        lines += (command.symbol != nullptr) ? std::format ("{} = ${:02X}\n", command.symbol, command.value) : std::string();
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::GetChipLines
//
//  W5100, CASSETTE or ACIA for an Apple II build. The SOS build is given
//  SOS, and the values it shares with the host: the Ready flag, the block
//  module's id and version, and its addresses.
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkProgram::GetChipLines (LinkChip chip)
{
    switch (chip)
    {
        case LinkChip::W5100:     return "W5100 = 1\n";
        case LinkChip::Cassette:  return "CASSETTE = 1\n";
        case LinkChip::Network:   return "NETWORK = 1\n";
        case LinkChip::Sos:       return std::format ("SOS = 1\nSOSFLAG = ${:02X}\nBLKMODID = {}\nBLKMODVER = {}\nSOSORG = ${:04X}\nBUFBASE = ${:04X}\n",
                                                      kSosReadyFlag, kBlockModuleId, kBlockModuleVersion, kSosAddress, kSosBufferBase);
        default:                  return "ACIA = 1\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::Assemble
//
//  The source starts with its own .org; the result must begin at `origin`,
//  so a source edited to load elsewhere fails here rather than on the Apple.
//  Each build is given its chip's lines ahead of the source (GetChipLines),
//  and the source selects each one's code with ifdef; a build with faults
//  is given FAULTS the same way.
//  The commands' numbers are given ahead of it too, from GetCommandLines.
//  An error's line is the source's, after the lines put ahead of it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::Assemble (const std::string & source, Word origin, LinkChip chip, std::vector<Byte> & outBytes, std::string & outErrors, bool faults, std::unordered_map<std::string, Word> * outSymbols)
{
    HRESULT          hr        = S_OK;
    static const Cpu s_cpu;
    Assembler        assembler (InstructionSetProvider (s_cpu.GetInstructionSet()));
    std::string      chipLine  = GetChipLines (chip);
    std::string      faultLine = faults ? "FAULTS = 1\n" : "";
    std::string      prefix    = std::format ("PROGVER = {}\nSHIMVER = {}\n", kProgramVersion, kShimVersion) + chipLine + faultLine + GetCommandLines();
    int              ahead     = (int) std::count (prefix.begin(), prefix.end(), '\n');
    AssemblyResult   result    = assembler.Assemble (prefix + source);
    bool             placed    = false;



    outBytes.clear();
    outErrors.clear();

    for (const AssemblyError & error : result.errors)
    {
        outErrors += std::format ("line {}: {}\n", error.lineNumber - ahead, error.message);
    }

    CBREx (result.success, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    placed = result.startAddress == origin;
    CBREx (placed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outBytes = result.bytes;

    if (outSymbols != nullptr)
    {
        *outSymbols = result.symbols;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgram::AssembleModule
//
//  The program is assembled first for its symbols. The module's length in
//  its header must be its size, and the CRC covers the bytes after the CRC.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkProgram::AssembleModule (const std::string & source, std::vector<Byte> & outBytes, std::string & outErrors, LinkChip chip, bool faults)
{
    HRESULT                                 hr      = S_OK;
    std::vector<Byte>                       program;
    std::unordered_map<std::string, Word>   symbols;
    std::vector<std::string>                exported;
    std::string                             imports;
    Word                                    crc     = 0;
    bool                                    fits    = false;
    bool                                    sized   = false;



    outBytes.clear();

    hr = Assemble (GetProgramSource(), kProgramAddress, chip, program, outErrors, faults, &symbols);
    CHR (hr);

    for (const auto & [name, value] : symbols)
    {
        bool  plain = !name.empty() && (isalpha ((unsigned char) name[0]) || name[0] == '_')
                   && std::all_of (name.begin(), name.end(), [] (char c) { return isalnum ((unsigned char) c) || c == '_'; });

        if (plain && name != "PROGVER" && name != "SHIMVER")
        {
            exported.push_back (name);
        }
    }

    std::sort (exported.begin(), exported.end());

    for (const std::string & name : exported)
    {
        imports += std::format ("p_{} = ${:04X}\n", name, symbols[name]);
    }

    hr = Assemble (imports + source, kModuleAddress, chip, outBytes, outErrors, faults);
    CHR (hr);

    fits  = outBytes.size() > kModuleCrcStart && outBytes.size() <= kModuleLimit;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    sized = (size_t) (outBytes[kModuleLength] | (outBytes[kModuleLength + 1] << 8)) == outBytes.size();
    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    crc = CassoLinkCodec::ComputeCrc (std::span<const Byte> (outBytes).subspan (kModuleCrcStart));

    outBytes[kModuleCrcOffset]     = (Byte) (crc & 0xFF);
    outBytes[kModuleCrcOffset + 1] = (Byte) (crc >> 8);

Error:
    return hr;
}
