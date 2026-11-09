#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerDeletion.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
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
//  too large for the bin is deleted only once the user confirms.
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
        hr = m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), hostPaths);
    }
    else if (m_context.browser.TryGetSelectedShellIds (hostPaths))
    {
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
    }

    m_context.folderSync.RefreshAfterHostChange();
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
