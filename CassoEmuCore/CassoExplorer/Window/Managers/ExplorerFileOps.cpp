#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/AppleFilePropertiesDialog.h"
#include "CassoExplorer/WozPropertiesDialog.h"
#include "CassoExplorer/Model/WozProperties.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "AssetBootstrap.h"
#include "Config/UserConfigStore.h"
#include "Core/JsonWriter.h"
#include "Core/MachineScanner.h"
#include "Core/TextEncoding.h"
#include "Core/AppMessages.h"
#include "Core/AppTimerIds.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "Seams/Win32IntentChannel.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ExplorerFileOps
//
////////////////////////////////////////////////////////////////////////////////

ExplorerFileOps::ExplorerFileOps (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::Create
//
//  The rename box, an edit box laid over a list row's name or a tree node's
//  label, as Explorer's F2 opens one.
//
//  The rename box, as a child of the window, and the pointer and keys while
//  a rename is open.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::Create()
{
    m_renameBox = m_context.window.CreateChild<DxuiTextInput>();

    m_renameBox->SetVisible      (false);
    m_renameBox->SetOverText     (true);
    m_renameBox->SetHwnd         (m_context.window.GetHwnd());
    m_renameBox->SetTextRenderer (m_context.window.GetTextRenderer());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::HandleRenameMouse
//
//  While a rename is open, the box has the events over it, and moves and
//  releases anywhere; a press or the wheel elsewhere commits the name.
//  Returns whether the event was over the box.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerFileOps::HandleRenameMouse (const DxuiMouseEvent & ev)
{
    bool  over = ExplorerLayout::Contains (m_renameBox->GetBounds(), ev.positionDip);



    if (!IsRenaming())
    {
        return false;
    }

    if (!over && (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel))
    {
        EndRename (true);
        return false;
    }

    m_renameBox->OnMouse (ev);
    m_context.window.Invalidate();

    return over;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::HandleRenameKey
//
//  A rename in place has every key: Enter commits the new name, and Escape
//  puts the old one back.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::HandleRenameKey (const DxuiKeyEvent & ev)
{
    if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE))
    {
        EndRename (ev.vk == VK_RETURN);
    }
    else
    {
        m_renameBox->OnKey (ev);
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::BeginRename
//
//  Over the name, with the name selected up to its extension, as Explorer
//  selects it; an entry inside an image has no extension to leave out.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::BeginRename()
{
    RECT          cell = {};
    int           row  = -1;
    std::wstring  name;
    size_t        dot  = std::wstring::npos;
    bool          host = !m_context.browser.IsVolumeLocation();



    //  The focused item, as Explorer renames, even with others selected
    //  around it.
    row = m_context.listPane.GetFocusedRow();

    if (row < 0 || std::find (m_context.browser.GetSelectedRows().begin(), m_context.browser.GetSelectedRows().end(), row) == m_context.browser.GetSelectedRows().end())
    {
        row = m_context.browser.GetSelectedRows().empty() ? -1 : m_context.browser.GetSelectedRows()[0];
    }

    if (row < 0 || row >= (int) m_context.browser.GetRows().size())
    {
        return;
    }

    if (!m_context.listPane.TryGetRenameRect (row, cell))
    {
        return;
    }

    name = m_context.browser.GetRows()[(size_t) row].name;

    //  A shortcut's hidden extension stays out of the box and goes back on
    //  the new name, as in File Explorer.
    m_renameHiddenExtension.clear();

    if (host && !m_context.browser.GetRows()[(size_t) row].label.empty() && name.starts_with (m_context.browser.GetRows()[(size_t) row].label))
    {
        m_renameHiddenExtension = name.substr (m_context.browser.GetRows()[(size_t) row].label.size());
        name                    = m_context.browser.GetRows()[(size_t) row].label;
        host                    = false;   // the whole name is selected
    }

    m_renameBox->SetMaxLength (host ? MAX_PATH : kMaxCatalogName);
    m_renameBox->SetText      (name);
    m_renameBox->Layout       (cell, m_context.scaler);
    m_renameBox->SetVisible   (true);
    m_renameBox->SetFocused   (true);
    m_renameBox->SelectAll();

    dot = host ? name.find_last_of (L'.') : std::wstring::npos;

    //  The stem only, so typing keeps the type.
    if (dot != std::wstring::npos && dot > 0 && !m_context.browser.GetRows()[(size_t) row].isDirectory)
    {
        m_renameBox->SetSelection (0, dot);
    }

    m_renameRow = row;
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::BeginTreeRename
//
//  Over the node's label, to the tree's edge, with the whole name selected,
//  as File Explorer renames a folder in its navigation pane.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::BeginTreeRename (const std::wstring & id)
{
    RECT          label = {};
    std::wstring  path;



    if (!m_context.treePane.HasNode (id) || !m_context.browser.TryGetNodePath (id, path))
    {
        return;
    }

    if (IsRenaming())
    {
        EndRename (false);
    }

    if (!m_context.treePane.TryGetRenameRect (id, label))
    {
        return;
    }

    m_renameBox->SetMaxLength (MAX_PATH);
    m_renameBox->SetText      (std::filesystem::path (path).filename().wstring());
    m_renameBox->Layout       (label, m_context.scaler);
    m_renameBox->SetVisible   (true);
    m_renameBox->SetFocused   (true);
    m_renameBox->SelectAll();

    m_renameTreeId = id;
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::EndRename
//
//  A name left as it was, or emptied, changes nothing. A renamed row is
//  selected under its new name, as File Explorer selects it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::EndRename (bool commit)
{
    HRESULT       hr      = S_OK;
    int           row     = m_renameRow;
    std::wstring  newName = m_renameBox->GetText() + ((m_renameRow >= 0) ? m_renameHiddenExtension : std::wstring());
    std::wstring  oldName;
    std::wstring  path;
    std::wstring  treeId  = m_renameTreeId;



    m_renameRow = -1;
    m_renameTreeId.clear();
    m_renameBox->SetFocused (false);
    m_renameBox->SetVisible (false);
    m_context.window.Invalidate();

    //  A folder renamed from its node in the tree, as in File Explorer.
    if (!treeId.empty())
    {
        oldName = std::filesystem::path (m_context.browser.TryGetNodePath (treeId, path) ? path : std::wstring()).filename().wstring();

        if (commit && !path.empty() && !newName.empty() && newName != oldName)
        {
            hr = m_context.shellVerbs.RenameItem (m_context.window.GetHwnd(), path, newName);

            if (SUCCEEDED (hr))
            {
                PushUndo (UndoStep { UndoStep::Kind::HostRename, Location(), { (std::filesystem::path (path).parent_path() / newName).wstring() }, oldName, newName });
            }

            m_context.folderSync.RefreshAfterHostChange();
        }

        return;
    }

    if (!commit || row < 0 || row >= (int) m_context.browser.GetRows().size())
    {
        return;
    }

    oldName = m_context.browser.GetRows()[(size_t) row].name;

    if (newName.empty() || newName == oldName)
    {
        return;
    }

    if (!m_context.browser.IsVolumeLocation())
    {
        if (m_context.browser.TryGetRowPath (row, path))
        {
            hr = m_context.shellVerbs.RenameItem (m_context.window.GetHwnd(), path, newName);

            if (SUCCEEDED (hr))
            {
                PushUndo (UndoStep { UndoStep::Kind::HostRename, Location(), { (std::filesystem::path (path).parent_path() / newName).wstring() }, oldName, newName });
                m_context.manifestSync.NoteRename (path, newName);
            }
        }

        //  A search's row takes the new name at once, since searching again
        //  would find the old one until the index catches up.
        if (SUCCEEDED (hr) && SearchQuery::IsId (m_context.browser.GetLocation().path))
        {
            bool  kept = m_context.browser.RenameSearchRow (row, newName);

            m_context.listPane.FillList();

            if (kept)
            {
                m_context.listPane.SelectRowNamed (newName);
            }

            return;
        }

        m_context.folderSync.RefreshAfterHostChange();

        if (SUCCEEDED (hr))
        {
            m_context.listPane.SelectRowNamed (newName);
        }

        return;
    }

    {
        CassoExplorerActions::Outcome  renamed;

        //  The row renamed is the one the box was over, whatever else is
        //  selected.
        m_context.browser.SetSelectedRows ({ row });
        renamed = m_context.actions.RenameSelected (newName);

        if (renamed.Succeeded())
        {
            PushUndo (UndoStep { UndoStep::Kind::ImageRename, m_context.browser.GetLocation(), {}, oldName, newName });
        }

        m_context.commandManager.ReportOutcome (renamed, L"Rename");
        m_context.listPane.FillList();

        if (renamed.Succeeded())
        {
            m_context.listPane.SelectRowNamed (newName);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::DeleteSelectionPermanently
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::DeleteSelectionPermanently()
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
//  ExplorerFileOps::RecycleHostFiles
//
//  To the Recycle Bin rather than gone, so a move whose files a disk image
//  has in another form can be undone. Through the shell's delete, so a file
//  too large for the bin is deleted only once the user confirms.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerFileOps::RecycleHostFiles (const std::vector<std::wstring> & paths)
{
    HRESULT  hr = m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), paths);



    return (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED)) ? S_OK : hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::GetPasteFolder
//
//  A single folder row takes the paste; otherwise the folder being shown.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerFileOps::GetPasteFolder() const
{
    Location  location;



    if (m_context.browser.GetSelectedRows().size() == 1
     && m_context.browser.TryGetRowLocation (m_context.browser.GetSelectedRows()[0], location)
     && location.kind == Location::Kind::HostFolder)
    {
        return location.path;
    }

    return m_context.browser.GetLocation().path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::CopyEntriesToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::CopyEntriesToClipboard (HostFileNaming::Style style)
{
    std::wstring  failure;
    HRESULT       hr      = CassoExplorerDragOut::CopyToClipboard (m_context.browser, style, failure);



    //  A file that could not be read has a message with the file's name; for
    //  any other failure, as the clipboard in use, the text comes from
    //  its code.
    if (FAILED (hr))
    {
        m_context.window.ShowMessage ((failure.empty() ? L"The files could not be copied: " + CassoExplorerDragOut::DescribeCopyFailure (hr) : failure + L"."), MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::CutEntriesToClipboard
//
//  What is deleted is read now, while the entries are on display: the
//  paste may come after the window has moved elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::CutEntriesToClipboard (HostFileNaming::Style style)
{
    std::wstring            failure;
    HRESULT                 hr      = S_OK;
    CutSource               cut;
    std::vector<FileEntry>  entries;
    HWND                    hwnd    = m_context.window.GetHwnd();



    if (m_context.browser.IsAppleDriveLocation())
    {
        cut.onDrive = true;

        if (!m_context.actions.TryPlanDriveDeletion (cut.drive))
        {
            return;
        }
    }
    else
    {
        cut.image = TextEncoding::WideToNarrow (m_context.browser.GetLocation().path);
        m_context.browser.GetSelectedEntries (entries);

        for (const FileEntry & entry : entries)
        {
            cut.catalogPaths.push_back (m_context.browser.GetEntryPath (entry));
        }
    }

    hr = CassoExplorerDragOut::CopyToClipboard (m_context.browser, style, failure, true, [hwnd]()
    {
        BOOL  posted = PostMessageW (hwnd, (UINT) AppMessage::ExplorerCutPasted, 0, 0);

        IGNORE_RETURN_VALUE (posted, TRUE);
    });

    if (FAILED (hr))
    {
        m_context.window.ShowMessage ((failure.empty() ? L"The files could not be cut: " + CassoExplorerDragOut::DescribeCopyFailure (hr) : failure + L"."), MB_ICONWARNING);
        return;
    }

    cut.valid = true;
    m_cut     = std::move (cut);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::FinishCut
//
//  The cut entries deleted, now that a target has pasted them. A drive's
//  deletions go over the link and are reported as they finish.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::FinishCut()
{
    CassoExplorerActions::Outcome  outcome;
    CutSource                      cut     = std::move (m_cut);



    m_cut = CutSource();

    if (!cut.valid)
    {
        return;
    }

    outcome = cut.onDrive ? m_context.actions.RunDriveDeletion (cut.drive)
                          : m_context.actions.DeleteEntries (cut.image, cut.catalogPaths);

    m_context.commandManager.ReportOutcome (outcome, L"Move");
    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::CopySelectedPaths
//
//  Each path in quotes on its own line, as Explorer's Copy as path gives them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::CopySelectedPaths()
{
    std::wstring  text;
    std::wstring  path;



    for (int row : m_context.browser.GetSelectedRows())
    {
        if (!m_context.browser.TryGetRowPath (row, path))
        {
            continue;
        }

        if (!text.empty())
        {
            text += L"\r\n";
        }

        text += L"\"" + path + L"\"";
    }

    if (!text.empty())
    {
        DxuiClipboard::SetText (m_context.window.GetHwnd(), text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::PushUndo
//
//  Kept to a few dozen steps, as an image deletion's copy is a folder on disk.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::PushUndo (UndoStep step)
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
//  ExplorerFileOps::GetUndoLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ExplorerFileOps::GetUndoLabel (UndoStep::Kind kind)
{
    switch (kind)
    {
        case UndoStep::Kind::Recycle:
        case UndoStep::Kind::ImageDelete:    return L"&Undo Delete";
        case UndoStep::Kind::HostRename:
        case UndoStep::Kind::ImageRename:    return L"&Undo Rename";
        case UndoStep::Kind::HostNewFolder:
        case UndoStep::Kind::ImageNewFolder: return L"&Undo New Folder";
        case UndoStep::Kind::HostNewItem:    return L"&Undo New";
        case UndoStep::Kind::HostCopy:
        case UndoStep::Kind::ImagePut:       return L"&Undo Copy";
        case UndoStep::Kind::HostMove:       return L"&Undo Move";
        default:                             return L"&Undo";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::UndoLast
//
//  An image step goes back to the image it was made in first, since what it
//  puts back is selected by name in the list.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::UndoLast()
{
    HRESULT                        hr = S_OK;
    UndoStep                       step;
    std::vector<std::wstring>      saved;
    std::error_code                ignored;
    CassoExplorerActions::Outcome  restored;



    if (m_undo.empty())
    {
        MessageBeep (MB_OK);
        return;
    }

    step = std::move (m_undo.back());
    m_undo.pop_back();

    switch (step.kind)
    {
        case UndoStep::Kind::Recycle:
            hr = m_context.shellVerbs.RestoreRecycled (m_context.window.GetHwnd(), step.paths);
            m_context.manifestSync.NoteRestores (step.paths);
            m_restoreTicks = kRestoreRetries;
            SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerRestore, 1000, nullptr);
            break;

        case UndoStep::Kind::HostRename:
            hr = m_context.shellVerbs.RenameItem (m_context.window.GetHwnd(), step.paths[0], step.oldName);

            if (SUCCEEDED (hr))
            {
                m_context.manifestSync.NoteRename (step.paths[0], step.oldName);
            }

            break;

        case UndoStep::Kind::HostNewFolder:
        case UndoStep::Kind::HostNewItem:
        case UndoStep::Kind::HostCopy:
            hr = m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), step.paths);
            break;

        case UndoStep::Kind::HostMove:
            hr = m_context.shellVerbs.MoveItemsTo (m_context.window.GetHwnd(), step.paths, step.sources);
            break;

        default:
            if (m_context.browser.GetLocation() != step.location)
            {
                m_context.browser.NavigateToLocation (step.location);
                m_context.listPane.FillList();
            }

            //  A rename and a new folder are undone through the selection, so
            //  the list has to be where the step was made, with the entry in it;
            //  otherwise the undo would act on whatever is selected.
            if (m_context.browser.GetLocation() != step.location
                || ((step.kind == UndoStep::Kind::ImageRename || step.kind == UndoStep::Kind::ImageNewFolder) && !m_context.listPane.SelectRowNamed (step.newName)))
            {
                m_context.window.ShowMessage (L"That could not be undone.", MB_ICONWARNING);
                return;
            }

            if (step.kind == UndoStep::Kind::ImageRename)
            {
                m_context.commandManager.ReportOutcome (m_context.actions.RenameSelected (step.oldName), L"Undo rename");
            }
            else if (step.kind == UndoStep::Kind::ImageNewFolder)
            {
                m_context.commandManager.ReportOutcome (m_context.actions.DeleteSelected(), L"Undo new folder");
            }
            else if (step.kind == UndoStep::Kind::ImagePut)
            {
                m_context.commandManager.ReportOutcome (m_context.actions.DeleteEntries (TextEncoding::WideToNarrow (step.location.path), step.entries), L"Undo copy");
            }
            else
            {
                for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (step.savedDir, ignored))
                {
                    saved.push_back (entry.path().wstring());
                }

                restored = m_context.actions.PutFiles (saved, MakeAddressPrompt());
                m_context.commandManager.ReportOutcome (restored, L"Undo delete");

                //  The saved copies are the only ones, so they go only once
                //  every file is back.
                if (restored.IsWhole())
                {
                    std::filesystem::remove_all (step.savedDir, ignored);
                }
                else
                {
                    m_undo.push_back (std::move (step));
                }
            }

            m_context.listPane.FillList();
            return;
    }

    //  Canceled at the shell's prompt, the step can be undone later.
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
//  ExplorerFileOps::PasteHere
//
//  The clipboard's files into the folder, with an undo that takes back what
//  the paste made: a copy goes to the Recycle Bin, a move goes back.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::PasteHere (const std::wstring & folder)
{
    IShellItemVerbs::PasteResult  pasted;
    ComPtr<IDataObject>           data;
    HRESULT                       hr     = OleGetClipboard (&data);



    //  An image's or a linked Apple's files are written here rather than by
    //  the folder's drop target, whose name clash prompt has no Keep both and
    //  gives the files a folder they never came from.
    if (SUCCEEDED (hr) && ActionsCopy::IsCassoCopy (data.Get()))
    {
        CassoExplorerActions::Outcome  outcome = m_context.actions.PasteVirtualFiles (data.Get(), folder, MakeCollisionPrompt(), pasted.created);

        m_context.commandManager.ReportOutcome (outcome, L"Copy");
        pasted.sources.assign (pasted.created.size(), std::wstring());

        //  A cut's files are deleted where they were, now they are here.
        ReportPasted (data.Get(), outcome);
    }
    else
    {
        hr = m_context.shellVerbs.PasteInto (m_context.window.GetHwnd(), folder, pasted);
    }

    //  After a paste that failed or was canceled, the cut files remain cut,
    //  as in File Explorer. Any error is in the shell's dialog.
    if (SUCCEEDED (hr))
    {
        m_context.listPane.ClearCutPaths();
    }

    if (!pasted.created.empty())
    {
        UndoStep  step;

        step.kind    = pasted.moved ? UndoStep::Kind::HostMove : UndoStep::Kind::HostCopy;
        step.paths   = pasted.created;
        step.sources = pasted.sources;

        PushUndo (std::move (step));
    }

    //  What was pasted is selected once it is listed, as in File Explorer.
    if (!pasted.created.empty())
    {
        std::vector<std::wstring>  names;

        for (const std::wstring & path : pasted.created)
        {
            names.push_back (std::filesystem::path (path).filename().wstring());
        }

        m_context.listPane.SelectWhenListed (folder, std::move (names));
    }

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::PasteIntoVolume
//
//  The clipboard's files into the disk image or the linked Apple's drive
//  open in the list, by the rules a drop there follows: another image's
//  entries are copied byte for byte, and host files go in by Put's rules.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::PasteIntoVolume()
{
    HRESULT                        hr         = S_OK;
    ComPtr<IDataObject>            data;
    ExplorerDragDrop::DropSource   source;
    Location                       location   = m_context.browser.GetLocation();
    VolumeKind                     targetKind = VolumeKind::Unknown;
    std::string                    inner;
    VolumeListing                  listing;
    CassoExplorerActions::Outcome  outcome;



    hr = OleGetClipboard (&data);

    if (FAILED (hr) || !m_context.dragDrop.ReadDropSource (data.Get(), source))
    {
        return;
    }

    if (m_context.browser.IsAppleDriveLocation())
    {
        outcome = source.fromImage ? m_context.actions.CopyEntriesToAppleDrive (source.image, source.kind, source.catalogPaths)
                                   : m_context.actions.PutFiles (source.hostPaths, MakeAddressPrompt());

        m_context.commandManager.ReportOutcome (outcome, L"Put");
        ReportPasted (data.Get(), outcome);
        return;
    }

    inner = (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string();

    if (!m_context.browser.GetOperations().List (TextEncoding::WideToNarrow (location.path), inner, listing, targetKind).Succeeded())
    {
        m_context.window.ShowMessage (L"The disk image could not be read.", MB_ICONWARNING);
        return;
    }

    outcome = source.fromImage ? m_context.actions.CopyEntriesInto (source.image, source.kind, source.catalogPaths, location.path, targetKind, inner)
                               : m_context.actions.PutInto (location.path, targetKind, inner, source.hostPaths, MakeAddressPrompt());

    PushPutUndo (outcome, location.path, inner);
    m_context.commandManager.ReportOutcome (outcome, L"Put");

    //  Pasted back where it was cut from, nothing moved.
    if (!(source.fromImage && _wcsicmp (TextEncoding::NarrowToWide (source.image).c_str(), location.path.c_str()) == 0 && IsCutFrom (inner)))
    {
        ReportPasted (data.Get(), outcome);
    }

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ReportPasted
//
//  As File Explorer reports a paste of a cut, so the source deletes what it
//  cut: only when every file landed whole.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::ReportPasted (IDataObject * data, const CassoExplorerActions::Outcome & outcome)
{
    HRESULT    hr        = S_OK;
    FORMATETC  preferred = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_PREFERREDDROPEFFECT), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    FORMATETC  pasted    = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_PASTESUCCEEDED), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM  medium    = {};
    DWORD      effect    = DROPEFFECT_NONE;
    DWORD    * locked    = nullptr;



    if (data == nullptr || !outcome.IsWhole() || outcome.written == 0)
    {
        return;
    }

    hr = data->GetData (&preferred, &medium);

    if (SUCCEEDED (hr))
    {
        locked = (DWORD *) GlobalLock (medium.hGlobal);

        if (locked != nullptr)
        {
            effect = *locked;
            GlobalUnlock (medium.hGlobal);
        }

        ReleaseStgMedium (&medium);
    }

    if (effect != DROPEFFECT_MOVE)
    {
        return;
    }

    medium         = {};
    medium.tymed   = TYMED_HGLOBAL;
    medium.hGlobal = GlobalAlloc (GMEM_MOVEABLE, sizeof (DWORD));

    if (medium.hGlobal == nullptr)
    {
        return;
    }

    locked = (DWORD *) GlobalLock (medium.hGlobal);

    if (locked != nullptr)
    {
        *locked = DROPEFFECT_MOVE;
        GlobalUnlock (medium.hGlobal);
    }

    hr = data->SetData (&pasted, &medium, TRUE);

    if (FAILED (hr))
    {
        ReleaseStgMedium (&medium);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::IsCutFrom
//
//  Whether the last cut's entries came from the image directory `inner`.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerFileOps::IsCutFrom (const std::string & inner) const
{
    if (!m_cut.valid || m_cut.onDrive || m_cut.catalogPaths.empty())
    {
        return false;
    }

    const std::string &  first = m_cut.catalogPaths[0];
    size_t               slash = first.find_last_of ('/');

    return ((slash == std::string::npos) ? std::string() : first.substr (0, slash)) == inner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::PushPutUndo
//
//  A put or a copy into an image is undone by deleting what it made, in the
//  directory it went into.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::PushPutUndo (const CassoExplorerActions::Outcome & outcome, const std::wstring & image, const std::string & inner)
{
    UndoStep  step;



    if (outcome.created.empty())
    {
        return;
    }

    step.kind     = UndoStep::Kind::ImagePut;
    step.location = inner.empty() ? Location::MakeDiskImage (image) : Location::MakeDiskDirectory (image, inner);
    step.entries  = outcome.created;

    PushUndo (std::move (step));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::MakeAddressPrompt
//
//  Asks for a binary's load address, prefilled with the content's
//  suggestion, and asks again after an address that does not parse.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::AddressFn ExplorerFileOps::MakeAddressPrompt()
{
    return [this] (const std::wstring & hostName, Word suggested, Word & outAddress)
    {
        std::wstring  text = std::format (L"${:04X}", suggested);

        for (;;)
        {
            if (!CassoExplorerPromptDialog::Ask (m_context.window.GetHwnd(), m_context.theme, L"Load address",
                                           hostName + L" is a binary. Load address:", text, 8, text))
            {
                return false;
            }

            if (ActionsGoTo::TryParseAddress (text, outAddress))
            {
                return true;
            }

            m_context.window.ShowMessage (L"Type an address from $0000 to $FFFF.", MB_ICONWARNING);
        }
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::MakeCollisionPrompt
//
//  File Explorer's question for a file of the same name in the folder a
//  copy goes into. Keep both is the default, so Enter or Escape writes over
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::CollisionFn ExplorerFileOps::MakeCollisionPrompt()
{
    return [this] (const std::wstring & hostPath, int index, int count)
    {
        std::filesystem::path  path    (hostPath);
        std::wstring           ofCount = (count > 1) ? std::format (L" ({} of {})", index, count) : std::wstring();
        std::wstring           text    = std::format (L"{} already has a file called \"{}\".{}\n\nReplace it with the copy, skip this file, or keep both?",
                                                      path.parent_path().filename().wstring(), path.filename().wstring(), ofCount);
        int                    choice  = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, text.c_str(), L"Replace or skip files", MB_YESNOCANCEL | MB_DEFBUTTON3,
                                                         { L"Replace", L"Skip", L"Keep both" });

        return (choice == IDYES) ? CassoExplorerActions::Collision::Replace
             : (choice == IDNO)  ? CassoExplorerActions::Collision::Skip
                                 : CassoExplorerActions::Collision::KeepBoth;
    };
}
