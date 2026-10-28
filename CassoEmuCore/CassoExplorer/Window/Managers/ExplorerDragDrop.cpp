#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "Devices/Disk/DiskImageStore.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::ExplorerDragDrop
//
////////////////////////////////////////////////////////////////////////////////

ExplorerDragDrop::ExplorerDragDrop (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::SetIconDpi
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::SetIconDpi (UINT dpi)
{
    m_dragIcons.SetDpi    (dpi);
    m_dragIcons.SetSizePx (MulDiv (kDragImageDip, (int) dpi, (int) DxuiDpiScaler::kBaseDpi));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::BeginTreeDrag
//
//  The tree received the press and not the release, which the drag loop takes.
//
//  A drag from a node in the tree: a linked Apple's whole drive, or a
//  folder or disk image on the PC, which drags as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::BeginTreeDrag (const Location & location)
{
    HRESULT  hr = S_OK;



    m_context.treePane.ReleasePress();

    //  A disk image has Casso's floppy, which the shell has no icon for.
    if (location.kind != Location::Kind::AppleDrive)
    {
        hr = m_context.shellVerbs.DragItems ({ location.path }, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK,
                                              (location.kind == Location::Kind::DiskImage) ? m_dragIcons.GetForKind (IShellIcons::Kind::DiskImage).get() : nullptr);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_context.folderSync.RefreshAfterHostChange();
        m_context.window.Invalidate();
        return;
    }

    BeginDiskDrag (location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::BeginDiskDrag
//
//  A linked Apple's whole drive, as one image file, from its node in the tree
//  or its row in the list.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::BeginDiskDrag (const std::wstring & driveId)
{
    HRESULT                                  hr      = S_OK;
    DWORD                                    effect  = DROPEFFECT_NONE;
    POINT                                    start   = {};
    std::vector<DxuiDragDropSource::Format>  formats = CassoExplorerDragOut::BuildDiskFormats (m_context.browser, driveId);
    BOOL                                     got     = FALSE;



    if (formats.empty())
    {
        return;
    }

    got = GetCursorPos (&start);
    IGNORE_RETURN_VALUE (got, TRUE);

    hr = DxuiDragDropSource::Begin (std::move (formats), DROPEFFECT_COPY, start, effect, nullptr, m_dragIcons.GetForKind (IShellIcons::Kind::DiskImage).get());
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::GetRowDragImage
//
//  The first selected row's icon at the drag image's size.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> ExplorerDragDrop::GetRowDragImage()
{
    const std::vector<int>  & selected = m_context.browser.GetSelectedRows();
    bool                      found    = !selected.empty() && selected[0] >= 0 && (size_t) selected[0] < m_context.browser.GetRows().size();



    return found ? BrowserCells::GetRowIcon (m_context.browser.GetRows()[(size_t) selected[0]], m_context.browser.GetLocation(), m_dragIcons) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::TryGetAppleDropDrive
//
//  The linked Apple's drive a drop would land on: a drive's node or row,
//  or the drive in the list.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::TryGetAppleDropDrive (int tag, POINT screen, Location & outDrive)
{
    POINT                 client = screen;
    int                   row    = -1;
    const DxuiTreeNode  * node   = nullptr;
    Location              location;



    ScreenToClient (m_context.window.GetHwnd(), &client);

    if (tag == CassoExplorerWindow::kDropTagTree)
    {
        row  = m_context.treePane.HitTestRow (client);
        node = (row >= 0) ? m_context.treePane.GetNodeAt (row) : nullptr;

        if (node != nullptr && m_context.browser.TryGetNodeLocation (node->id, location) && location.kind == Location::Kind::AppleDrive)
        {
            outDrive = location;
            return true;
        }

        return false;
    }

    if (tag != CassoExplorerWindow::kDropTagList)
    {
        return false;
    }

    row = m_context.listPane.IsVisible() ? m_context.listPane.HitTestRow (client) : -1;

    if (row >= 0 && m_context.browser.TryGetRowLocation (row, location) && location.kind == Location::Kind::AppleDrive)
    {
        outDrive = location;
        return true;
    }

    if (m_context.browser.IsAppleDriveLocation())
    {
        outDrive = m_context.browser.GetLocation();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::TryGetDropLocation
//
//  On the list: the image folder under the pointer, or the image the list
//  shows. On the tree: the image or directory node under the pointer. A host
//  folder takes no drop here; Explorer is for that.
//
//  Drag-in: the image location under a drop point, what a drop there
//  would do, and the drop.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::TryGetDropLocation (int tag, POINT screen, Location & outLocation)
{
    POINT                 client = screen;
    int                   row    = -1;
    const DxuiTreeNode  * node   = nullptr;



    ScreenToClient (m_context.window.GetHwnd(), &client);

    if (tag == CassoExplorerWindow::kDropTagTree)
    {
        row  = m_context.treePane.HitTestRow (client);
        node = (row >= 0) ? m_context.treePane.GetNodeAt (row) : nullptr;

        //  A root holds drives and known folders, not files, so it takes no drop.
        if (node == nullptr || !m_context.browser.TryGetNodeLocation (node->id, outLocation) || outLocation.kind == Location::Kind::Root)
        {
            return false;
        }
    }
    else
    {
        row = m_context.listPane.HitTestRow (client);

        //  A disk image row in a folder, or a directory row in an image, takes
        //  the drop itself; anywhere else in the list, the location shown does.
        if (row < 0 || !m_context.browser.TryGetRowLocation (row, outLocation) ||
            (outLocation.kind != Location::Kind::DiskDirectory && outLocation.kind != Location::Kind::DiskImage))
        {
            outLocation = m_context.browser.GetLocation();
        }
    }

    return outLocation.kind == Location::Kind::DiskImage || outLocation.kind == Location::Kind::DiskDirectory;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::TryGetHostDropFolder
//
//  A folder row or tree node on the host, or, on the list's empty space, the
//  host folder the list shows.
//
//  A host folder under the drag takes it as Explorer would: the drag goes
//  to the folder's own shell drop target, which copies or moves it with
//  Explorer's progress, conflict and undo handling.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::TryGetHostDropFolder (int tag, POINT screen, std::wstring & outFolder)
{
    POINT                 client = screen;
    int                   row    = -1;
    const DxuiTreeNode  * node   = nullptr;
    Location              location;



    ScreenToClient (m_context.window.GetHwnd(), &client);

    if (tag == CassoExplorerWindow::kDropTagTree)
    {
        row  = m_context.treePane.HitTestRow (client);
        node = (row >= 0) ? m_context.treePane.GetNodeAt (row) : nullptr;

        if (node == nullptr || !m_context.browser.TryGetNodeLocation (node->id, location))
        {
            return false;
        }
    }
    else
    {
        row = m_context.listPane.IsVisible() ? m_context.listPane.HitTestRow (client) : -1;

        //  An archive takes a drop as a folder does, and a program opens with
        //  what is dropped on it, each through the shell's own target.
        if (IsShellDropFile (row, outFolder))
        {
            return true;
        }

        //  A disk image's row takes the drop itself, so its folder is not the
        //  target; a drop on a file row that takes none goes to the listed folder.
        if (row >= 0 && m_context.browser.TryGetRowLocation (row, location) && location.kind != Location::Kind::HostFolder && location.kind != Location::Kind::RecycleBin)
        {
            return false;
        }

        if (row < 0 || (location.kind != Location::Kind::HostFolder && location.kind != Location::Kind::RecycleBin))
        {
            location = m_context.browser.GetLocation();
        }
    }

    //  The Recycle Bin's own target recycles what is dropped on it, as a drop
    //  on Explorer's Recycle Bin does.
    if (location.kind == Location::Kind::RecycleBin)
    {
        outFolder = L"shell:RecycleBinFolder";
        return true;
    }

    if (location.kind != Location::Kind::HostFolder || location.path.empty())
    {
        return false;
    }

    outFolder = location.path;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::IsShellDropFile
//
//  A file in the list that the shell's own drop target handles: an archive,
//  which takes a copy of what is dropped, and a program, which opens with
//  the dropped files as its arguments.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::IsShellDropFile (int row, std::wstring & outPath) const
{
    std::wstring  extension;



    if (row < 0 || (size_t) row >= m_context.browser.GetRows().size() || m_context.browser.GetLocation().kind != Location::Kind::HostFolder)
    {
        return false;
    }

    const CatalogRow &  item = m_context.browser.GetRows()[(size_t) row];

    if (item.isDirectory || item.isDiskImage || !m_context.browser.TryGetRowPath (row, outPath))
    {
        return false;
    }

    extension = std::filesystem::path (outPath).extension().wstring();

    return item.isArchive || IsProgramPath (outPath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::IsProgramPath
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::IsProgramPath (const std::wstring & path)
{
    std::wstring  extension = std::filesystem::path (path).extension().wstring();



    return std::any_of (std::begin (kProgramExtensions), std::end (kProgramExtensions),
                        [&extension] (const wchar_t * known) { return _wcsicmp (extension.c_str(), known) == 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::GetDraggedPaths
//
//  The paths in a drag's file list; empty when the drag has no file list.
//
//  The files in a drag, and whether a drop on a folder or item would
//  put them back where they are or onto themselves.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerDragDrop::GetDraggedPaths (IDataObject * data)
{
    FORMATETC                  format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM                  medium = {};
    HDROP                      drop   = nullptr;
    UINT                       count  = 0;
    UINT                       length = 0;
    HRESULT                    hr     = E_FAIL;
    std::wstring               path;
    std::vector<std::wstring>  paths;



    if (data != nullptr)
    {
        hr = data->GetData (&format, &medium);
    }

    if (FAILED (hr))
    {
        return paths;
    }

    drop  = (HDROP) GlobalLock (medium.hGlobal);
    count = (drop != nullptr) ? DragQueryFileW (drop, 0xFFFFFFFF, nullptr, 0) : 0;

    //  A path can be longer than MAX_PATH, so each one's length comes first.
    for (UINT index = 0; index < count; index++)
    {
        length = DragQueryFileW (drop, index, nullptr, 0);
        path.assign ((size_t) length + 1, L'\0');

        if (length > 0 && DragQueryFileW (drop, index, path.data(), length + 1) == length)
        {
            path.resize (length);
            paths.push_back (path);
        }
    }

    if (drop != nullptr)
    {
        GlobalUnlock (medium.hGlobal);
    }

    ReleaseStgMedium (&medium);

    return paths;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::IsDraggedFromFolder
//
//  Whether every file in the drag lies directly in the folder, so that a
//  drop there would move each onto itself.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::IsDraggedFromFolder (IDataObject * data, const std::wstring & folder)
{
    std::vector<std::wstring>  paths = GetDraggedPaths (data);



    return !paths.empty() && std::all_of (paths.begin(), paths.end(), [&folder] (const std::wstring & path)
    {
        return ExplorerFolderSync::IsSameFolder (CassoExplorerBrowser::GetParentFolder (path), folder);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::IsDraggedItem
//
//  Whether the target is one of the files or folders in the drag, a drop
//  File Explorer does not allow: a folder into itself, or a disk image, archive
//  or program onto itself.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::IsDraggedItem (IDataObject * data, const std::wstring & target)
{
    std::vector<std::wstring>  paths = GetDraggedPaths (data);



    return !target.empty() && std::any_of (paths.begin(), paths.end(), [&target] (const std::wstring & path)
    {
        return ExplorerFolderSync::IsSameFolder (path, target);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::ForwardHostDrag
//
//  Enters the folder's target when the drag first reaches it, leaving the
//  one before; over it after that.
//
////////////////////////////////////////////////////////////////////////////////

DWORD ExplorerDragDrop::ForwardHostDrag (IDataObject * data, const std::wstring & folder, POINT screen)
{
    HRESULT             hr     = S_OK;
    ComPtr<IShellItem>  item;
    DWORD               effect = m_dropTarget.GetAllowedEffects();
    POINTL              at     = { screen.x, screen.y };



    if (m_hostDrop != nullptr && ExplorerFolderSync::IsSameFolder (folder, m_hostDropFolder))
    {
        hr = m_hostDrop->DragOver (m_dropTarget.GetDragKeyState(), at, &effect);

        return SUCCEEDED (hr) ? effect : DROPEFFECT_NONE;
    }

    LeaveHostDrop();

    hr = SHCreateItemFromParsingName (folder.c_str(), nullptr, IID_PPV_ARGS (&item));

    if (SUCCEEDED (hr))
    {
        hr = item->BindToHandler (nullptr, BHID_SFUIObject, IID_PPV_ARGS (&m_hostDrop));
    }

    if (FAILED (hr) || m_hostDrop == nullptr)
    {
        m_hostDrop.Reset();
        return DROPEFFECT_NONE;
    }

    m_hostDropFolder = folder;

    hr = m_hostDrop->DragEnter (data, m_dropTarget.GetDragKeyState(), at, &effect);

    return SUCCEEDED (hr) ? effect : DROPEFFECT_NONE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::LeaveHostDrop
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::LeaveHostDrop()
{
    if (m_hostDrop != nullptr)
    {
        HRESULT  hr = m_hostDrop->DragLeave();

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_hostDrop.Reset();
    m_hostDropFolder.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::GetDropEffect
//
//  What a drop at this point would do, so the pointer and the words under the
//  drag image say so before the button is let go. A tab takes no drop, but
//  one the drag rests on opens, so the drop can go into its list.
//
////////////////////////////////////////////////////////////////////////////////

DWORD ExplorerDragDrop::GetDropEffect (IDataObject * data, int tag, POINT screen)
{
    Location      location;
    DropSource    source;
    std::wstring  hostFolder;
    std::wstring  driveId;
    bool          readOnly = false;
    HRESULT       hr       = S_OK;
    DWORD         effect   = DROPEFFECT_NONE;



    if (tag == CassoExplorerWindow::kDropTagTabs)
    {
        HoverDropTab (screen);
        ShowDropTarget (tag, screen, false);
        DescribeDrop (data, DROPEFFECT_NONE, location);
        return DROPEFFECT_NONE;
    }

    if (data != nullptr && TryGetHostDropFolder (tag, screen, hostFolder)
        && (IsDraggedFromFolder (data, hostFolder) || IsDraggedItem (data, hostFolder)))
    {
        LeaveHostDrop();
        ShowDropTarget (tag, screen, false);
        DescribeDrop   (data, DROPEFFECT_NONE, location);

        return DROPEFFECT_NONE;
    }

    //  A linked Apple's whole drive, onto a host folder: its image is made
    //  there by Casso Explorer, with no waiting in the drop.
    if (data != nullptr && CassoExplorerDragOut::TryGetDraggedDrive (data, driveId))
    {
        LeaveHostDrop();

        effect = (TryGetHostDropFolder (tag, screen, hostFolder) && !hostFolder.starts_with (L"shell:")) ? DROPEFFECT_COPY : DROPEFFECT_NONE;

        ShowDropTarget (tag, screen, effect != DROPEFFECT_NONE);
        return effect;
    }

    //  Onto a linked Apple's drive: an image is written to the whole disk, and
    //  files are copied in.
    if (data != nullptr && TryGetAppleDropDrive (tag, screen, location) && ReadDropSource (data, source))
    {
        LeaveHostDrop();
        ShowDropTarget (tag, screen, true);
        return DROPEFFECT_COPY;
    }


    if (data != nullptr && TryGetHostDropFolder (tag, screen, hostFolder))
    {
        effect = ForwardHostDrag (data, hostFolder, screen);
        ShowDropTarget (tag, screen, effect != DROPEFFECT_NONE);

        return effect;
    }

    LeaveHostDrop();

    if (data != nullptr && TryGetDropLocation (tag, screen, location) && ReadDropSource (data, source))
    {
        if (m_context.app.fs != nullptr)
        {
            hr = m_context.app.fs->GetReadOnlyAttribute (location.path, readOnly);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        effect = (readOnly || IsDraggedItem (data, location.path)) ? DROPEFFECT_NONE : ChooseDropEffect (source, location);
    }

    DescribeDrop   (data, effect, location);
    ShowDropTarget (tag, screen, effect != DROPEFFECT_NONE);

    return effect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::ReadDropSource
//
//  Another image's entries when the drag holds them, host files otherwise.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::ReadDropSource (IDataObject * data, DropSource & outSource)
{
    FORMATETC  entries = { (CLIPFORMAT) RegisterClipboardFormatA (DragPayload::kPrivateFormatName), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM  medium  = {};
    HRESULT    hr      = S_OK;



    outSource = DropSource();

    if (data == nullptr)
    {
        return false;
    }

    hr = data->GetData (&entries, &medium);

    if (SUCCEEDED (hr))
    {
        const char  * bytes = (const char *) GlobalLock (medium.hGlobal);
        size_t        size  = GlobalSize (medium.hGlobal);

        if (bytes != nullptr)
        {
            outSource.fromImage = DragPayload::DecodeCatalogEntries (std::string (bytes, strnlen (bytes, size)),
                                                                     outSource.image, outSource.kind, outSource.catalogPaths);
            GlobalUnlock (medium.hGlobal);
        }

        ReleaseStgMedium (&medium);
    }

    if (outSource.fromImage)
    {
        return true;
    }

    hr = DxuiDragDropTarget::ExtractHDropPaths (data, outSource.hostPaths);

    return SUCCEEDED (hr) && !outSource.hostPaths.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::ChooseDropEffect
//
//  A disk image is a volume of its own, so entries moved within one image
//  move, and anything coming from elsewhere is copied, unless Ctrl or Shift
//  says otherwise. A drop that would go nowhere is none: an image onto
//  itself, or entries moved into the directory they are already in or into
//  one of themselves.
//
////////////////////////////////////////////////////////////////////////////////

DWORD ExplorerDragDrop::ChooseDropEffect (const DropSource & source, const Location & target) const
{
    bool         same   = source.fromImage && ExplorerFolderSync::IsSameFolder (TextEncoding::NarrowToWide (source.image), target.path);
    std::string  into   = (target.kind == Location::Kind::DiskDirectory) ? target.innerPath : std::string();
    DWORD        effect = DragPayload::ChooseDropEffect (same, m_dropTarget.GetDragKeyState(), m_dropTarget.GetAllowedEffects());



    for (const std::wstring & path : source.hostPaths)
    {
        if (ExplorerFolderSync::IsSameFolder (path, target.path))
        {
            return DROPEFFECT_NONE;
        }
    }

    for (const std::string & path : (same ? source.catalogPaths : std::vector<std::string>()))
    {
        size_t       slash  = path.find_last_of ('/');
        std::string  parent = (slash == std::string::npos) ? std::string() : path.substr (0, slash);

        if (into == path || into.rfind (path + "/", 0) == 0)
        {
            return DROPEFFECT_NONE;
        }

        if (effect == DROPEFFECT_MOVE && parent == into)
        {
            return DROPEFFECT_NONE;
        }
    }

    return effect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::DescribeDrop
//
//  The words under the drag image, as Explorer's: "Move to Disks", "Copy to
//  GAMES". None where the drop would do nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::DescribeDrop (IDataObject * data, DWORD effect, const Location & target)
{
    HRESULT       hr    = S_OK;
    std::wstring  place;
    size_t        slash = 0;



    if (data == nullptr)
    {
        return;
    }

    if (effect == DROPEFFECT_NONE)
    {
        hr = DxuiDragDropTarget::SetDropDescription (data, DROPIMAGE_INVALID, L"", L"");
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    if (target.kind == Location::Kind::DiskDirectory && !target.innerPath.empty())
    {
        slash = target.innerPath.find_last_of ('/');
        place = TextEncoding::NarrowToWide ((slash == std::string::npos) ? target.innerPath : target.innerPath.substr (slash + 1));
    }
    else
    {
        slash = target.path.find_last_of (L"\\/");
        place = (slash == std::wstring::npos) ? target.path : target.path.substr (slash + 1);
    }

    hr = DxuiDragDropTarget::SetDropDescription (data,
                                                 (effect == DROPEFFECT_MOVE) ? DROPIMAGE_MOVE : DROPIMAGE_COPY,
                                                 (effect == DROPEFFECT_MOVE) ? L"Move to %1" : L"Copy to %1",
                                                 place.c_str());
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::HoverDropTab
//
//  The tab under the drag opens at once, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::HoverDropTab (POINT screen)
{
    POINT  client = screen;
    int    index  = -1;



    ScreenToClient (m_context.window.GetHwnd(), &client);
    index = m_context.tabManager.HitTest (client);

    if (index >= 0 && (size_t) index != m_context.browser.GetBrowserModel().GetActiveIndex())
    {
        m_context.tabManager.SwitchToTab ((size_t) index);
        m_context.window.Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::ShowDropTarget
//
//  The row or tree node a drop would land on is lit as the pointer moves, as
//  Explorer lights its drop target. A drop on the list's empty space goes to
//  the location shown, so nothing is lit there. Nothing is redrawn while the
//  target stays the same, since OLE asks many times a second.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::ShowDropTarget (int tag, POINT screen, bool accepted)
{
    POINT     client   = screen;
    Location  location;
    int       listRow  = -1;
    int       treeRow  = -1;
    int       row      = -1;



    ScreenToClient (m_context.window.GetHwnd(), &client);

    if (accepted && tag == CassoExplorerWindow::kDropTagTree)
    {
        treeRow = m_context.treePane.HitTestRow (client);
    }
    else if (accepted && tag == CassoExplorerWindow::kDropTagList)
    {
        row = m_context.listPane.HitTestRow (client);

        std::wstring  target;

        if (row >= 0 && ((m_context.browser.TryGetRowLocation (row, location) &&
                          (location.kind == Location::Kind::DiskDirectory || location.kind == Location::Kind::DiskImage
                           || location.kind == Location::Kind::HostFolder))
                         || IsShellDropFile (row, target)))
        {
            listRow = row;
        }
    }

    //  A collapsed node the drag rests on for a second opens, as in File
    //  Explorer's navigation pane.
    if (treeRow >= 0 && treeRow == m_treeDropRow && GetTickCount64() - m_treeDropSinceMs >= kTreeDropExpandMs)
    {
        m_context.treePane.ExpandRowForDrop (treeRow);
    }

    if (listRow == m_context.listPane.GetDropRow() && treeRow == m_treeDropRow)
    {
        return;
    }

    m_context.listPane.SetDropRow (listRow);
    m_treeDropRow     = treeRow;
    m_treeDropSinceMs = GetTickCount64();
    m_context.treePane.SetDropRow (treeRow);

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::ClearDropTarget
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::ClearDropTarget()
{
    LeaveHostDrop();

    if (m_context.listPane.GetDropRow() < 0 && m_treeDropRow < 0)
    {
        return;
    }

    m_context.listPane.SetDropRow (-1);
    m_context.treePane.ClearDropTarget();
    m_treeDropRow = -1;

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::HandleDrop
//
//  Another image's entries are copied byte for byte when the drag holds
//  them; otherwise the host files go in by Put's rules. A move removes what
//  it copied once everything has landed: entries from their image, and host
//  files to the Recycle Bin. The whole move happens here, so the source is
//  told of a copy and removes nothing itself.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::HandleDrop (IDataObject * data, int tag, POINT screen)
{
    Location                       location;
    DropSource                     source;
    VolumeKind                     targetKind = VolumeKind::Unknown;
    std::string                    inner;
    VolumeListing                  listing;
    CassoExplorerActions::Outcome  outcome;
    CassoExplorerActions::Outcome  removed;
    DWORD                          effect     = DROPEFFECT_NONE;
    BOOL                           posted     = FALSE;
    HRESULT                        recycled   = S_OK;
    std::wstring                   hostFolder;
    std::wstring                   driveId;
    POINTL                         at         = { screen.x, screen.y };



    //  Back onto the folder the files came from, or onto one of them: nothing
    //  to do.
    if (TryGetHostDropFolder (tag, screen, hostFolder)
        && (IsDraggedFromFolder (data, hostFolder) || IsDraggedItem (data, hostFolder)))
    {
        m_dropTarget.SetDropResult (DROPEFFECT_NONE);
        return;
    }


    //  A linked Apple's whole drive onto a host folder: its image, made there.
    //  AHEAD OF THE FOLDER'S OWN TARGET, which would request the image's bytes
    //  and wait on the UI thread while the whole disk came over the link.
    if (CassoExplorerDragOut::TryGetDraggedDrive (data, driveId) && TryGetHostDropFolder (tag, screen, hostFolder) && !hostFolder.starts_with (L"shell:"))
    {
        std::wstring  link;
        int           slot  = 0;
        int           drive = 0;

        LeaveHostDrop();
        m_dropTarget.SetDropResult (DROPEFFECT_COPY);

        if (TreeModel::TryParseLinkId (driveId, link, slot, drive))
        {
            outcome = m_context.actions.CreateImageFromAppleDrive (TextEncoding::WideToNarrow (link), drive, hostFolder);

            if (!outcome.Succeeded())
            {
                m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
            }

            m_context.cassoLinkManager.HandleConnectionChanged();
        }

        return;
    }

    //  Onto a host folder, the folder's own target does the drop.
    if (m_hostDrop != nullptr && TryGetHostDropFolder (tag, screen, hostFolder) && ExplorerFolderSync::IsSameFolder (hostFolder, m_hostDropFolder))
    {
        effect = m_dropTarget.GetAllowedEffects();

        HRESULT  hr = m_hostDrop->Drop (data, m_dropTarget.GetDragKeyState(), at, &effect);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_dropTarget.SetDropResult (effect);

        m_hostDrop.Reset();
        m_hostDropFolder.clear();
        return;
    }

    //  Onto a linked Apple's drive: one disk image dropped on the drive itself
    //  is written to the whole disk; files, or an image's files, are copied
    //  in. Each write goes over the link, and is reported when it is done.
    if (TryGetAppleDropDrive (tag, screen, location) && ReadDropSource (data, source))
    {
        DiskFormat  format = DiskFormat::Dsk;
        bool        single = !source.fromImage && source.hostPaths.size() == 1 && location.innerPath.empty();
        HRESULT     known  = single ? DiskImageStore::GetSourceFormatByExtension (TextEncoding::WideToNarrow (source.hostPaths[0]), format) : E_FAIL;
        bool        image  = single && SUCCEEDED (known);

        LeaveHostDrop();
        m_dropTarget.SetDropResult (DROPEFFECT_COPY);

        if (image)
        {
            m_context.cassoLinkManager.StartImageWrite (location, source.hostPaths[0]);
            return;
        }

        if (m_context.browser.GetLocation().path != location.path || m_context.browser.GetLocation().innerPath != location.innerPath)
        {
            m_context.addressBar.GoToLocation (location);
        }

        outcome = source.fromImage ? m_context.actions.CopyEntriesToAppleDrive (source.image, source.kind, source.catalogPaths)
                                   : m_context.actions.PutFiles (source.hostPaths, m_context.fileOps.MakeAddressPrompt());

        m_context.commandManager.ReportOutcome (outcome, L"Put");
        return;
    }

    if (!TryGetDropLocation (tag, screen, location) || !ReadDropSource (data, source))
    {
        return;
    }

    effect = IsDraggedItem (data, location.path) ? DROPEFFECT_NONE : ChooseDropEffect (source, location);

    if (effect == DROPEFFECT_NONE)
    {
        return;
    }

    m_dropTarget.SetDropResult (DROPEFFECT_COPY);

    inner = (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string();

    //  What the target is, by listing where the drop goes.
    if (!m_context.browser.GetOperations().List (TextEncoding::WideToNarrow (location.path), inner, listing, targetKind).Succeeded())
    {
        m_context.window.ShowMessage (L"The disk image could not be read.", MB_ICONWARNING);
        return;
    }

    //  A right-drag asks what the drop means, once this call has returned and
    //  the drag itself is over.
    if (m_dropTarget.IsRightDrag())
    {
        m_pendingDrop              = PendingDrop();
        m_pendingDrop.valid        = true;
        m_pendingDrop.location     = location;
        m_pendingDrop.targetKind   = targetKind;
        m_pendingDrop.inner        = inner;
        m_pendingDrop.screen       = screen;
        m_pendingDrop.hostPaths    = source.hostPaths;
        m_pendingDrop.fromImage    = source.fromImage;
        m_pendingDrop.sourceImage  = source.image;
        m_pendingDrop.sourceKind   = source.kind;
        m_pendingDrop.catalogPaths = source.catalogPaths;

        posted = PostMessageW (m_context.window.GetHwnd(), (UINT) ExplorerMessage::DropMenu, 0, 0);
        IGNORE_RETURN_VALUE (posted, TRUE);
        return;
    }

    if (source.fromImage)
    {
        outcome = m_context.actions.CopyEntriesInto (source.image, source.kind, source.catalogPaths, location.path, targetKind, inner);
    }
    else
    {
        outcome = m_context.actions.PutInto (location.path, targetKind, inner, source.hostPaths, m_context.fileOps.MakeAddressPrompt());
    }

    //  Only a copy that landed whole is taken from where it came: one salvaged
    //  from a damaged disk does not delete the original.
    if (effect == DROPEFFECT_MOVE && outcome.IsWhole())
    {
        if (source.fromImage)
        {
            removed = m_context.actions.DeleteEntries (source.image, source.catalogPaths);

            if (!removed.Succeeded())
            {
                outcome = removed;
            }
        }
        else
        {
            recycled = m_context.fileOps.RecycleHostFiles (source.hostPaths);
        }

        if (FAILED (recycled))
        {
            outcome.warning += (outcome.warning.empty() ? L"" : L"\n") + std::wstring (L"The originals could not be moved to the Recycle Bin.");
        }
    }

    if (effect != DROPEFFECT_MOVE)
    {
        m_context.fileOps.PushPutUndo (outcome, location.path, inner);
    }

    m_context.commandManager.ReportOutcome (outcome, (effect == DROPEFFECT_MOVE) ? L"Move" : L"Put");
    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::BeginDragOut
//
//  The drag runs its own loop until the drop or the cancel, so the list is
//  told the button came up afterwards; it saw the press and never the
//  release.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::BeginDragOut()
{
    HRESULT                                  hr       = S_OK;
    DWORD                                    effect   = DROPEFFECT_NONE;
    HostFileNaming::Style                    style    = m_context.commandManager.GetNamingStyle();
    auto                                     log      = std::make_shared<CassoExplorerDragOut::RenderLog>();
    std::vector<DxuiDragDropSource::Format>  formats  = CassoExplorerDragOut::BuildFormats (m_context.browser, style, log);
    POINT                                    start    = {};
    std::vector<FileEntry>                   entries;
    std::vector<std::string>                 paths;
    std::vector<std::wstring>                shellIds;
    bool                                     inImage  = m_context.browser.IsImageLocation();
    std::string                              image    = TextEncoding::WideToNarrow (m_context.browser.GetLocation().path);
    CassoExplorerActions::Outcome            outcome;
    BOOL                                     got      = FALSE;
    Location                                 drive;
    std::shared_ptr<const DxuiIconImage>     icon     = GetRowDragImage();
    bool                                     images   = !m_context.browser.GetSelectedRows().empty();



    m_context.listPane.ReleasePress();

    //  Casso's own icon goes with a drag of disk images and of files with no
    //  file of their own, which the shell has no image for.
    for (int row : m_context.browser.GetSelectedRows())
    {
        images = images && row >= 0 && (size_t) row < m_context.browser.GetRows().size() && m_context.browser.GetRows()[(size_t) row].isDiskImage;
    }

    //  Dragging a linked machine's drive drags its whole disk, as dragging its
    //  node in the tree does.
    if (m_context.cassoLinkManager.TryGetSelectedLinkDrive (drive))
    {
        BeginDiskDrag (drive.path);
        return;
    }

    if (!m_context.browser.PrepareCassoLinkItems())
    {
        return;
    }

    //  Files on the disk, and items the shell lists, drag as File Explorer's
    //  do: the shell's own data object, with its thumbnails, its drop words,
    //  and what each target does with it. Only an image's entries need the
    //  formats built here.
    if (m_context.fileOps.TryGetSelectedShellIds (shellIds))
    {
        hr = m_context.shellVerbs.DragItems (shellIds, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, images ? icon.get() : nullptr);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_context.folderSync.RefreshAfterHostChange();
        m_context.window.Invalidate();
        return;
    }

    if (formats.empty())
    {
        return;
    }

    //  What a move would take away, read now: the drag may change the list.
    if (inImage)
    {
        m_context.browser.GetSelectedEntries (entries);

        for (const FileEntry & entry : entries)
        {
            paths.push_back (m_context.browser.GetEntryPath (entry));
        }
    }

    got = GetCursorPos (&start);
    IGNORE_RETURN_VALUE (got, TRUE);

    hr = DxuiDragDropSource::Begin (std::move (formats), DROPEFFECT_COPY | DROPEFFECT_MOVE, start, effect, nullptr,
                                    (images || inImage || m_context.browser.IsAppleDriveLocation()) ? icon.get() : nullptr);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  A target that moved the entries by copying them leaves their removal
    //  to the source, as Explorer does with files it cannot move itself. A
    //  host file is Explorer's to move, and Casso Explorer's drops report a
    //  copy once they have finished a move themselves. The originals are
    //  deleted only when the target read every file whole: not one skipped
    //  in its conflict dialog, one that failed to read, or one from a disk
    //  with sectors that could not be decoded.
    if (effect == DROPEFFECT_MOVE && inImage && !paths.empty() && !log->IsWhole())
    {
        m_context.window.ShowMessage (L"The originals were not deleted, because the destination did not get every file whole: a file was "
                              L"skipped, could not be read, or came from sectors the disk could not decode.", MB_ICONWARNING);
    }
    else if (effect == DROPEFFECT_MOVE && inImage && !paths.empty())
    {
        outcome = m_context.actions.DeleteEntries (image, paths);
        m_context.commandManager.ReportOutcome (outcome, L"Move");
        m_context.folderSync.RefreshAfterHostChange();
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::ShowDropMenu
//
//  What a right-drag's drop does, asked at the point it landed. A file whose
//  name or container records what it is leaves nothing to choose, so the
//  menu names that and offers it alone; a file that records nothing offers
//  the conversions a put can make.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::ShowDropMenu()
{
    std::vector<DxuiPopupMenuItem>  items;
    CassoExplorerActions::DropKind  kind;
    std::wstring                    label;



    if (!m_pendingDrop.valid)
    {
        return;
    }

    m_context.menus.GetMenuCommands().clear();

    if (m_pendingDrop.fromImage)
    {
        //  Entries from another image go across as they are, types mapped.
        m_context.menus.AddMenuCommand (items, L"&Copy here", [this]() { RunDrop (CassoExplorerActions::Conversion::ByContent); });
    }
    else
    {
        kind = m_context.actions.DescribeDrop (m_pendingDrop.hostPaths, m_pendingDrop.targetKind);

        if (kind.determined)
        {
            label = L"&Copy here as " + kind.label;
            m_context.menus.AddMenuCommand (items, label.c_str(), [this]() { RunDrop (CassoExplorerActions::Conversion::ByContent); });
        }
        else
        {
            m_context.menus.AddMenuCommand (items, L"&Copy here",                   [this]() { RunDrop (CassoExplorerActions::Conversion::ByContent); });
            m_context.menus.AddMenuCommand (items, L"Copy here as &text",           [this]() { RunDrop (CassoExplorerActions::Conversion::Text); });
            m_context.menus.AddMenuCommand (items, L"Copy here as Applesoft &BASIC", [this]() { RunDrop (CassoExplorerActions::Conversion::Applesoft); });
            m_context.menus.AddMenuCommand (items, L"Copy here as &binary...",      [this]() { RunDrop (CassoExplorerActions::Conversion::Binary); });
        }
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    m_context.menus.AddMenuCommand (items, L"Ca&ncel", [this]() { m_pendingDrop = PendingDrop(); });

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), m_pendingDrop.screen.x, m_pendingDrop.screen.y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::RunDrop
//
//  The drop the menu settled, with the conversion it chose.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::RunDrop (CassoExplorerActions::Conversion conversion)
{
    PendingDrop                    drop    = m_pendingDrop;
    CassoExplorerActions::Outcome  outcome;



    m_pendingDrop = PendingDrop();

    if (!drop.valid)
    {
        return;
    }

    if (drop.fromImage)
    {
        outcome = m_context.actions.CopyEntriesInto (drop.sourceImage, drop.sourceKind, drop.catalogPaths,
                                                      drop.location.path, drop.targetKind, drop.inner);
    }
    else
    {
        outcome = m_context.actions.PutInto (drop.location.path, drop.targetKind, drop.inner, drop.hostPaths,
                                              m_context.fileOps.MakeAddressPrompt(), conversion);
    }

    m_context.fileOps.PushPutUndo    (outcome, drop.location.path, drop.inner);
    m_context.commandManager.ReportOutcome  (outcome, L"Put");
    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::HandleDropFile
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::HandleDropFile (const std::wstring & path)
{
    CassoExplorerActions::Outcome  outcome = m_context.actions.PutFiles ({ path }, m_context.fileOps.MakeAddressPrompt());



    m_context.fileOps.PushPutUndo   (outcome, m_context.browser.GetLocation().path, m_context.browser.GetLocation().innerPath);
    m_context.commandManager.ReportOutcome (outcome, L"Put");
    m_context.listPane.FillList();
}
