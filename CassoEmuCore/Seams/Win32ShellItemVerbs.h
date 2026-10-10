#pragma once

#include "Pch.h"

#include "Seams/IShellItemVerbs.h"





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

class Win32ShellItemVerbs : public IShellItemVerbs
{
public:
    Win32ShellItemVerbs  () = default;
    ~Win32ShellItemVerbs () override;

    HRESULT  Open                (HWND owner, const std::wstring & path) override;
    HRESULT  GetOpenWithHandlers (const std::wstring & path, std::vector<Handler> & outHandlers) override;
    HRESULT  OpenWith            (HWND owner, const std::wstring & path, size_t handlerIndex) override;
    HRESULT  ChooseOtherApp      (HWND owner, const std::wstring & path) override;
    HRESULT  ShowShellMenu       (HWND owner, const std::vector<std::wstring> & paths, POINT screenPx) override;
    HRESULT  Recycle             (HWND owner, const std::vector<std::wstring> & paths) override;
    HRESULT  RestoreRecycled     (HWND owner, const std::vector<std::wstring> & paths) override;
    HRESULT  ListRecycled        (std::vector<RecycledItem> & outItems) override;
    HRESULT  RunRecycledVerb     (HWND owner, const std::vector<std::wstring> & ids, RecycledVerb verb) override;
    HRESULT  ShowRecycledMenu    (HWND owner, const std::vector<std::wstring> & ids, POINT screenPx) override;
    HRESULT  EmptyRecycleBin     (HWND owner) override;
    void     WatchRecycleBin     (HWND hwnd, UINT message) override;
    void     UnwatchRecycleBin   () override;
    HRESULT  ListShellFolder     (const std::wstring & id, std::vector<ShellFolderItem> & outItems) override;
    ShellLister  GetShellLister  () override { return &ReadShellFolder; }
    HRESULT  GetShellItemName    (const std::wstring & id, std::wstring & outName) override;
    HRESULT  OpenShellItem       (HWND owner, const std::wstring & id) override;
    HRESULT  GetShellParent      (const std::wstring & id, ShellFolderItem & outParent) override;
    HRESULT  ListNavigationRoots (std::vector<ShellFolderItem> & outRoots) override;
    HRESULT  ListPinnedFolders   (std::vector<ShellFolderItem> & outFolders) override;
    HRESULT  ListDesktopFolders  (std::vector<ShellFolderItem> & outFolders) override;
    HRESULT  SetPinnedToQuickAccess (HWND owner, const std::wstring & folder, bool pinned) override;
    HRESULT  RenameItem          (HWND owner, const std::wstring & path, const std::wstring & newName) override;
    HRESULT  PlaceOnClipboard    (HWND owner, const std::vector<std::wstring> & paths, bool cut) override;
    bool     ClipboardHasFiles   () const override;
    HRESULT  PasteInto           (HWND owner, const std::wstring & folder, PasteResult & outResult) override;
    HRESULT  MoveItemsTo         (HWND owner, const std::vector<std::wstring> & paths, const std::vector<std::wstring> & targets) override;
    HRESULT  CreateFolder        (HWND owner, const std::wstring & parent, const std::wstring & name) override;
    HRESULT  Share               (HWND owner, const std::vector<std::wstring> & paths) override;

private:
    static constexpr UINT            s_kFirstCommandId = 1;
    static constexpr UINT            s_kLastCommandId  = 0x7FFF;
    static constexpr const wchar_t * s_kClassName      = L"CassoExplorerShellMenuHost";
    static constexpr const wchar_t * s_kCenteredProp   = L"CassoExplorerCentered";

    //  The canonical verb of Explorer's Share command, which opens the same
    //  share sheet an app would through the data transfer manager.
    static constexpr const char    * s_kShareVerb      = "Windows.ModernShare";
    static constexpr const wchar_t * s_kShareVerbW     = L"Windows.ModernShare";

