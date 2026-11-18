#include "Pch.h"

#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosStartup.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::IsSosLoader
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosStartup::IsSosLoader (std::span<const Byte> block)
{
    constexpr Byte    kJump       = 0x4C;
    constexpr size_t  kTextAt     = 3;
    constexpr char    kText[]     = "SOS BOOT";
    constexpr size_t  kTextLength = sizeof (kText) - 1;



    return block.size() >= kTextAt + kTextLength
        && block[0] == kJump
        && std::equal (kText, kText + kTextLength, block.begin() + kTextAt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::IsAppleIILoader
//
//  A block of zeros, as a volume ProDOS left unbootable has, isn't one.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosStartup::IsAppleIILoader (std::span<const Byte> block)
{
    constexpr Byte  kSectorCount = 0x01;



    return !block.empty() && block[0] == kSectorCount
        && std::any_of (block.begin() + 1, block.end(), [] (Byte b) { return b != 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::HasFile
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosStartup::HasFile (const vector<FileEntry> & root, const char * name)
{
    return std::any_of (root.begin(), root.end(), [name] (const FileEntry & entry) { return !entry.isDirectory && _stricmp (entry.name.c_str(), name) == 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::Detect
//
////////////////////////////////////////////////////////////////////////////////

StartupSystems ProDosStartup::Detect (std::span<const Byte> block0, std::span<const Byte> block1, const vector<FileEntry> & root)
{
    StartupSystems  startup;
    bool            hasSos  = true;



    for (const char * name : kpszSosFiles)
    {
        hasSos = hasSos && HasFile (root, name);
    }

    startup.appleII  = IsAppleIILoader (block0) && HasFile (root, "PRODOS");
    startup.appleIII = (IsSosLoader (block0) || IsSosLoader (block1)) && hasSos;

    return startup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::DescribeListingNote
//
////////////////////////////////////////////////////////////////////////////////

std::string ProDosStartup::DescribeListingNote (const StartupSystems & startup, bool hasProDosFile)
{
    if (startup.appleII && startup.appleIII)
    {
        return "Starts an Apple II with ProDOS and an Apple /// with SOS";
    }

    if (startup.appleIII)
    {
        return hasProDosFile ? std::string ("Starts an Apple /// with SOS")
                             : std::string ("Starts an Apple /// with SOS. Not bootable on an Apple II: no PRODOS file on the volume");
    }

    return hasProDosFile ? std::string() : std::string ("Not bootable: no PRODOS file on the volume");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProDosStartup::GetLabel (const StartupSystems & startup)
{
    if (startup.appleII && startup.appleIII)
    {
        return L"Apple II and Apple ///";
    }

    if (startup.appleII)
    {
        return L"Apple II";
    }

    return startup.appleIII ? L"Apple ///" : L"None";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

void ProDosStartup::ReadBlock (const vector<Byte> & sectors, int block, std::array<Byte, kBlockBytes> & outBlock)
{
    size_t  i = 0;



    for (i = 0; i < kBlockBytes; i++)
    {
        outBlock[i] = sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), block, i)];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::WriteBlock
//
////////////////////////////////////////////////////////////////////////////////

void ProDosStartup::WriteBlock (vector<Byte> & ioSectors, int block, const std::array<Byte, kBlockBytes> & bytes)
{
    size_t  i = 0;



    for (i = 0; i < kBlockBytes; i++)
    {
        ioSectors[ProDosSkeleton::GetBlockByteOffset (ioSectors.size(), block, i)] = bytes[i];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup::InstallSos
//
//  The files are written first, each through the volume's verified write,
//  and the loader last, so on a failure `ioSectors` isn't changed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosStartup::InstallSos (vector<Byte> & ioSectors, const vector<Byte> & sosDisk, std::string & outError)
{
    HRESULT                                hr      = S_OK;
    std::array<Byte, kBlockBytes>          first   = {};
    std::array<Byte, kBlockBytes>          second  = {};
    std::array<Byte, kBlockBytes>          target  = {};
    const std::array<Byte, kBlockBytes>  * loader  = nullptr;
    vector<Byte>                           result  = ioSectors;
    vector<Byte>                           next;
    FilePayload                            payload;
    bool                                   isSized = ProDosSkeleton::IsValidVolumeSize (sosDisk.size()) && ProDosSkeleton::IsValidVolumeSize (ioSectors.size());
    int                                    block   = 0;



    CBRF (isSized, outError = "the SOS disk isn't a ProDOS volume");

    ReadBlock (sosDisk, 0, first);
    ReadBlock (sosDisk, 1, second);

    loader = IsSosLoader (first) ? &first : IsSosLoader (second) ? &second : nullptr;
    CBRF (loader != nullptr, outError = "the SOS disk has no SOS loader in block 0 or 1");

    for (const char * name : kpszSosFiles)
    {
        hr = ProDosVolume (sosDisk).Read (FilePath::Parse (name), payload);
        CHRF (hr, outError = std::string ("the SOS disk has no ") + name + " file");

        hr = ProDosVolume (result).Write (FilePath::Parse (name), payload, next);
        CHRF (hr, outError = std::string ("there is no room for ") + name);

        result = std::move (next);
    }

    ReadBlock (result, 0, target);

    block = IsAppleIILoader (target) ? 1 : 0;

    WriteBlock (result, block, *loader);

    ioSectors = std::move (result);

Error:
    return hr;
}
