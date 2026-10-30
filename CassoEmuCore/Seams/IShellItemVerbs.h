#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IShellItemVerbs
//
//  What Windows itself does with a real file or folder: open it in the
//  program it belongs to, offer the other programs that can open it, and
//  show the shell's own context menu.
//
//  A SEAM BECAUSE A TEST MAY NOT CALL THE SHELL. Which of these a menu offers
//  is decided by the caller; this only carries them out.
//
////////////////////////////////////////////////////////////////////////////////

class IShellItemVerbs
{
public:
    //  A program registered to open a file's type.
    struct Handler
    {
        std::wstring  name;
    };

    //  An item in the Recycle Bin, with the columns Explorer shows for it.
    //  The id is the shell's name for the item, which the calls below take.
    struct RecycledItem
    {
        std::wstring  id;
        std::wstring  name;
        std::wstring  originalFolder;
        std::wstring  typeText;
        uint64_t      sizeBytes    = 0;
        int64_t       deletedUnix  = 0;
        bool          hasDeleted   = false;
        int64_t       modifiedUnix = 0;
        bool          hasModified  = false;
        bool          isFolder     = false;
    };

    enum class RecycledVerb { Restore, Delete, Properties };

    //  An item in any shell folder: the shell's own name for it, the name it
    //  shows, and its path when it is a file or folder on a disk.
    struct ShellFolderItem
    {
        std::wstring  id;
        std::wstring  name;
        std::wstring  path;
        std::wstring  typeText;
        uint64_t      sizeBytes     = 0;
        int64_t       modifiedUnix  = 0;
        bool          hasModified   = false;
        bool          isFolder      = false;
        bool          isFile        = false;   // a folder that is a file too: a zip, a library
        bool          hasSubfolders = false;
        bool          leading       = false;   // one of Explorer's first group: Home, Gallery, the user's OneDrive
        bool          isThisPc      = false;   // the shell's own This PC, where Casso Explorer puts its own
        bool          isRecycleBin  = false;
        bool          isNetwork     = false;
        bool          isLibraries   = false;
        bool          shownByShell  = true;    // File Explorer's pane shows it
        bool          isPinned      = false;   // pinned to Quick access, not only frequent
        int           shellOrder    = 0;       // 1 and up: its place in its folder's own Name order, which Explorer sorts by
        std::wstring  folder;                  // a search result's folder
        std::wstring  imagePath;               // a search result inside this disk image
        size_t        catalogIndex  = 0;       // and where it is in that image's catalog
        std::string   imageDirectory;          // the directory inside the image that lists it, or empty for the volume's
        std::wstring  iconId;                  // the namespace entry whose icon it shows, as OneDrive's cloud

        //  What the Home folder records of an item; homeGroup is 0 anywhere else.
        int           homeGroup     = 0;       // System.Home.Grouping: 1 is Quick access, 3 is Recent
        uint32_t      homeOrder     = 0;       // System.Home.SortOrder
        bool          homePinned    = false;
        bool          isShared      = false;
        int64_t       accessedUnix  = 0;
        int64_t       activityUnix  = 0;
        std::wstring  folderDisplay;           // System.ItemFolderPathDisplay
        std::wstring  sharedBy;
        std::wstring  activity;                // System.ActivityInfo
        int64_t       sharedUnix    = 0;
    };

    //  What a paste did: each item it took and the item it made, in the same
    //  order, and whether the items were moved rather than copied.
    struct PasteResult
    {
        bool                       moved = false;
        std::vector<std::wstring>  sources;
        std::vector<std::wstring>  created;
    };

    virtual ~IShellItemVerbs () = default;

    //  The file's default action, as a double-click in Explorer does.
    virtual HRESULT  Open (HWND owner, const std::wstring & path) = 0;

    //  The programs Windows recommends for the file's type, in its order,
    //  and opening the file in one of them by its index in that list.
    virtual HRESULT  GetOpenWithHandlers (const std::wstring & path, std::vector<Handler> & outHandlers) = 0;
    virtual HRESULT  OpenWith            (HWND owner, const std::wstring & path, size_t handlerIndex) = 0;

    //  Windows' own Open with dialog, for a program not on the list.
    virtual HRESULT  ChooseOtherApp (HWND owner, const std::wstring & path) = 0;

    //  The shell's full context menu for the items, which all share a
    //  folder, at a screen point. Whatever is picked is carried out there,
    //  including by other programs' menu handlers. With a rename callback the
    //  menu has Rename, and picking it calls the callback.
    virtual HRESULT  ShowShellMenu (HWND owner, const std::vector<std::wstring> & paths, POINT screenPx, const std::function<void()> & rename) = 0;