    static LRESULT CALLBACK  MenuHostProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HRESULT  EnumHandlers    (const std::wstring & path, std::vector<IAssocHandler *> & outHandlers);
    HRESULT  CreateMenuHost  (HWND owner);
    HRESULT  GetItemsMenu    (HWND owner, const std::vector<std::wstring> & paths, IContextMenu ** outMenu);
    HRESULT  GetItemArray    (const std::vector<std::wstring> & paths, IShellItemArray ** outItems);
    HRESULT  CreateOperation (HWND owner, IFileOperation ** outOperation);
    HRESULT  TrackMenu       (HWND owner, IContextMenu * menu, POINT screenPx);
    HRESULT  GetRecycledMenu (HWND owner, const std::vector<std::wstring> & ids, IContextMenu ** outMenu);

    //  The Recycle Bin's columns: where an item was, and when it went.
    static constexpr PROPERTYKEY  s_kDeletedFrom  = { { 0x9B174B33, 0x40FF, 0x11D2, { 0xA2, 0x7E, 0x00, 0xC0, 0x4F, 0xC3, 0x08, 0x71 } }, 2 };
    static constexpr PROPERTYKEY  s_kDateDeleted  = { { 0x9B174B33, 0x40FF, 0x11D2, { 0xA2, 0x7E, 0x00, 0xC0, 0x4F, 0xC3, 0x08, 0x71 } }, 3 };

