#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/AppleFilePropertiesDialog.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
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
            }
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



    if (!TryGetSelectedShellIds (ids))
    {
        return;
    }

    hr = m_context.shellVerbs.DeletePermanently (m_context.window.GetHwnd(), ids);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::TryGetSelectedShellIds
//
//  The selection by the shell's names for it, when every selected row is a
//  host file or an item the shell lists; false when any is inside a disk
//  image.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerFileOps::TryGetSelectedShellIds (std::vector<std::wstring> & outIds) const
{
    const std::vector<CatalogRow> &  rows = m_context.browser.GetRows();



    outIds.clear();

    if (m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        m_context.browser.GetSelectedHostPaths (outIds);
        return !outIds.empty();
    }

    //  CassoLink's items drag as the files they are written to.
    if (m_context.browser.TryGetSelectedCassoLinkPaths (outIds))
    {
        return true;
    }

    if (m_context.browser.GetLocation().kind != Location::Kind::ShellFolder)
    {
        return false;
    }

    for (int row : m_context.browser.GetSelectedRows())
    {
        if (row < 0 || (size_t) row >= rows.size() || !rows[(size_t) row].imagePath.empty())
        {
            outIds.clear();
            return false;
        }

        outIds.push_back (rows[(size_t) row].hostPath.empty() ? rows[(size_t) row].shellId : rows[(size_t) row].hostPath);
    }

    return !outIds.empty();
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
//  ExplorerFileOps::GetSelectedImagePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerFileOps::GetSelectedImagePath() const
{
    std::vector<std::wstring>  paths;



    m_context.browser.GetSelectedHostPaths (paths);

    return (paths.size() == 1) ? paths[0] : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::InsertIntoDrive
//
//  The Casso that launched this browser when it is still there, otherwise
//  any running Casso, otherwise a new one with the disk in drive 1. The
//  answer arrives later as a reply message.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::InsertIntoDrive (const std::wstring & imagePath, int drive)
{
    HWND  target = nullptr;
    bool  sent   = false;



    if (imagePath.empty())
    {
        return;
    }

    target = FindCassoTarget();

    if (target == nullptr)
    {
        OpenInNewCasso (imagePath);
        return;
    }

    sent = Win32IntentChannel::SendTo (target, m_context.window.GetHwnd(), Win32IntentChannel::GetMessageId(),
                                       Win32IntentChannel::EncodeInsert (TextEncoding::WideToNarrow (imagePath), drive));

    if (!sent)
    {
        m_context.window.ShowMessage (L"Casso did not answer the request to insert the disk.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::OpenInNewCasso
//
//  The folder is recorded here, since the new Casso learns of the disk from
//  its command line rather than from a hand-off it would record itself.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::OpenInNewCasso (const std::wstring & imagePath)
{
    wchar_t       module[MAX_PATH] = {};
    DWORD         length           = GetModuleFileNameW (nullptr, module, ARRAYSIZE (module));
    std::wstring  exe;
    HRESULT       hr               = S_OK;



    if (imagePath.empty() || length == 0 || length >= ARRAYSIZE (module))
    {
        return;
    }

    exe = LaunchCommand::GetSiblingPath (std::filesystem::path (module).parent_path().wstring(), LaunchCommand::kCassoExe);

    if (!m_launcher.Exists (exe))
    {
        m_context.window.ShowMessage (LaunchCommand::DescribeMissing (exe), MB_ICONERROR);
        return;
    }

    hr = m_launcher.Launch (exe, LaunchCommand::MakeCassoArguments (imagePath, m_context.app.titlePrefix));

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"Casso could not be started.", MB_ICONERROR);
        return;
    }

    if (m_context.app.fs != nullptr)
    {
        KnownFolderStore  store (*m_context.app.fs, m_context.app.baseDir);

        hr = store.Append (std::filesystem::path (imagePath).parent_path().wstring(), (int64_t) _time64 (nullptr));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
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
//  ExplorerFileOps::OpenSelectedEntries
//
//  Files inside an image open in the programs Windows has for them, from
//  copies in a folder of their own, as Explorer opens a file inside a zip.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::OpenSelectedEntries()
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
//  ExplorerFileOps::OpenEachSelected
//
//  Several items open each on its own, as Explorer's do: a folder or an image
//  in a tab of its own, a file in its program. All are read before the first
//  tab opens, since a new tab lists something else.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::OpenEachSelected()
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
//  ExplorerFileOps::ShowRowProperties
//
//  A host item, a drive among them, has Windows' own Properties sheet. An
//  entry inside an image has none, so its catalog details are shown instead.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::ShowRowProperties (int row)
{
    Location                           location = m_context.browser.GetLocation();
    std::vector<DxuiListView::Column>  columns  = BrowserCells::GetColumns();
    std::vector<DxuiListView::Cell>    cells;
    std::wstring                       path;
    std::wstring                       text;
    size_t                             i        = 0;
    FileEntry                          entry;
    CassoExplorerBrowser::SearchMatch  match;
    std::wstring                       error;



    //  An entry inside an image or on a linked drive: its Apple properties,
    //  editable.
    if (m_context.browser.IsVolumeLocation() && m_context.browser.TryGetRowEntry (row, entry))
    {
        ShowAppleProperties();
        return;
    }

    if (!m_context.browser.TryGetRowPath (row, path))
    {
        return;
    }

    //  Anything on the disk, a search's result among them, has Windows' sheet.
    if (location.kind == Location::Kind::HostFolder || location.kind == Location::Kind::Root || GetFileAttributesW (path.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        ShowHostProperties (path);
        return;
    }

    //  A search's match inside an image has the sheet it has in its image.
    if (m_context.browser.TryFindSearchMatch (m_context.browser.GetRows()[(size_t) row], match, error))
    {
        CassoExplorerPropertiesDialog::Show (m_context.window.GetHwnd(), m_context.theme, m_context.browser.GetRows()[(size_t) row].name,
                                            CassoExplorerProperties::DescribeEntry (match.entry, match.kind,
                                                BrowserModel::FormatAddress (match.directory.empty() ? Location::MakeDiskImage (match.imagePath)
                                                                                                     : Location::MakeDiskDirectory (match.imagePath, match.directory))));
        return;
    }

    cells = BrowserCells::ToCells (m_context.browser.GetRows()[(size_t) row], location);
    text  = path + L"\n\n";

    for (i = 0; i < columns.size() && i < cells.size(); i++)
    {
        if (!cells[i].text.empty() && !columns[i].title.empty())
        {
            text += columns[i].title + L": " + cells[i].text + L"\n";
        }
    }

    m_context.window.ShowMessage (text, MB_ICONINFORMATION);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ShowAppleProperties
//
//  The selected entries of the open disk, in the Apple file properties
//  dialog. Its edits are written as it applies them, and the list is filled
//  again after each.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::ShowAppleProperties()
{
    std::vector<FileEntry>  entries;
    bool                    readOnly = m_context.browser.IsImageLocation() && m_context.browser.IsWriteProtected();



    m_context.browser.GetSelectedEntries (entries);

    AppleFilePropertiesDialog::Show (m_context.window.GetHwnd(), m_context.theme, entries, m_context.browser.GetVolumeKind(), readOnly,
                                     [this] (const AppleFileProperties::Edits & edits, std::vector<FileEntry> & inOutEntries) -> std::wstring
    {
        CassoExplorerActions::Outcome  outcome = m_context.actions.ApplyProperties (inOutEntries, edits);

        m_context.listPane.FillList();

        //  The dialog's files as the list has them now, so the next Apply
        //  starts from what this one wrote. The list's selection doesn't
        //  survive the refill, so the files go by name.
        for (FileEntry & entry : inOutEntries)
        {
            std::string  name = (edits.nameEdited && inOutEntries.size() == 1) ? TextEncoding::WideToNarrow (edits.name) : entry.name;
            FileEntry    fresh;

            for (int row = 0; row < (int) m_context.browser.GetRows().size(); row++)
            {
                if (m_context.browser.TryGetRowEntry (row, fresh) && _stricmp (fresh.name.c_str(), name.c_str()) == 0)
                {
                    entry = fresh;
                    break;
                }
            }
        }

        return outcome.Succeeded() ? std::wstring() : (outcome.message.empty() ? std::wstring (L"The changes weren't written.") : outcome.message);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::IsHostAppleFile
//
//  A file on the PC whose name is in the Descriptive or CiderPress form, or
//  an AppleSingle container, known here by its .as name; opening the dialog
//  reads it to be sure.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerFileOps::IsHostAppleFile (const std::wstring & path)
{
    std::wstring    leaf   = std::filesystem::path (path).filename().wstring();
    ParsedHostName  parsed;



    if (_wcsicmp (std::filesystem::path (path).extension().wstring().c_str(), L".as") == 0)
    {
        return true;
    }

    return HostFileNaming::Parse (leaf, parsed) && parsed.hasType && parsed.converted == ParsedHostName::ConvertedKind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ShowHostAppleProperties
//
//  An Apple file on the PC in the Apple file properties dialog, in ProDOS's
//  vocabulary. Each Apply rewrites an AppleSingle file and renames the file
//  for its form, then reads it again for the next.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::ShowHostAppleProperties (const std::wstring & path)
{
    IDiskFileIo                             & io      = m_context.browser.GetOperations().GetFileIo();
    std::vector<Byte>                         bytes;
    HostAppleFile                             file;
    AppleFilePropertiesDialog::HostOptions    options;
    std::wstring                              current = path;
    HRESULT                                   hr      = S_OK;



    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (path), bytes);

    if (SUCCEEDED (hr))
    {
        file = HostAppleFile::Read (std::filesystem::path (path).filename().wstring(), bytes);
    }

    if (file.form == HostAppleFile::Form::None)
    {
        m_context.window.ShowMessage (std::filesystem::path (path).filename().wstring() + L" isn't an Apple file: its name doesn't record a type, and it isn't AppleSingle.",
                                      MB_ICONINFORMATION);
        return;
    }

    options.showAccess  = file.form == HostAppleFile::Form::AppleSingle;
    options.showDates   = file.form == HostAppleFile::Form::AppleSingle;
    options.offerRename = file.form == HostAppleFile::Form::AppleSingle;
    options.nameKind    = VolumeKind::Dos33;
    options.leafName    = file.leafName;
    options.isTaken     = [folder = std::filesystem::path (path).parent_path()] (const std::wstring & leaf)
    {
        return GetFileAttributesW ((folder / leaf).c_str()) != INVALID_FILE_ATTRIBUTES;
    };

    AppleFilePropertiesDialog::Show (m_context.window.GetHwnd(), m_context.theme, { file.entry }, VolumeKind::ProDos, false,
                                     [this, &io, &file, &current] (const AppleFileProperties::Edits & edits, std::vector<FileEntry> & inOutEntries) -> std::wstring
    {
        HostAppleFile::Change  change;
        std::wstring           target;
        std::vector<Byte>      fresh;
        HRESULT                result = file.Apply (edits, edits.renameHostFile, change);

        if (result == HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED))
        {
            return L"This AppleSingle file has parts, such as a resource fork, that rewriting it would drop, so it can't be changed here.";
        }

        if (SUCCEEDED (result) && !change.bytes.empty())
        {
            result = io.WriteAllBytes (TextEncoding::WideToNarrow (current), change.bytes);
        }

        target = (std::filesystem::path (current).parent_path() / change.leafName).wstring();

        if (SUCCEEDED (result) && _wcsicmp (target.c_str(), current.c_str()) != 0)
        {
            result = MoveFileExW (current.c_str(), target.c_str(), 0) ? S_OK : HRESULT_FROM_WIN32 (GetLastError());

            if (SUCCEEDED (result))
            {
                current = target;
            }
        }

        m_context.folderSync.RefreshAfterHostChange();

        if (FAILED (result))
        {
            return L"The changes weren't written: " + TextEncoding::NarrowToWide (std::system_category().message (HRESULT_CODE (result)));
        }

        result = io.ReadAllBytes (TextEncoding::WideToNarrow (current), fresh);

        if (SUCCEEDED (result))
        {
            file         = HostAppleFile::Read (std::filesystem::path (current).filename().wstring(), fresh);
            inOutEntries = { file.entry };
        }

        return std::wstring();
    }, &options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ShowLocationProperties
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::ShowLocationProperties (const Location & location)
{
    //  Home and the roots have no host path to show properties for.
    if (location.kind == Location::Kind::None || location.kind == Location::Kind::Root)
    {
        return;
    }

    if (location.kind == Location::Kind::DiskDirectory)
    {
        m_context.window.ShowMessage (BrowserModel::FormatAddress (location), MB_ICONINFORMATION);
        return;
    }

    ShowHostProperties (location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ShowHostProperties
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::ShowHostProperties (const std::wstring & path)
{
    BOOL  shown = SHObjectProperties (m_context.window.GetHwnd(), SHOP_FILEPATH, path.c_str(), nullptr);



    IGNORE_RETURN_VALUE (shown, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ShowHostProperties
//
//  Open Windows' single Properties sheet for several items, as File Explorer
//  does for a selection.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::ShowHostProperties (const std::vector<std::wstring> & paths)
{
    HRESULT                            hr = S_OK;
    std::vector<PIDLIST_ABSOLUTE>      ids;
    ComPtr<IShellItemArray>            items;
    ComPtr<IDataObject>                data;



    for (const std::wstring & path : paths)
    {
        PIDLIST_ABSOLUTE  id = nullptr;

        hr = SHParseDisplayName (path.c_str(), nullptr, &id, 0, nullptr);
        CHR (hr);

        ids.push_back (id);
    }

    hr = SHCreateShellItemArrayFromIDLists ((UINT) ids.size(), (PCIDLIST_ABSOLUTE_ARRAY) ids.data(), &items);
    CHR (hr);

    hr = items->BindToHandler (nullptr, BHID_DataObject, IID_PPV_ARGS (&data));
    CHR (hr);

    hr = SHMultiFileProperties (data.Get(), 0);
    CHR (hr);

Error:
    for (PIDLIST_ABSOLUTE id : ids)
    {
        CoTaskMemFree (id);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
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
        case UndoStep::Kind::ImageDelete:    return L"&Undo delete";
        case UndoStep::Kind::HostRename:
        case UndoStep::Kind::ImageRename:    return L"&Undo rename";
        case UndoStep::Kind::HostNewFolder:
        case UndoStep::Kind::ImageNewFolder: return L"&Undo new folder";
        case UndoStep::Kind::HostNewItem:    return L"&Undo new";
        case UndoStep::Kind::HostCopy:
        case UndoStep::Kind::ImagePut:       return L"&Undo copy";
        case UndoStep::Kind::HostMove:       return L"&Undo move";
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
            m_restoreTicks = kRestoreRetries;
            SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerRestore, 1000, nullptr);
            break;

        case UndoStep::Kind::HostRename:
            hr = m_context.shellVerbs.RenameItem (m_context.window.GetHwnd(), step.paths[0], step.oldName);
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





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::FindCassoTarget
//
//  The Casso that launched this browser while it is still there, otherwise
//  any running Casso, otherwise none.
//
////////////////////////////////////////////////////////////////////////////////

HWND ExplorerFileOps::FindCassoTarget() const
{
    std::vector<HWND>  running;
    HWND               window  = nullptr;
    CassoTarget        target;



    //  Top-level windows come back front to back, so the first Casso found
    //  is the one most recently active.
    while ((window = FindWindowExW (nullptr, window, Win32IntentChannel::kWindowClass, nullptr)) != nullptr)
    {
        running.push_back (window);
    }

    target = CassoTargeting::Choose (m_context.app.request.owner, m_context.app.request.owner != nullptr && IsWindow (m_context.app.request.owner) != FALSE,
                                     running, MachineConfig());

    return target.hwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::GetDefaultMachineDriveCount
//
//  The drives of the machine a new Casso would start, chosen as Casso chooses
//  it: the machine last selected, else the Apple //e. Read once, from its
//  configuration alone; its ROMs are not needed to count its drives. Both
//  drives when the configuration cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerFileOps::GetDefaultMachineDriveCount()
{
    constexpr std::wstring_view  s_kPreferredDefaultMachine = L"Apple2e";
    HRESULT                hr          = S_OK;
    GlobalUserPrefs        prefs;
    Win32FileSystem        files;
    std::wstring           machine;
    std::vector<fs::path>  searchPaths;
    fs::path               configPath;
    std::string            jsonText;
    std::string            error;
    MachineConfig          config;
    int                    drives      = 0;



    BAIL_OUT_IF (m_defaultDriveCount >= 0, S_OK);

    m_defaultDriveCount = Win32IntentChannel::kMaxDriveCount;

    hr = prefs.Load (AssetBootstrap::GetAssetBaseDirectory().wstring(), files);
    IGNORE_RETURN_VALUE (hr, S_OK);

    searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(), PathResolver::GetWorkingDirectory());
    machine     = TextEncoding::NarrowToWide (prefs.lastSelectedMachine);
    machine     = MachineScanner::SelectCanonical (MachineScanner::Scan (searchPaths, &MachineScanner::ListDirectory, &MachineScanner::ReadFile),
                                                   machine, s_kPreferredDefaultMachine);
    configPath  = PathResolver::FindFile (searchPaths, fs::path ("Machines") / fs::path (machine) / (fs::path (machine).string() + ".json"));
    BAIL_OUT_IF (configPath.empty(), S_OK);

    hr = files.ReadAllText (configPath.wstring(), jsonText);
    CHR (hr);

    hr = MergeUserMachineDelta (TextEncoding::WideToNarrow (machine), files, jsonText);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  Only the slots are read here, never a ROM's bytes, so a ROM that is
    //  missing stands in as the config file itself rather than stopping the
    //  slots from being read; one that is there is found where Casso finds it.
    hr = MachineConfigLoader::Load (jsonText, fs::path (machine).string(), searchPaths,
                                    [&configPath] (const std::vector<fs::path> & paths, const fs::path & relative)
                                    {
                                        fs::path  found = PathResolver::FindFile (paths, relative);

                                        return found.empty() ? configPath : found;
                                    },
                                    config, error);
    CHR (hr);

    drives = config.AttachedDiskIiDriveCount();

    if (drives > 0)
    {
        m_defaultDriveCount = drives;
    }

Error:
    return m_defaultDriveCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::MergeUserMachineDelta
//
//  The user's changes to the machine, such as a drive detached in Casso's
//  settings, merged into its configuration as Casso merges them. The prefs
//  file is only read: a delta Casso would migrate is merged as it is, which
//  is also Casso's own fallback when a migration fails.
//
//  A machine's configuration with the user's changes to it from Casso's
//  prefs file merged in, as Casso merges them; the file is only read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerFileOps::MergeUserMachineDelta (const std::string & machine, IFileSystem & files, std::string & inoutJson)
{
    HRESULT            hr       = S_OK;
    UserConfigStore    store (AssetBootstrap::GetAssetBaseDirectory().wstring());
    std::string        prefsText;
    JsonValue          base;
    JsonValue          prefs;
    JsonParseError     error;
    const JsonValue  * machines = nullptr;
    const JsonValue  * delta    = nullptr;



    hr = files.ReadAllText (store.GetUserPrefsFilePath(), prefsText);
    CHR (hr);

    hr = JsonParser::Parse (prefsText, prefs, error);
    CHR (hr);

    hr = JsonParser::Parse (inoutJson, base, error);
    CHR (hr);

    BAIL_OUT_IF (!prefs.HasObject ("machines", machines), S_OK);
    BAIL_OUT_IF (!machines->HasObject (machine, delta), S_OK);

    inoutJson = JsonWriter::Write (UserConfigStore::MergeJson (base, *delta));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::AskCassoToDescribe
//
//  The answer arrives as a reply message and updates the drive count the
//  menus read. With no Casso running, a new one opens with the default
//  machine, so its configuration says how many drives there are.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::AskCassoToDescribe()
{
    HWND  target = FindCassoTarget();
    bool  sent   = false;



    if (target == nullptr)
    {
        m_cassoDriveCount = GetDefaultMachineDriveCount();
        return;
    }

    sent = Win32IntentChannel::SendTo (target, m_context.window.GetHwnd(), Win32IntentChannel::GetMessageId(), Win32IntentChannel::EncodeDescribe());
    IGNORE_RETURN_VALUE (sent, true);
}