    //  The shell's menu for a folder's background, as its classic menu has it
    //  outside File Explorer: other programs' commands, New and Properties.
    virtual HRESULT  ShowBackgroundMenu (HWND owner, const std::wstring & folder, POINT screenPx) = 0;

    //  One verb from the items' shell menu, by its canonical name, invoked
    //  as if picked there: an archive's `extract` opens its Extract All.
    virtual HRESULT  RunItemVerb (HWND owner, const std::vector<std::wstring> & paths, const char * verb) = 0;

    //  Which of the canonical verbs the items' shell menu has, in the order
    //  given, with the menu's label for each: a drive's Format and Eject come
    //  from its shell menu rather than from the registry.
    struct ItemVerb
    {
        std::string   verb;
        std::wstring  label;
    };

    virtual HRESULT  ListItemVerbs (const std::vector<std::wstring> & paths, const std::vector<std::string> & verbs, std::vector<ItemVerb> & outFound)
    {
        UNREFERENCED_PARAMETER (paths);
        UNREFERENCED_PARAMETER (verbs);

        outFound.clear();
        return S_OK;
    }

    //  Explorer's own file operations, with its progress, conflict and undo
    //  handling: deleting to the Recycle Bin, renaming, and cut, copy and
    //  paste through the clipboard other programs share.
    virtual HRESULT  Recycle           (HWND owner, const std::vector<std::wstring> & paths) = 0;

    //  The latest deleted copy of each path back out of the Recycle Bin, as
    //  Explorer's undo restores a deletion.
    virtual HRESULT  RestoreRecycled   (HWND owner, const std::vector<std::wstring> & paths) = 0;

    //  The Recycle Bin as Explorer shows it: its items, Restore and Delete
    //  (for good, after the shell's own confirmation) on some of them, the
    //  shell's context menu for them, and emptying it.
    virtual HRESULT  ListRecycled      (std::vector<RecycledItem> & outItems) = 0;
    virtual HRESULT  RunRecycledVerb   (HWND owner, const std::vector<std::wstring> & ids, RecycledVerb verb) = 0;
    virtual HRESULT  ShowRecycledMenu  (HWND owner, const std::vector<std::wstring> & ids, POINT screenPx) = 0;
    virtual HRESULT  EmptyRecycleBin   (HWND owner) = 0;

    //  Any shell folder's items, the name a shell item shows, and its default
    //  action, as a double-click in Explorer does, for an item that is not a
    //  file on a disk.
    virtual HRESULT  ListShellFolder   (const std::wstring & id, std::vector<ShellFolderItem> & outItems) { (void) id; (void) outItems; return E_NOTIMPL; }

    //  The same listing as something that can run on another thread, apart
    //  from this object, which may be gone by the time a slow one finishes.
    //  By default it calls ListShellFolder on this object, which suits a fake
    //  read on the caller's thread.
    using ShellLister = std::function<HRESULT (const std::wstring & id, std::vector<ShellFolderItem> & outItems)>;

    //  Whether Windows previews a host file: a preview handler for its type,
    //  or a thumbnail for a picture or a video.
    virtual bool  HasShellPreview (const std::wstring & path) { (void) path; return false; }

    virtual ShellLister  GetShellLister   ()                                                                { return [this] (const std::wstring & id, std::vector<ShellFolderItem> & outItems) { return ListShellFolder (id, outItems); }; }
    virtual HRESULT      GetShellItemName (const std::wstring & id, std::wstring & outName)               { (void) id; (void) outName; return E_NOTIMPL; }
    virtual HRESULT      OpenShellItem    (HWND owner, const std::wstring & id)                            { (void) owner; (void) id; return E_NOTIMPL; }

    //  The folder above a shell item, as Explorer's Up goes: its id, the name
    //  it shows, and its path when it is a folder on a disk. S_FALSE at the
    //  top, where there is nothing above.
    virtual HRESULT  GetShellParent    (const std::wstring & id, ShellFolderItem & outParent)          { (void) id; (void) outParent; return E_NOTIMPL; }

    //  The navigation pane's own roots, in the order Explorer shows them, and
    //  the folders pinned to Quick access, in theirs.
    virtual HRESULT  ListNavigationRoots (std::vector<ShellFolderItem> & outRoots)                     { (void) outRoots; return E_NOTIMPL; }
    virtual HRESULT  ListPinnedFolders   (std::vector<ShellFolderItem> & outFolders)                   { (void) outFolders; return E_NOTIMPL; }