    //  PKEY_ItemTypeText, PKEY_Size and PKEY_DateModified.
    static constexpr PROPERTYKEY  s_kTypeText     = { { 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } }, 4 };
    static constexpr PROPERTYKEY  s_kSize         = { { 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } }, 12 };
    static constexpr PROPERTYKEY  s_kDateModified = { { 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } }, 14 };

    static int64_t  ToUnixSeconds (const FILETIME & when);

    //  The shell's confirmation and property dialogs center on the screen;
    //  while one of these is alive, each dialog that opens on this thread is
    //  moved over the window that asked for it instead.
    class CenterDialogsOver
    {
    public:
        explicit CenterDialogsOver (HWND owner);
        ~CenterDialogsOver();

        CenterDialogsOver (const CenterDialogsOver &)             = delete;
        CenterDialogsOver & operator= (const CenterDialogsOver &) = delete;

    private:
        static LRESULT CALLBACK  OnCbt (int code, WPARAM wParam, LPARAM lParam);

        static thread_local HWND  s_owner;
        HHOOK                     m_hook  = nullptr;
        HWND                      m_prior = nullptr;
    };

    static bool  IsFromFolder (IDataObject * data, const std::wstring & folder);

    //  Hears a file operation out: each item copied or moved and the item it
    //  made. Lives on the caller's stack for the one operation it is advised
    //  to, so its reference count does nothing.
    class OperationRecorder : public IFileOperationProgressSink
    {
    public:
        std::vector<std::wstring>  sources;
        std::vector<std::wstring>  created;

        IFACEMETHODIMP          QueryInterface (REFIID riid, void ** ppv) override;
        IFACEMETHODIMP_(ULONG)  AddRef         () override                                           { return 2; }
        IFACEMETHODIMP_(ULONG)  Release        () override                                           { return 1; }

        IFACEMETHODIMP  StartOperations  () override                                                 { return S_OK; }
        IFACEMETHODIMP  FinishOperations (HRESULT) override                                          { return S_OK; }
        IFACEMETHODIMP  PreRenameItem    (DWORD, IShellItem *, LPCWSTR) override                     { return S_OK; }
        IFACEMETHODIMP  PostRenameItem   (DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override { return S_OK; }
        IFACEMETHODIMP  PreMoveItem      (DWORD, IShellItem *, IShellItem *, LPCWSTR) override       { return S_OK; }
        IFACEMETHODIMP  PostMoveItem     (DWORD, IShellItem * item, IShellItem *, LPCWSTR, HRESULT hr, IShellItem * made) override;
        IFACEMETHODIMP  PreCopyItem      (DWORD, IShellItem *, IShellItem *, LPCWSTR) override       { return S_OK; }
        IFACEMETHODIMP  PostCopyItem     (DWORD, IShellItem * item, IShellItem *, LPCWSTR, HRESULT hr, IShellItem * made) override;
        IFACEMETHODIMP  PreDeleteItem    (DWORD, IShellItem *) override                              { return S_OK; }
        IFACEMETHODIMP  PostDeleteItem   (DWORD, IShellItem *, HRESULT, IShellItem *) override       { return S_OK; }
        IFACEMETHODIMP  PreNewItem       (DWORD, IShellItem *, LPCWSTR) override                     { return S_OK; }
        IFACEMETHODIMP  PostNewItem      (DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) override { return S_OK; }
        IFACEMETHODIMP  UpdateProgress   (UINT, UINT) override                                       { return S_OK; }
        IFACEMETHODIMP  ResetTimer       () override                                                 { return S_OK; }
        IFACEMETHODIMP  PauseTimer       () override                                                 { return S_OK; }
        IFACEMETHODIMP  ResumeTimer      () override                                                 { return S_OK; }

    private:
        void  Record (IShellItem * item, HRESULT hr, IShellItem * made);
    };

    static std::wstring  GetItemPath (IShellItem * item);
    static HRESULT       ReadShellFolder (const std::wstring & id, std::vector<ShellFolderItem> & outItems);

    //  An item's id, name, path, type, size, date and what kind of folder it
    //  is, as a listing shows it.
    static HRESULT       DescribeShellItem (IShellItem * item, ShellFolderItem & outEntry);

    //  The shell's class ids for the roots Explorer treats apart.
    static constexpr const wchar_t *  s_kHomeId       = L"::{F874310E-B6B7-47DC-BC84-B9E6B38F5903}";
    static constexpr const wchar_t *  s_kGalleryId    = L"::{E88865EA-0E1C-4E20-9AA6-EDCD0212C87C}";
    static constexpr const wchar_t *  s_kThisPcId     = L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}";
    static constexpr const wchar_t *  s_kRecycleBinId = L"::{645FF040-5081-101B-9F08-00AA002F954E}";
    static constexpr const wchar_t *  s_kQuickAccessId = L"shell:::{679F85CB-0220-4080-B29B-5540CC05AAB6}";
    static constexpr const wchar_t *  s_kLibrariesId  = L"::{031E4825-7B94-4DC3-B131-E946B44C8DD5}";
    static constexpr const wchar_t *  s_kNetworkId    = L"::{F02C1A0D-BE21-4350-88B0-7367FC96EF3C}";
    static constexpr const wchar_t *  s_kControlPanelId = L"::{26EE0668-A00A-44D7-9371-BEB064C98683}";
    static constexpr const wchar_t *  s_kLinuxId      =L"::{B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6}";

    //  Where a navigation root goes in Explorer's order.
    static int  GetNavigationRank (const ShellFolderItem & root);
    static int   GetDesktopRank   (const ShellFolderItem & root);

    //  A search stops at this many results, as a folder too large to read
    //  whole would otherwise never finish.
    static constexpr size_t  s_kMaxResults = 5000;

    static HRESULT  RunSearch     (const std::wstring & id, std::vector<ShellFolderItem> & outItems);
    static bool     IsIndexed     (const std::wstring & scope);
    static HRESULT  QueryIndex    (const std::wstring & scope, const std::wstring & query, std::vector<std::wstring> & outPaths);
    static void     WalkForNames  (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths);

    //  The disk images a search reads, at most, so a folder of thousands
    //  cannot keep it running for minutes.
    static constexpr size_t  s_kMaxImages           = 2000;
    static constexpr size_t  s_kMaxImageDirectories = 256;   // in one image, so a looping catalog cannot run on

    static void     SearchImages  (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<ShellFolderItem> & outItems);
    static void     WalkForImages (const std::wstring & scope, std::vector<std::wstring> & outImages);
    static bool  IsDesktopRoot    (const ShellFolderItem & item);
    static bool  IsOnDesktop      (const std::wstring & path);

    ULONG           m_binWatch   = 0;
    ULONG           m_binDeletes = 0;
    HWND            m_menuHost   = nullptr;
    IContextMenu2 * m_activeMenu = nullptr;
};
