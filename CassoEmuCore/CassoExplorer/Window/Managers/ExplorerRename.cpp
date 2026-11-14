#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerRename.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "Core/TextEncoding.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerRename::ExplorerRename
//
////////////////////////////////////////////////////////////////////////////////

ExplorerRename::ExplorerRename (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerRename::Create
//
//  The rename box, an edit box laid over a list row's name or a tree node's
//  label, as Explorer's F2 opens one.
//
//  The rename box, as a child of the window, and the pointer and keys while
//  a rename is open.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerRename::Create()
{
    m_renameBox = m_context.window.CreatePaneRoutedChild<DxuiTextInput>();

    m_renameBox->SetVisible      (false);
    m_renameBox->SetOverText     (true);
    m_renameBox->SetHwnd         (m_context.window.GetHwnd());
    m_renameBox->SetTextRenderer (m_context.window.GetTextRenderer());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerRename::HandleRenameMouse
//
//  While a rename is open, the box has the events over it, and moves and
//  releases anywhere; a press or the wheel elsewhere commits the name.
//  Returns whether the event was over the box.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerRename::HandleRenameMouse (const DxuiMouseEvent & ev)
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
//  ExplorerRename::HandleRenameKey
//
//  A rename in place has every key: Enter commits the new name, and Escape
//  puts the old one back.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerRename::HandleRenameKey (const DxuiKeyEvent & ev)
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
//  ExplorerRename::BeginRename
//
//  Over the name, with the name selected up to its extension, as Explorer
//  selects it; an entry inside an image has no extension to leave out.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerRename::BeginRename()
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
//  ExplorerRename::BeginTreeRename
//
//  Over the node's label, to the tree's edge, with the whole name selected,
//  as File Explorer renames a folder in its navigation pane.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerRename::BeginTreeRename (const std::wstring & id)
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
//  ExplorerRename::EndRename
//
//  A name left as it was, or emptied, changes nothing. A renamed row is
//  selected under its new name, as File Explorer selects it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerRename::EndRename (bool commit)
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
                m_context.undo.Push (ExplorerUndoStep { ExplorerUndoStep::Kind::HostRename, Location(), { (std::filesystem::path (path).parent_path() / newName).wstring() }, oldName, newName });
                m_context.actions.GetA2retroNet().NoteMove (path, (std::filesystem::path (path).parent_path() / newName).wstring());
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
                m_context.undo.Push (ExplorerUndoStep { ExplorerUndoStep::Kind::HostRename, Location(), { (std::filesystem::path (path).parent_path() / newName).wstring() }, oldName, newName });
                m_context.manifestSync.NoteRename (path, newName);
                m_context.actions.GetA2retroNet().NoteMove (path, (std::filesystem::path (path).parent_path() / newName).wstring());
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
            m_context.undo.Push (ExplorerUndoStep { ExplorerUndoStep::Kind::ImageRename, m_context.browser.GetLocation(), {}, oldName, newName });
        }

        m_context.commandManager.ReportOutcome (renamed, L"Rename");
        m_context.listPane.FillList();

        if (renamed.Succeeded())
        {
            m_context.listPane.SelectRowNamed (newName);
        }
    }
}
