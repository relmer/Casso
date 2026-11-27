#include "Pch.h"

#include "CassoExplorer/Actions/ActionsAppleDrive.h"
#include "CassoExplorer/Actions/ActionsCopy.h"
#include "CassoExplorer/Actions/ActionsLinkDisk.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoLinkShimBuilder.h"
#include "CassoLink/CassoLinkProDosDisk.h"
#include "CassoLink/CassoLinkSosDisk.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/LinkImageSource.h"
#include "Core/TextEncoding.h"
#include "Devices/Tape/WavCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::ActionsLinkDisk
//
////////////////////////////////////////////////////////////////////////////////

ActionsLinkDisk::ActionsLinkDisk (CassoExplorerBrowser & browser)
    : m_browser (browser)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::SaveLinkToDisk
//
//  The link's boot disk, made in memory and written to the open Apple drive
//  over the link: its boot sector loads the shim at the link's speed.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsLinkDisk::SaveLinkToDisk (const std::string & link, int drive)
{
    ActionsTypes::Outcome               outcome;
    std::vector<Byte>                   disk;
    std::shared_ptr<ILinkImageSource>   image;
    HRESULT                             hr      = LinkProgram::BuildUniversalBootDisk (CassoLinkShimBuilder::MakeShimOptions (CassoLinkShimBuilder::GetPcName(), FindLinkSettings (link)), disk);



    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink disk could not be made.");
        return outcome;
    }

    image = std::make_shared<MemoryImageSource> (std::format ("casso-link-{}.dsk", drive), std::move (disk));

    ActionsAppleDrive::SubmitWrite (*m_browser.GetLinks(), link, drive, [drive, image] (CassoLinkSession & session) { return session.WriteDisk (drive, image); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::SaveLinkDiskImage
//
//  The same boot disk as an image file on the PC, for an emulator or to
//  write to a disk some other way. A Wi-Fi modem link's disk dials this PC
//  before its first Ready.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsLinkDisk::SaveLinkDiskImage (const std::wstring & imagePath, const CassoLinkSettings & settings)
{
    ActionsTypes::Outcome  outcome;
    std::vector<Byte>      disk;
    HRESULT                hr      = LinkProgram::BuildUniversalBootDisk (CassoLinkShimBuilder::MakeShimOptions (CassoLinkShimBuilder::GetPcName(), settings), disk);



    if (SUCCEEDED (hr))
    {
        hr = m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (imagePath), disk);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink disk could not be saved.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::SaveLinkProDosDisk
//
//  The CassoLink ProDOS disk, from the user's ProDOS disk at usersDisk, for
//  the link's speed, saved as an image file on the PC.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsLinkDisk::SaveLinkProDosDisk (const std::wstring & usersDisk, const std::wstring & imagePath, const CassoLinkSettings & settings)
{
    std::vector<Byte>      disk;
    ActionsTypes::Outcome  outcome = MakeLinkProDosDisk (usersDisk, settings, disk);
    HRESULT                hr      = S_OK;



    if (!outcome.Succeeded())
    {
        return outcome;
    }

    hr = m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (imagePath), disk);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink ProDOS disk could not be saved.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::MakeLinkProDosDisk
//
//  The CassoLink ProDOS disk's bytes, from the user's ProDOS disk, for the
//  link's speed.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsLinkDisk::MakeLinkProDosDisk (const std::wstring & usersDisk, const CassoLinkSettings & settings, std::vector<Byte> & outDisk)
{
    ActionsTypes::Outcome  outcome;
    std::string            source  = TextEncoding::WideToNarrow (usersDisk);
    std::vector<Byte>      users;
    HRESULT                hr      = m_browser.GetOperations().GetFileIo().ReadAllBytes (source, users);



    outDisk.clear();

    if (SUCCEEDED (hr))
    {
        hr = CassoLinkProDosDisk::Make (users, source, settings.baud, outDisk);
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink ProDOS disk could not be made. The disk chosen must be a 140K ProDOS disk with PRODOS and BASIC.SYSTEM.");
    }
    else if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink ProDOS disk could not be made.");
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::SaveLinkProDosToDisk
//
//  The CassoLink ProDOS disk, made in memory for the link's speed, written
//  to the linked Apple's drive.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsLinkDisk::SaveLinkProDosToDisk (const std::string & link, int drive, const std::wstring & usersDisk)
{
    std::vector<Byte>                   disk;
    ActionsTypes::Outcome               outcome = MakeLinkProDosDisk (usersDisk, FindLinkSettings (link), disk);
    std::shared_ptr<ILinkImageSource>   image;



    if (!outcome.Succeeded())
    {
        return outcome;
    }

    image = std::make_shared<MemoryImageSource> (std::format ("casso-link-prodos-{}.dsk", drive), std::move (disk));

    ActionsAppleDrive::SubmitWrite (*m_browser.GetLinks(), link, drive, [drive, image] (CassoLinkSession & session) { return session.WriteDisk (drive, image); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::FindLinkSettings
//
//  The connection's settings by its name; the defaults when there's none.
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkSettings ActionsLinkDisk::FindLinkSettings (const std::string & link) const
{
    CassoLinkSettings  found;



    for (const CassoLinkSettings & settings : m_browser.GetLinks()->GetLinks())
    {
        if (settings.name == link)
        {
            found = settings;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::SaveLinkSosDisk
//
//  The CassoLink SOS disk, from the user's SOS disk at usersDisk, for the
//  link's speed, saved as an image file on the PC.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsLinkDisk::SaveLinkSosDisk (const std::wstring & usersDisk, const std::wstring & imagePath, const CassoLinkSettings & settings)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io      = m_browser.GetOperations().GetFileIo();
    std::string            source  = TextEncoding::WideToNarrow (usersDisk);
    std::vector<Byte>      users;
    std::vector<Byte>      disk;
    std::string            reason;
    HRESULT                hr      = io.ReadAllBytes (source, users);



    if (SUCCEEDED (hr))
    {
        hr = CassoLinkSosDisk::Make (users, source, settings.baud, disk, reason);
    }

    if (SUCCEEDED (hr))
    {
        hr = io.WriteAllBytes (TextEncoding::WideToNarrow (imagePath), disk);
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink SOS disk could not be made. The Apple ///'s serial port runs at 19,200 baud or slower; choose a slower speed for this connection.");
    }
    else if (FAILED (hr) && !reason.empty())
    {
        ActionsCopy::AppendMessage (outcome, hr, std::format (L"The CassoLink SOS disk could not be made: {}. Apple's System Demo disk has SOS.KERNEL, and a SOS.DRIVER with .RS232.",
                                                              TextEncoding::NarrowToWide (reason)));
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink SOS disk could not be made. The disk chosen must be a 140K SOS disk with SOS.KERNEL and SOS.DRIVER.");
    }
    else if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink SOS disk could not be saved.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk::SaveCassetteBootstrap
//
//  The cassette bootstrap for a link as a WAV file on the PC, to send into a
//  ][+ or //e's cassette input from another device, or to insert in Casso's
//  tape deck. outCommand is the monitor command that reads it.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsLinkDisk::SaveCassetteBootstrap (const std::wstring & wavPath, const CassoLinkSettings & settings, std::string & outCommand)
{
    ActionsTypes::Outcome  outcome;
    std::vector<Byte>      wav;
    TapeAudio              audio;
    HRESULT                hr      = S_OK;



    //  A cassette link's tape is the program itself; a serial link's is the
    //  shim, which loads the program over the card.
    if (settings.transport == LinkTransport::Cassette)
    {
        hr = LinkAudioBootstrap::BuildCassetteLinkAudio (audio, outCommand);

        if (SUCCEEDED (hr))
        {
            WavCodec::Encode (audio, wav);
        }
    }
    else
    {
        hr = LinkAudioBootstrap::BuildWav (CassoLinkShimBuilder::MakeShimOptions (CassoLinkShimBuilder::GetPcName(), settings), wav, outCommand);
    }

    if (SUCCEEDED (hr))
    {
        hr = m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (wavPath), wav);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The cassette bootstrap could not be saved.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





