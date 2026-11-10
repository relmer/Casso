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
    HRESULT                     hr        = S_OK;
    int                         drive     = 0;
    int                         partition = 0;
    PhysicalDrive               found;
    std::wstring                question;
    std::vector<PhysicalDrive>  drives;
    std::vector<WindowsVolume>  volumes;



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

    question = std::format (L"Initialize {} for an Apple?\r\n\r\nEverything on the card is erased", PhysicalCards::FormatDrive (found));

    if (!volumes.empty())
    {
        question += L", including:\r\n";

        for (const WindowsVolume & volume : volumes)
        {
            std::wstring  where = volume.letter.empty() ? std::wstring (L"No drive letter") : volume.letter;
            std::wstring  label = volume.label.empty() ? std::wstring (L"no label") : volume.label;

            question += std::format (L"\r\n    {} {} ({})", where, label, volume.fileSystem.empty() ? std::wstring (L"no file system") : volume.fileSystem);
        }
    }
    else
    {
        question += L".";
    }

    question += L"\r\n\r\nThe card then has up to eight empty 32 MB ProDOS volumes, as a CFFA card does.";

    if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, question.c_str(), L"Initialize card for Apple",
                        MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Initialize", L"Cancel" }) != IDOK)
    {
        return;
    }

    hr = m_context.app.cards->Initialize (drive, m_context.window.GetHwnd());

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