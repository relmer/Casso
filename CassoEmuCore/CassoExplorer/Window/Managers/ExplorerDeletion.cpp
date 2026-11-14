#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerDeletion.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/DiskImagePictures.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Seams/IDiskPictures.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::ExplorerDeletion
//
////////////////////////////////////////////////////////////////////////////////

ExplorerDeletion::ExplorerDeletion (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::DeleteSelectionPermanently
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDeletion::DeleteSelectionPermanently()
{
    std::vector<std::wstring>  ids;
    HRESULT                    hr = S_OK;



    if (!m_context.browser.TryGetSelectedShellIds (ids))
    {
        return;
    }

    hr = m_context.shellVerbs.DeletePermanently (m_context.window.GetHwnd(), ids);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_context.manifestSync.NoteDeletes (ids);
    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::RecycleHostFiles
//
//  To the Recycle Bin rather than gone, so a move whose files a disk image
//  has in another form can be undone. Through the shell's delete, so a file
//  too large for the bin is deleted only once the user confirms. A disk
//  image's thumbnail is kept: the image's bytes are in the image it was
//  moved into, and a copy out of it finds the thumbnail by its content.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerDeletion::RecycleHostFiles (const std::vector<std::wstring> & paths)
{
    HRESULT  hr = m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), paths);



    return (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED)) ? S_OK : hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::DeleteSelected
