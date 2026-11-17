#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "CassoExplorer/Model/CassoLinkShimBuilder.h"
#include "Core/NumberFormat.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectionText::DescribeJobs
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnectionText::DescribeJobs (const std::vector<std::shared_ptr<LinkOperation>> & jobs)
{
    std::wstring  text;
    std::wstring  what;



    if (jobs.empty())
    {
        return text;
    }

    switch (jobs[0]->kind)
    {
        case LinkOperationKind::ReadFile:        what = L"Reading a file";    break;
        case LinkOperationKind::WriteFile:       what = L"Writing a file";    break;
        case LinkOperationKind::Delete:          what = L"Deleting";          break;
        case LinkOperationKind::Rename:          what = L"Renaming";          break;
        case LinkOperationKind::SetAttributes:   what = L"Changing";          break;
        case LinkOperationKind::MakeDirectory:   what = L"Making a folder";   break;
        case LinkOperationKind::RemoveDirectory: what = L"Deleting a folder"; break;
        case LinkOperationKind::ReadDisk:        what = L"Reading the disk";  break;
        case LinkOperationKind::WriteDisk:       what = L"Writing the disk";  break;
        case LinkOperationKind::CheckLinkDisk:   what = L"Checking the disk"; break;
        case LinkOperationKind::FullSend:        what = L"Full Send";         break;
        case LinkOperationKind::Krunch:          what = L"Krunching";         break;
        case LinkOperationKind::CaptureDisk:     what = L"Capturing";         break;
        default:                                 what = L"Working";           break;
    }

    if (jobs[0]->kind == LinkOperationKind::FullSend && jobs[0]->fullSendWaiting)
    {
        text = L"Full Send: waiting for a disk on the Apple";
    }
    else if (jobs[0]->kind == LinkOperationKind::FullSend && jobs[0]->fullSendChecking)
    {
        text = std::format (L"Full Send: checking the disk in drive {}", jobs[0]->drive);
    }
    else if (jobs[0]->kind == LinkOperationKind::FullSend && !jobs[0]->GetDetail().empty())
    {
        text = std::format (L"Full Send: the disk in drive {} is likely copy-protected; using raw capture mode: {}%, {}", jobs[0]->drive, jobs[0]->progress.load(),
                            TextEncoding::NarrowToWide (jobs[0]->GetDetail()));
    }
    else if (jobs[0]->kind == LinkOperationKind::FullSend)
    {
        text = std::format (L"Full Send: sending the disk in drive {}: {}%", jobs[0]->drive, jobs[0]->progress.load());
    }
    else if (jobs[0]->kind == LinkOperationKind::CaptureDisk && !jobs[0]->GetDetail().empty())
    {
        text = std::format (L"Capturing drive {}: {}%, {}", jobs[0]->drive, jobs[0]->progress.load(), TextEncoding::NarrowToWide (jobs[0]->GetDetail()));
    }
    else
    {
        text = std::format (L"{} in drive {}: {}%", what, jobs[0]->drive, jobs[0]->progress.load());
    }

    if (jobs.size() > 1)
    {
        text += std::format (L", {} more queued", jobs.size() - 1);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectionText::DescribeFailure
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnectionText::DescribeFailure (const LinkOperation & operation)
{
    std::wstring  what = operation.path.IsEmpty() ? std::format (L"Drive {}", operation.drive) : TextEncoding::NarrowToWide (operation.path.ToString());
    std::wstring  why;



    if (!operation.message.empty())
    {
        return TextEncoding::NarrowToWide (operation.message);
    }

    switch (operation.result)
    {
        case HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT):    why = std::format (L"the disk in drive {} is write-protected.", operation.drive); break;
        case HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND): why = L"the disk could not be read; it may be missing or unformatted.";              break;
        case HRESULT_FROM_WIN32 (ERROR_CRC):              why = L"a sector on the disk is damaged.";                                         break;
        case HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT):      why = L"a sector did not read back as written.";                                   break;
        case HRESULT_FROM_WIN32 (ERROR_DISK_FULL):        why = L"there isn't enough free space on the disk.";                                break;
        case HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE):   why = L"the file is too large for this disk.";                                     break;
        case HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE):      why = L"the volume directory is full. ProDOS has room for 51 entries there.";      break;
        case HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS):
        case HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS):   why = L"an entry with that name is already there.";                                break;
        case HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND):   why = L"it is not on the disk.";                                                   break;
        case HRESULT_FROM_WIN32 (ERROR_CANCELLED):        why = L"it was canceled.";                                                         break;
        case HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE):      why = L"the connection to the Apple closed.";                                      break;
        case HRESULT_FROM_WIN32 (ERROR_TIMEOUT):          why = L"the Apple stopped answering.";                                             break;
        case HRESULT_FROM_WIN32 (ERROR_INVALID_DATA):     why = L"replies from the Apple arrived damaged on every attempt. Check the cable and the port's speed."; break;
        default:                                          why = std::format (L"the operation failed (error 0x{:08X}).", (uint32_t) operation.result); break;
    }

    return what + L": " + why;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectionText::DescribeState
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnectionText::DescribeState (LinkState state, const CassoLinkSettings & settings, const std::wstring & pcName)
{
    std::wstring  name    = TextEncoding::NarrowToWide (settings.name);
    std::wstring  address = TextEncoding::NarrowToWide (settings.address);
    std::wstring  where   = NamesItsPort (name, address) ? name : std::format (L"{} at {}", name, address);
    std::wstring  port    = TextEncoding::NarrowToWide (settings.port);
    std::wstring  baud    = FormatBaud (settings.baud);
    std::wstring  text;
    bool          server  = settings.transport == LinkTransport::AdtProServer;



    switch ((server || settings.IsSearch()) ? LinkState::Opening : state)
    {
        case LinkState::Closed:          text = std::format (L"Click {} to connect.", name);                     break;
        case LinkState::Bootstrapping:   text = (settings.transport == LinkTransport::Cassette) ? std::format (L"Sending the program to {} as audio...", name)
                                                                                                : std::format (L"Typing the loader into {}...", name);     break;
        case LinkState::LoadingProgram:  text = std::format (L"Loading Casso's program into {}...", name);        break;
        case LinkState::Ready:
        case LinkState::Busy:            text = L"Connected.";                                                     break;

        case LinkState::Searching:
            text = std::format (L"Searching the COM ports for {} at {} and {} baud.\nBoot a CassoLink disk on the Apple, "
                                L"or choose Bootstrap the Apple from {}'s menu.", name, FormatBaud (settings.bootstrapBaud), baud, name);
            break;

        //  Opening, waiting for Ready, or no Apple yet: what to do on the Apple.
        default:
            switch (settings.transport)
            {
                case LinkTransport::Serial:
                    text = std::format (L"Searching for {} on {} at {} baud.\nBoot a CassoLink disk on the Apple, or choose Bootstrap the Apple from {}'s menu.",
                                        name, port.empty() ? std::wstring (L"the COM ports") : port, baud, name);
                    break;

                case LinkTransport::Emulated:
                    text = std::format (L"Waiting for {}.\nBoot a CassoLink disk in that Casso to connect.", NamesItsPort (name, address) ? name : std::format (L"{} on {}", name, address));
                    break;

                case LinkTransport::WifiModem:
                    text = std::format (L"Waiting for {} to dial {}.local:{}.\nBoot that Apple from a CassoLink disk saved from {}.",
                                        name, pcName, address.substr (address.rfind (L':') + 1), name);
                    break;

                case LinkTransport::Uthernet:
                    text = settings.IsSearch()
                         ? std::wstring (L"Each Apple on this PC's network that boots a CassoLink disk on an Uthernet II is listed "
                                         L"when its announcement arrives.")
                         : std::format (L"Connecting to {}.\nBoot that Apple from a CassoLink disk saved from {}.", where, name);
                    break;

                case LinkTransport::Cassette:
                    text = std::format (L"Waiting for the CassoLink program on {}.\nChoose Send the program from {}'s menu.", name, name);
                    break;

                case LinkTransport::AdtProServer:
                    text = (state == LinkState::Closed)
                         ? std::format (L"Click {} to serve ADTPro's client from {}.", name, settings.folder)
                         : std::format (L"Serving ADTPro's client from {}.\nStart ADTPro's client on the Apple, and choose {} in its configuration.",
                                        settings.folder, settings.IsComPort() ? port : address);
                    break;

                default:
                    text = std::format (L"Connecting to {}.\nStart the CassoLink program on that machine.", where);
                    break;
            }

            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectionText::NamesItsPort
//
//  The port's digits standing alone in the name: "Uthernet II at
//  127.0.0.1:6502" and "Casso on port 6502" say it, "My //c" and
//  "Port 65020" do not.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnectionText::NamesItsPort (const std::wstring & name, const std::wstring & address)
{
    size_t        colon = address.rfind (L':');
    std::wstring  port  = (colon == std::wstring::npos) ? std::wstring() : address.substr (colon + 1);
    size_t        at    = port.empty() ? std::wstring::npos : name.find (port);
    bool          found = false;



    while (!found && at != std::wstring::npos)
    {
        bool  digitBefore = at > 0 && iswdigit (name[at - 1]);
        bool  digitAfter  = at + port.size() < name.size() && iswdigit (name[at + port.size()]);

        found = !digitBefore && !digitAfter;
        at    = name.find (port, at + 1);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectionText::FormatBaud
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnectionText::FormatBaud (DWORD baud)
{
    return NumberFormat::FormatInteger (baud);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectionText::DescribeDial
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnectionText::DescribeDial (const std::wstring & pcName, const CassoLinkSettings & settings)
{
    CassoLinkSettings  modem = settings;



    modem.transport = LinkTransport::WifiModem;

    return std::format (L"Dial {} from the modem.", TextEncoding::NarrowToWide (CassoLinkShimBuilder::MakeDialCommand (pcName, modem)));
}
