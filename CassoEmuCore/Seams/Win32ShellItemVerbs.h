#pragma once

#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShellVerbsTypes.h"
#include "Seams/ShellVerbs/Win32ShellMenus.h"
#include "Seams/ShellVerbs/Win32RecycleBin.h"
#include "Seams/ShellVerbs/Win32ShellNamespace.h"
#include "Seams/ShellVerbs/Win32ShellSearch.h"
#include "Seams/ShellVerbs/Win32ShellTransfers.h"
#include "Seams/Win32ShellPreview.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs
//
//  IShellItemVerbs through the shell's COM interfaces, on the UI thread's
//  apartment.
//
//  THE SHELL MENU RUNS IN ITS OWN WINDOW. Send to, Open with and the
//  handlers other programs add fill their submenus only when opened, and
//  draw their own rows, so the window the menu belongs to has to pass those
//  messages back to the menu while it is up. A hidden window of this class's
//  own does that, so the caller's window procedure knows nothing about it.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellItemVerbs : public IShellItemVerbs, public Win32ShellVerbsTypes
{
public:
    Win32ShellItemVerbs  () = default;
    ~Win32ShellItemVerbs () override;

    HRESULT  Open                (HWND owner, const std::wstring & path) override { return m_menus.Open (owner, path); }
    HRESULT  GetOpenWithHandlers (const std::wstring & path, std::vector<Handler> & outHandlers) override { return m_menus.GetOpenWithHandlers (path, outHandlers); }
    HRESULT  OpenWith            (HWND owner, const std::wstring & path, size_t handlerIndex) override { return m_menus.OpenWith (owner, path, handlerIndex); }
    HRESULT  ChooseOtherApp      (HWND owner, const std::wstring & path) override { return m_menus.ChooseOtherApp (owner, path); }
    HRESULT  ShowShellMenu       (HWND owner, const std::vector<std::wstring> & paths, POINT screenPx, const std::function<void()> & rename) override { return m_menus.ShowShellMenu (owner, paths, screenPx, rename); }
    HRESULT  ShowBackgroundMenu  (HWND owner, const std::wstring & folder, POINT screenPx) override { return m_menus.ShowBackgroundMenu (owner, folder, screenPx); }
    HRESULT  RunItemVerb         (HWND owner, const std::vector<std::wstring> & paths, const char * verb) override { return m_menus.RunItemVerb (owner, paths, verb); }
    bool     HasShellPreview     (const std::wstring & path) override { return Win32ShellPreview::HasPreview (path); }
    HRESULT  Recycle             (HWND owner, const std::vector<std::wstring> & paths) override { return m_recycleBin.Recycle (owner, paths); }
    HRESULT  RestoreRecycled     (HWND owner, const std::vector<std::wstring> & paths) override { return m_recycleBin.RestoreRecycled (owner, paths); }
    HRESULT  ListRecycled        (std::vector<RecycledItem> & outItems) override { return m_recycleBin.ListRecycled (outItems); }
    HRESULT  RunRecycledVerb     (HWND owner, const std::vector<std::wstring> & ids, RecycledVerb verb) override { return m_recycleBin.RunRecycledVerb (owner, ids, verb); }
    HRESULT  ShowRecycledMenu    (HWND owner, const std::vector<std::wstring> & ids, POINT screenPx) override { return m_recycleBin.ShowRecycledMenu (owner, ids, screenPx); }
    HRESULT  EmptyRecycleBin     (HWND owner) override { return m_recycleBin.EmptyRecycleBin (owner); }
    void     WatchRecycleBin     (HWND hwnd, UINT message) override { m_recycleBin.WatchRecycleBin (hwnd, message); }
    void     UnwatchRecycleBin   () override { m_recycleBin.UnwatchRecycleBin(); }
    HRESULT  ListShellFolder     (const std::wstring & id, std::vector<ShellFolderItem> & outItems) override { return m_namespace.ListShellFolder (id, outItems); }
    ShellLister  GetShellLister  () override { return &Win32ShellNamespace::ReadShellFolder; }
    HRESULT  GetShellItemName    (const std::wstring & id, std::wstring & outName) override { return m_namespace.GetShellItemName (id, outName); }
    HRESULT  OpenShellItem       (HWND owner, const std::wstring & id) override { return m_namespace.OpenShellItem (owner, id); }
    HRESULT  GetShellParent      (const std::wstring & id, ShellFolderItem & outParent) override { return m_namespace.GetShellParent (id, outParent); }
    HRESULT  ListNavigationRoots (std::vector<ShellFolderItem> & outRoots) override { return m_namespace.ListNavigationRoots (outRoots); }
    HRESULT  ListPinnedFolders   (std::vector<ShellFolderItem> & outFolders) override { return m_namespace.ListPinnedFolders (outFolders); }
    HRESULT  ListDesktopFolders  (std::vector<ShellFolderItem> & outFolders) override { return m_namespace.ListDesktopFolders (outFolders); }
    HRESULT  ListDesktopNamespace (std::vector<ShellFolderItem> & outFolders) override { return m_namespace.ListDesktopNamespace (outFolders); }
    HRESULT  SetPinnedToQuickAccess (HWND owner, const std::wstring & folder, bool pinned) override { return m_namespace.SetPinnedToQuickAccess (owner, folder, pinned); }
    bool     IsArchive (const std::wstring & path) override { return m_namespace.IsArchive (path); }
    HRESULT  DeletePermanently (HWND owner, const std::vector<std::wstring> & ids) override { return m_recycleBin.DeletePermanently (owner, ids); }
    HRESULT  DragItems (const std::vector<std::wstring> & ids, DWORD allowedEffects, const DxuiIconImage * dragImage = nullptr) override { return m_transfers.DragItems (ids, allowedEffects, dragImage); }
    HRESULT  ReadItemBytes (const std::wstring & id, size_t maxBytes, std::vector<Byte> & outBytes) override { return m_transfers.ReadItemBytes (id, maxBytes, outBytes); }
    HRESULT  RenameItem          (HWND owner, const std::wstring & path, const std::wstring & newName) override { return m_transfers.RenameItem (owner, path, newName); }
    HRESULT  PlaceOnClipboard    (HWND owner, const std::vector<std::wstring> & paths, bool cut) override { return m_transfers.PlaceOnClipboard (owner, paths, cut); }
    bool     ClipboardHasFiles   () const override { return m_transfers.ClipboardHasFiles(); }
    HRESULT  PasteInto           (HWND owner, const std::wstring & folder, PasteResult & outResult) override { return m_transfers.PasteInto (owner, folder, outResult); }
    HRESULT  MoveItemsTo         (HWND owner, const std::vector<std::wstring> & paths, const std::vector<std::wstring> & targets) override { return m_transfers.MoveItemsTo (owner, paths, targets); }
    HRESULT  CreateFolder        (HWND owner, const std::wstring & parent, const std::wstring & name) override { return m_transfers.CreateFolder (owner, parent, name); }
    HRESULT  Share               (HWND owner, const std::vector<std::wstring> & paths) override { return m_transfers.Share (owner, paths); }

private:
    static constexpr UINT            s_kFirstCommandId = 1;
    static constexpr UINT            s_kLastCommandId  = 0x7FFF;

    //  PKEY_ItemTypeText, PKEY_Size and PKEY_DateModified.
    static constexpr PROPERTYKEY  s_kTypeText     = { { 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } }, 4 };
    static constexpr PROPERTYKEY  s_kSize         = { { 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } }, 12 };
    static constexpr PROPERTYKEY  s_kDateModified = { { 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } }, 14 };

    HWND            m_menuHost   = nullptr;

    //  The shell verbs' parts, each with the state it uses.
    friend class Win32ShellMenus;
    friend class Win32RecycleBin;
    friend class Win32ShellNamespace;
    friend class Win32ShellSearch;
    friend class Win32ShellTransfers;
    Win32ShellMenus  m_menus { *this };
    Win32RecycleBin  m_recycleBin { *this };
    Win32ShellNamespace  m_namespace { *this };
    Win32ShellSearch  m_search { *this };
    Win32ShellTransfers  m_transfers { *this };
};