//
//  Delete on the selection: on the PC through the shell, to the Recycle Bin,
//  and inside a disk image or on an Apple drive by the volume, after a
//  confirmation that lists what goes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDeletion::DeleteSelected()
{
    if (m_context.browser.IsVolumeLocation())
    {
        DeleteFromVolume();
    }
    else
    {
        DeleteHostSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::DeleteHostSelection
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDeletion::DeleteHostSelection()
{
    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  hostPaths;
    std::vector<std::string>   thumbnails;



    //  Windows asks, and the Recycle Bin keeps it. A file inside a
    //  zip has no file of its own to recycle, so it is deleted for
    //  good after Windows' confirmation, as in File Explorer.
    if (m_context.preview.IsArchiveLocation())
    {
        DeleteSelectionPermanently();
        return;
    }

    //  A search's file goes to the Recycle Bin, with an undo, as it
    //  does from its own folder. An item a shell folder such as Home
    //  lists may be a recent or a cloud file with no path of its
    //  own; its own Delete command removes it, as File Explorer's does.
    if (m_context.browser.AreSelectedRowsSearchFiles())
    {
        m_context.browser.GetSelectedHostPaths (hostPaths);
        thumbnails = GetThumbnailKeys (hostPaths);
        hr         = m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), hostPaths);
    }
    else if (m_context.browser.TryGetSelectedShellIds (hostPaths))
    {
        //  Before an A2Pico card's drive image is deleted, a message lists
        //  its drive, since deleting it empties the drive.
        if (m_context.browser.GetLocation().kind == Location::Kind::HostFolder && !ConfirmA2PicoDrives (hostPaths))
        {
            return;
        }

        if (m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
        {
            thumbnails = GetThumbnailKeys (hostPaths);
        }

        hr = (m_context.browser.GetLocation().kind == Location::Kind::ShellFolder)
           ? m_context.shellVerbs.RunItemVerb (m_context.window.GetHwnd(), hostPaths, "delete")
           : m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), hostPaths);
    }

    if (FAILED (hr) && hr != HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        m_context.window.ShowMessage (L"The selected items could not be deleted.", MB_ICONWARNING);
    }

    if (SUCCEEDED (hr) && !hostPaths.empty() && (m_context.browser.GetLocation().kind == Location::Kind::HostFolder || m_context.browser.AreSelectedRowsSearchFiles()))
    {
        m_context.undo.Push (ExplorerUndoStep { ExplorerUndoStep::Kind::Recycle, Location(), hostPaths });
        m_context.manifestSync.NoteDeletes (hostPaths);

        for (const std::wstring & path : hostPaths)
        {
            m_context.actions.GetA2retroNet().NoteDelete (path);
        }

        RemoveThumbnails (hostPaths, thumbnails);
    }

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::GetThumbnailKeys
//
//  Read before the delete: a file in the Recycle Bin can't be read for its
//  hash.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ExplorerDeletion::GetThumbnailKeys (const std::vector<std::wstring> & paths)
{
    return (m_context.app.pictures != nullptr) ? m_context.app.pictures->GetKeys (paths) : std::vector<std::string>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::RemoveThumbnails
//
//  Only once the files are gone: a delete canceled in Windows'
//  confirmation removes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDeletion::RemoveThumbnails (const std::vector<std::wstring> & paths, const std::vector<std::string> & keys)
{
    bool  areGone = std::none_of (paths.begin(), paths.end(), [] (const std::wstring & path) { std::error_code  failed; return std::filesystem::exists (path, failed); });



    if (areGone && m_context.app.pictures != nullptr)
    {
        m_context.app.pictures->RemoveDeleted (keys);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::ClearThumbnails
//
//  With thumbnails the user set, the confirmation has their count and a
//  button that removes only the thumbnails made by booting.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDeletion::ClearThumbnails (HWND owner)
{
    HRESULT       hr       = S_OK;
    int           userSet  = m_context.app.pictures->CountUserSet();
    std::wstring  question = DiskImagePictures::GetClearQuestion (userSet);
    int           answer   = IDCANCEL;



    if (userSet > 0)
    {
        answer = DxuiMessageBox (owner, m_context.theme, question.c_str(), L"Clear thumbnails", MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON2,
                                 { L"Remove all", L"Remove booted only", L"Cancel" }, 0);
    }
    else
    {
        answer = DxuiMessageBox (owner, m_context.theme, question.c_str(), L"Clear thumbnails", MB_OKCANCEL | MB_ICONWARNING, { L"Clear", L"Cancel" });
    }

    if (answer == IDYES || answer == IDNO || answer == IDOK)
    {
        hr = m_context.app.pictures->Clear (answer == IDYES);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::ConfirmA2PicoDrives
//
//  True when none of the files is an A2Pico card's drive image, or the user
//  goes on with the delete.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDeletion::ConfirmA2PicoDrives (const std::vector<std::wstring> & paths)
{
    std::wstring  drives = m_context.actions.GetA2retroNet().DescribeDrives (paths);
    int           choice = IDYES;



    if (!drives.empty())
    {
        choice = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, drives.c_str(), L"Delete A2Pico drive images", MB_YESNO | MB_DEFBUTTON2, { L"Delete", L"Cancel" });
    }

    return choice == IDYES;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion::DeleteFromVolume
//
//  The row now in the deleted rows' place is selected afterward, as after a
//  delete in a folder.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDeletion::DeleteFromVolume()
{
    int                            answer  = 0;
    CassoExplorerActions::Outcome  outcome;
    std::vector<FileEntry>         entries;
    std::vector<std::wstring>      plan    = m_context.actions.DescribeDeletePlan();
    size_t                         count   = m_context.browser.GetSelectedRows().size();
    std::wstring                   link;
    int                            slot    = 0;
    int                            drive   = 0;
    std::wstring                   where   = TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, link, slot, drive)
                                             ? std::format (L"the disk in drive {}", drive)
                                             : std::filesystem::path (m_context.browser.GetLocation().path).filename().wstring();
    std::wstring                   what    = (count == 1) ? m_context.browser.GetRows()[(size_t) m_context.browser.GetSelectedRows()[0]].name
                                                          : std::format (L"these {} items", count);
    std::wstring                   message = std::format (L"Delete {} from {}?", what, where);



    //  A folder goes with everything below it, which the list cannot
    //  show, so the plan's own rows and totals go into the question.
    for (const std::wstring & line : plan)
    {
        message += L"\n" + line;
    }

    //  The buttons are labeled with their actions, as File Explorer's Delete File
    //  dialog's are.
    answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, message.c_str(), L"Delete", MB_OKCANCEL | MB_ICONWARNING, { L"Delete", L"Cancel" });

    if (answer == IDOK)
    {
        ExplorerUndoStep  step           { ExplorerUndoStep::Kind::ImageDelete, m_context.browser.GetLocation() };
        wchar_t           temp[MAX_PATH] = {};
        std::vector<int>  selected       = m_context.browser.GetSelectedRows();
        int               first          = -1;

        //  An AppleSingle copy keeps each file's bytes, type and
        //  address, so a put brings it back as it was. The copy holds
        //  files only, so a deletion that takes a folder is not kept.
        m_context.browser.GetSelectedEntries (entries);

        if (!m_context.browser.IsAppleDriveLocation() &&
            std::none_of (entries.begin(), entries.end(), [] (const FileEntry & entry) { return entry.isDirectory; }) &&
            GetTempPathW (MAX_PATH, temp) != 0)
        {
            std::error_code  ignore;

            step.savedDir = std::format (L"{}CassoExplorerUndo\\{}", temp, GetTickCount64());
            std::filesystem::create_directories (step.savedDir, ignore);
        }

        if (!step.savedDir.empty() && !m_context.actions.GetSelected (step.savedDir, HostFileNaming::Style::AppleSingle).Succeeded())
        {
            step.savedDir.clear();
        }

        first   = selected.empty() ? -1 : *std::min_element (selected.begin(), selected.end());
        outcome = m_context.actions.DeleteSelected();

        if (outcome.Succeeded() && !step.savedDir.empty())
        {
            m_context.undo.Push (std::move (step));
        }

        //  The row now in the deleted rows' place is selected, as after
        //  a delete in a folder.
        if (outcome.Succeeded() && m_context.browser.GetSelectedRows().empty())
        {
            m_context.browser.SelectRowAt (first);
        }

        m_context.commandManager.ReportOutcome (outcome, L"Delete");
        m_context.listPane.FillList();
    }
}