    //  What the address bar's leading chevron drops, as Explorer's does: the
    //  desktop's own roots, then the folders on the user's desktop by name.
    virtual HRESULT  ListDesktopFolders  (std::vector<ShellFolderItem> & outFolders)                   { (void) outFolders; return E_NOTIMPL; }

    //  Every folder in the desktop's namespace, in the desktop folder's own
    //  order, as File Explorer's tree has them under Desktop with the all-folders
    //  option on.
    virtual HRESULT  ListDesktopNamespace (std::vector<ShellFolderItem> & outFolders)                  { (void) outFolders; return E_NOTIMPL; }

    //  The desktop namespace's own root, which has no parsing name; the shell
    //  shell desktop's class id is used as its id.
    static constexpr const wchar_t *  kDesktopNamespaceId = L"::{00021400-0000-0000-C000-000000000046}";
    static constexpr const wchar_t *  kHomeId             = L"shell:::{679F85CB-0220-4080-B29B-5540CC05AAB6}";
    static constexpr const wchar_t *  kHomeTreeId         = L"::{F874310E-B6B7-47DC-BC84-B9E6B38F5903}";
    static constexpr const wchar_t *  kHomeRecentId       = L"shell:::{42254EE9-E625-4065-8F70-775090256F72}";
    static constexpr const wchar_t *  kHomeSharedId       = L"shell:::{18F546F6-B34B-4B30-B4C6-5E88BED8BD84}";

    //  Pins a folder to Quick access, or unpins it, through the shell's own
    //  verbs, so File Explorer's list changes with it.
    virtual HRESULT  SetPinnedToQuickAccess (HWND owner, const std::wstring & folder, bool pinned)    { (void) owner; (void) folder; (void) pinned; return E_NOTIMPL; }

    //  Whether the shell opens this file as a folder, as File Explorer opens a
    //  zip: an item that is both a folder and a stream.
    virtual bool     IsArchive (const std::wstring & path)                                           { (void) path; return false; }

    //  Deletes items for good, not to the Recycle Bin, after Windows' own
    //  confirmation, as Shift+Delete does in File Explorer; nothing to undo.
    virtual HRESULT  DeletePermanently (HWND owner, const std::vector<std::wstring> & ids)          { (void) owner; (void) ids; return E_NOTIMPL; }

    //  Drags items out through the shell's own data object, for items with no
    //  file of their own, such as files inside a zip, which the shell extracts
    //  where they are dropped. With `dragImage`, that image is drawn in place
    //  of the shell's image of the items.
    virtual HRESULT  DragItems (const std::vector<std::wstring> & ids, DWORD allowedEffects, const DxuiIconImage * dragImage = nullptr)
    {
        (void) ids; (void) allowedEffects; (void) dragImage; return E_NOTIMPL;
    }

    //  Up to `maxBytes` of an item the shell lists, read through its stream,
    //  for one with no file on a disk, such as a file inside a zip.
    virtual HRESULT  ReadItemBytes (const std::wstring & id, size_t maxBytes, std::vector<Byte> & outBytes) { (void) id; (void) maxBytes; (void) outBytes; return E_NOTIMPL; }

    //  Posts the message to the window whenever the Recycle Bin's contents
    //  may have changed, whoever changed them, until the watch ends.
    virtual void     WatchRecycleBin   (HWND hwnd, UINT message) { (void) hwnd; (void) message; }
    virtual void     UnwatchRecycleBin ()                        {}
    virtual HRESULT  RenameItem        (HWND owner, const std::wstring & path, const std::wstring & newName) = 0;
    virtual HRESULT  PlaceOnClipboard  (HWND owner, const std::vector<std::wstring> & paths, bool cut) = 0;
    virtual bool     ClipboardHasFiles () const = 0;
    virtual HRESULT  PasteInto         (HWND owner, const std::wstring & folder, PasteResult & outResult) = 0;

    //  Each item moved to the full path beside it, as an undo of a move puts
    //  items back where they were.
    virtual HRESULT  MoveItemsTo       (HWND owner, const std::vector<std::wstring> & paths, const std::vector<std::wstring> & targets) = 0;
    virtual HRESULT  CreateFolder      (HWND owner, const std::wstring & parent, const std::wstring & name) = 0;

    //  Windows' share sheet for the items, as Explorer's Share opens it.
    virtual HRESULT  Share             (HWND owner, const std::vector<std::wstring> & paths) = 0;
};
