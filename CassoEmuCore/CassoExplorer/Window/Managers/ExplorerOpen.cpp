#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Seams/IShellItemVerbs.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
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
//  ExplorerOpen::OpenPhysicalCard
//
//  Listing the drives doesn't open them, so the administrator prompt is
//  raised only for the drive chosen.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::OpenPhysicalCard()
{
    std::vector<PhysicalDrive>        drives;
    std::vector<DxuiPopupMenuItem>    items;
    POINT                             screen = {};
    HRESULT                           hr     = S_OK;
    BOOL                              got    = FALSE;



    if (m_context.app.cards == nullptr)
    {
        return;
    }

    hr = m_context.app.cards->ListRemovableDrives (drives);

    if (FAILED (hr) || drives.empty())
    {
        m_context.window.ShowMessage (L"No storage cards were found.", MB_ICONINFORMATION);
        return;
    }

    for (const PhysicalDrive & drive : drives)
    {
        std::wstring  label = PhysicalCards::FormatDrive (drive);

        m_context.menus.AddMenuCommand (items, label.c_str(), [this, drive]() { OpenCard (drive); });
    }

    got = GetCursorPos (&screen);
    IGNORE_RETURN_VALUE (got, TRUE);

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), screen.x, screen.y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::OpenCard
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::OpenCard (const PhysicalDrive & drive)
{
    HRESULT  hr = S_OK;



    hr = m_context.app.cards->Open (drive.number, m_context.window.GetHwnd());

    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        return;
    }

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"Unable to open " + PhysicalCards::FormatDrive (drive) + L".", MB_ICONWARNING);
        return;
    }

    m_context.browser.NavigateToLocation (Location::MakeDiskImage (TextEncoding::Utf8ToWide (PhysicalCards::MakeCardPath (drive.number))));
    m_context.listPane.FillList();
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