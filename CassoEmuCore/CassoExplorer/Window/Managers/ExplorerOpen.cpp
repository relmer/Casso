#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Seams/IShellItemVerbs.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostDialogs.h"
#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/Fat32Format.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"
#include "CassoExplorer/CassoExplorerInitializeCardDialog.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::ExplorerOpen
//
////////////////////////////////////////////////////////////////////////////////

ExplorerOpen::ExplorerOpen (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::OpenSelectedEntries
//
//  Files inside an image open in the programs Windows has for them, from
//  copies in a folder of their own, as Explorer opens a file inside a zip.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::OpenSelectedEntries()
{
    std::vector<std::wstring>  paths;
    HRESULT                    hr    = CassoExplorerDragOut::WriteToTempFolder (m_context.browser, m_context.commandManager.GetNamingStyle(), paths);



    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"The files could not be read out of the image to open.", MB_ICONWARNING);
        return;
    }

    for (const std::wstring & path : paths)
    {
        hr = m_context.shellVerbs.Open (m_context.window.GetHwnd(), path);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::OpenEachSelected
//
//  Several items open each on its own, as Explorer's do: a folder or an image
//  in a tab of its own, a file in its program. All are read before the first
//  tab opens, since a new tab lists something else.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::OpenEachSelected()
{
    HRESULT                    hr      = S_OK;
    std::vector<Location>      places;
    std::vector<std::wstring>  files;
    Location                   place;
    std::wstring               path;
    bool                       written = m_context.browser.PrepareCassoLinkItems();



    for (int row : m_context.browser.GetSelectedRows())
    {
        if (m_context.browser.TryGetRowLocation (row, place))
        {
            places.push_back (place);
        }
        else if (written && !m_context.browser.IsImageLocation() && m_context.browser.TryGetRowPath (row, path))
        {
            files.push_back (path);
        }
    }

    for (const std::wstring & file : files)
    {
        hr = m_context.shellVerbs.Open (m_context.window.GetHwnd(), file);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (const Location & each : places)
    {
        m_context.browser.OpenInNewTab (each);
    }

    if (!places.empty())
    {
        m_context.listPane.FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::PickPartitionImage
//
//  A card has no folder on the PC to write beside, so the image goes where
//  the user picks.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerOpen::PickPartitionImage (const std::wstring & partition, std::wstring & outTarget)
{
    FileDialogSpec         spec;
    std::filesystem::path  target;
    bool                   picked = false;
    int                    drive  = 0;
    int                    index  = 0;
    HRESULT                hr     = S_OK;



    outTarget.clear();

    if (PhysicalCards::ParsePath (TextEncoding::WideToUtf8 (partition), drive, index) != PhysicalCards::PathKind::Partition)
    {
        return true;
    }

    spec.defaultExtension = L"po";
    spec.defaultFileName  = std::format (L"Physical drive {} partition {}.po", drive, index);
    spec.askToReplace     = true;
    spec.title            = std::format (L"Create image from partition {}", index);

    hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, target, picked);

    if (FAILED (hr) || !picked)
    {
        return false;
    }

    outTarget = target.wstring();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::SetCardWriteConfirmed
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::SetCardWriteConfirmed (bool confirmed)
{
    if (m_context.app.cards != nullptr)
    {
        m_context.app.cards->SetConfirmed (confirmed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::InitializeCard
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::InitializeCard (const std::wstring & cardPath)
{
    HRESULT                            hr        = S_OK;
    int                                drive     = 0;
    int                                partition = 0;
    PhysicalDrive                      found;
    std::wstring                       question;
    std::wstring                       contents;
    uint64_t                           layout    = 0;
    std::vector<InitializeCardChoice>  choices;
    std::vector<PhysicalDrive>         drives;
    std::vector<WindowsVolume>         volumes;
    std::vector<PartitionEntry>        apple;
    PartitionLayoutKind                kind      = PartitionLayoutKind::None;
    int                                count     = 0;



    if (m_context.app.cards == nullptr ||
        PhysicalCards::ParsePath (TextEncoding::WideToUtf8 (cardPath), drive, partition) != PhysicalCards::PathKind::Card)
    {
        return;
    }

    hr = m_context.app.cards->ListRemovableDrives (drives);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const PhysicalDrive & each : drives)
    {
        found = each.number == drive ? each : found;
    }

    if (found.number != drive)
    {
        m_context.window.ShowMessage (L"The storage card is no longer in its drive.", MB_ICONWARNING);
        return;
    }

    hr = m_context.app.cards->ListVolumes (drive, volumes);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  A card open already lists its Apple partitions too.
    if (m_context.app.cards->IsOpen (drive))
    {
        hr = m_context.app.cards->ListPartitions (drive, kind, apple);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    count = RawDeviceServer::CountInitialVolumes (found.bytes / IRawDevice::kBlockBytes);

    BuildInitializeChoices (found.bytes, count, choices);

    if (choices.empty())
    {
        m_context.window.ShowMessage (std::format (L"{} is too small for a CFFA adapter.", PhysicalCards::FormatDrive (found)), MB_ICONWARNING);
        return;
    }

    question = std::format (L"Initialize {} for use with an Apple CFFA adapter?", PhysicalCards::FormatDrive (found));
    contents = L"This card currently contains these partitions:";

    for (const WindowsVolume & volume : volumes)
    {
        std::wstring  where = volume.letter.empty() ? std::wstring (L"No drive letter") : volume.letter;
        std::wstring  label = volume.label.empty() ? std::wstring (L"no label") : volume.label;

        contents += std::format (L"\r\n    {} {} ({})", where, label, volume.fileSystem.empty() ? std::wstring (L"no Windows file system") : volume.fileSystem);
    }

    for (const PartitionEntry & entry : apple)
    {
        contents += L"\r\n    " + TextEncoding::NarrowToWide (PartitionAccess::GetLabel (entry, true));
    }

    if (volumes.empty() && apple.empty())
    {
        contents += L"\r\n    None that Windows or Casso recognizes";
    }

    if (!CassoExplorerInitializeCardDialog::Confirm (m_context.window.GetHwnd(), m_context.theme, question, choices, (size_t) std::min (count, 4) - 1, contents, layout))
    {
        return;
    }

    hr = m_context.app.cards->Initialize (drive, m_context.window.GetHwnd(), layout);

    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        return;
    }

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (std::format (L"Unable to initialize {}.", PhysicalCards::FormatDrive (found)), MB_ICONWARNING);
        return;
    }

    m_context.browser.NavigateToLocation (Location::MakeDiskImage (cardPath));
    m_context.listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::BuildInitializeChoices
//
//  Each count of CFFA volumes that fits, then FAT32 for the
//  CFFA3000's disk image files when the card is large enough for it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::BuildInitializeChoices (uint64_t bytes, int maxVolumes, std::vector<InitializeCardChoice> & outChoices)
{
    uint64_t  unused = 0;
    uint64_t  fit    = std::min<uint64_t> (bytes / kSliceBytes, RawDeviceServer::kInitialVolumes);



    outChoices.clear();

    for (int count = 1; count <= maxVolumes; count++)
    {
        InitializeCardChoice  choice;

        choice.layout = (uint64_t) count;
        choice.label  = count == 1 && bytes <= kSliceBytes ? std::wstring (L"1 volume of the whole card")
                                                           : std::format (L"{} volume{} of 32 MB", count, count == 1 ? L"" : L"s");

        //  CFFA 1.x maps four volumes, or eight with its 8-drive firmware 1.2,
        //  and its GS/OS driver reads the space after the fourth as 1 GB
        //  drives. CFFA 2.0 maps up to 13 with its 65C02 firmware.
        if (count <= 4)
        {
            choice.description = L"Fully compatible with ProDOS and GS/OS on every CFFA: 1.x, 2.0, and a CFFA3000 in its raw CF mode.";
        }
        else
        {
            choice.description = std::format (L"Requires {}, on a 65C02 Apple, with its partition count set to {}. "
                                              L"Older firmware can't read volumes 5 to {}. GS/OS with the CFFA 1.x driver reads volume 5 as a 1 GB drive and can't see volumes 6 to {}; "
                                              L"erasing that drive in GS/OS destroys volumes 5 to {}.",
                                              count <= 8 ? L"CFFA firmware 1.2 (8 drives) or 2.0" : L"CFFA firmware 2.0",
                                              count, count, count, count);
        }

        //  The space of the card outside the volumes. A CFFA volume is at most
        //  32 MB, so the rest of a large card is unusable from the Apple.
        unused              = bytes > (uint64_t) count * kSliceBytes ? bytes - (uint64_t) count * kSliceBytes : 0;
        choice.description += fit == 0 ? std::wstring (L"\r\n\r\nThe card is smaller than 32 MB, so it is one volume.")
                                       : std::format (L"\r\n\r\n{} volume{} of 32 MB fit on this card. A CFFA maps up to 13.", fit, fit == 1 ? L"" : L"s");
        choice.description += unused > 0 ? std::format (L" {} of the card is unused and unavailable to the Apple.", TextEncoding::NarrowToWide (PartitionAccess::FormatSize (unused)))
                                         : std::wstring (L" The whole card is used.");

        outChoices.push_back (std::move (choice));
    }
    if (Fat32Format::CanFormat (bytes / IRawDevice::kBlockBytes))

    {
        outChoices.push_back (InitializeCardChoice { L"FAT32 for a CFFA3000's disk image files",
                                                     L"Copy .po, .2mg and other disk images onto the card in Windows. A CFFA3000 reads FAT32 with an MBR, which Windows doesn't offer for a card over 32 GB.",
                                                     RawDeviceProtocol::kLayoutFat32 });
    }
}
