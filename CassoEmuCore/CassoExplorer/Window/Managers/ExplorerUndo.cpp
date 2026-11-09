#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Managers/ExplorerTransferPrompts.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Core/AppTimerIds.h"
#include "Core/TextEncoding.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::ExplorerUndo
//
////////////////////////////////////////////////////////////////////////////////

ExplorerUndo::ExplorerUndo (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::Push
//
//  Kept to a few dozen steps, as an image deletion's copy is a folder on disk.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUndo::Push (ExplorerUndoStep step)
{
    static constexpr size_t  s_kMaxSteps = 50;



    m_undo.push_back (std::move (step));

    if (m_undo.size() > s_kMaxSteps)
    {
        std::error_code  ignored;

        if (!m_undo.front().savedDir.empty())
        {
            std::filesystem::remove_all (m_undo.front().savedDir, ignored);
        }

        m_undo.erase (m_undo.begin());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ExplorerUndo::GetLabel (ExplorerUndoStep::Kind kind)
{
    switch (kind)
    {
        case ExplorerUndoStep::Kind::Recycle:
        case ExplorerUndoStep::Kind::ImageDelete:    return L"&Undo Delete";
        case ExplorerUndoStep::Kind::HostRename:
        case ExplorerUndoStep::Kind::ImageRename:    return L"&Undo Rename";
        case ExplorerUndoStep::Kind::HostNewFolder:
        case ExplorerUndoStep::Kind::ImageNewFolder: return L"&Undo New Folder";
        case ExplorerUndoStep::Kind::HostNewItem:    return L"&Undo New";
        case ExplorerUndoStep::Kind::HostCopy:
        case ExplorerUndoStep::Kind::ImagePut:       return L"&Undo Copy";
        case ExplorerUndoStep::Kind::HostMove:       return L"&Undo Move";
        default:                             return L"&Undo";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::UndoLast
//
//  The newest step is taken off and undone by its kind. A step the shell's
//  prompt canceled, or an image deletion not every file of which came back,
//  goes back on, to be undone later.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUndo::UndoLast()
{
    HRESULT           hr       = S_OK;
    ExplorerUndoStep  step;
    bool              finished = true;



    if (m_undo.empty())
    {
        MessageBeep (MB_OK);
        return;
    }

    step = std::move (m_undo.back());
    m_undo.pop_back();

    switch (step.kind)
    {
        case ExplorerUndoStep::Kind::Recycle:       hr = UndoRecycle (step);    break;
        case ExplorerUndoStep::Kind::HostRename:    hr = UndoHostRename (step); break;
        case ExplorerUndoStep::Kind::HostNewFolder:
        case ExplorerUndoStep::Kind::HostNewItem:
        case ExplorerUndoStep::Kind::HostCopy:      hr = UndoHostCreate (step); break;
        case ExplorerUndoStep::Kind::HostMove:      hr = UndoHostMove (step);   break;

        default:
            finished = UndoInImage (step);

            if (!finished)
            {
                m_undo.push_back (std::move (step));
            }

            return;
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        m_undo.push_back (std::move (step));
    }
    else if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"That could not be undone.", MB_ICONWARNING);
    }

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::UndoRecycle
//
//  The shell restores the files a moment later, so the folder is listed again
//  on a timer until they appear.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerUndo::UndoRecycle (const ExplorerUndoStep & step)
{
    HRESULT  hr = m_context.shellVerbs.RestoreRecycled (m_context.window.GetHwnd(), step.paths);



    m_context.manifestSync.NoteRestores (step.paths);
    m_restoreTicks = kRestoreRetries;
    SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerRestore, 1000, nullptr);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::UndoHostRename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerUndo::UndoHostRename (const ExplorerUndoStep & step)
{
    HRESULT  hr = m_context.shellVerbs.RenameItem (m_context.window.GetHwnd(), step.paths[0], step.oldName);



    if (SUCCEEDED (hr))
    {
        m_context.manifestSync.NoteRename (step.paths[0], step.oldName);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::UndoHostCreate
//
//  A new folder, a new item or a copy goes to the Recycle Bin.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerUndo::UndoHostCreate (const ExplorerUndoStep & step)
{
    return m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), step.paths);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::UndoHostMove
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerUndo::UndoHostMove (const ExplorerUndoStep & step)
{
    return m_context.shellVerbs.MoveItemsTo (m_context.window.GetHwnd(), step.paths, step.sources);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::UndoInImage
//
//  A rename and a new folder are undone through the selection, so the list
//  goes back to where the step was made, with the entry in it; otherwise the
//  undo would act on whatever is selected. False when the step stays to be
//  undone again: an image deletion whose saved files didn't all go back, as
//  the saved copies are the only ones.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerUndo::UndoInImage (const ExplorerUndoStep & step)
{
    std::vector<std::wstring>      saved;
    std::error_code                ignored;
    CassoExplorerActions::Outcome  restored;
    bool                           finished = true;
    bool                           selects  = step.kind == ExplorerUndoStep::Kind::ImageRename || step.kind == ExplorerUndoStep::Kind::ImageNewFolder;



    if (m_context.browser.GetLocation() != step.location)
    {
        m_context.browser.NavigateToLocation (step.location);
        m_context.listPane.FillList();
    }

    if (m_context.browser.GetLocation() != step.location || (selects && !m_context.listPane.SelectRowNamed (step.newName)))
    {
        m_context.window.ShowMessage (L"That could not be undone.", MB_ICONWARNING);
        return true;
    }

    switch (step.kind)
    {
        case ExplorerUndoStep::Kind::ImageRename:
            m_context.commandManager.ReportOutcome (m_context.actions.RenameSelected (step.oldName), L"Undo rename");
            break;

        case ExplorerUndoStep::Kind::ImageNewFolder:
            m_context.commandManager.ReportOutcome (m_context.actions.DeleteSelected(), L"Undo new folder");
            break;

        case ExplorerUndoStep::Kind::ImagePut:
            m_context.commandManager.ReportOutcome (m_context.actions.DeleteEntries (TextEncoding::WideToNarrow (step.location.path), step.entries), L"Undo copy");
            break;

        default:
            for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (step.savedDir, ignored))
            {
                saved.push_back (entry.path().wstring());
            }

            restored = m_context.actions.PutFiles (saved, m_context.prompts.MakeAddressPrompt());
            m_context.commandManager.ReportOutcome (restored, L"Undo delete");
            finished = restored.IsWhole();

            if (finished)
            {
                std::filesystem::remove_all (step.savedDir, ignored);
            }

            break;
    }

    m_context.listPane.FillList();

    return finished;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo::PushPut
//
//  A put or a copy into an image is undone by deleting what it made, in the
//  directory it went into.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUndo::PushPut (const CassoExplorerActions::Outcome & outcome, const std::wstring & image, const std::string & inner)
{
    ExplorerUndoStep  step;



    if (outcome.created.empty())
    {
        return;
    }

    step.kind     = ExplorerUndoStep::Kind::ImagePut;
    step.location = inner.empty() ? Location::MakeDiskImage (image) : Location::MakeDiskDirectory (image, inner);
    step.entries  = outcome.created;

    Push (std::move (step));
}
