#pragma once

#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShellVerbsTypes.h"
#include "Seams/IShellItemVerbs.h"
#include "Seams/Win32ShellPreview.h"

class Win32ShellItemVerbs;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers
//
//  Moving items: drags, the clipboard, and paste and move through the
//  shell's file operations, with the moves and copies listed for undo;
//  virtual files, renames, new folders and Share.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellTransfers
{
public:
    explicit Win32ShellTransfers (Win32ShellItemVerbs & verbs);

    HRESULT  DragItems (const std::vector<std::wstring> & ids, DWORD allowedEffects, const DxuiIconImage * dragImage = nullptr);
    HRESULT  ReadItemBytes (const std::wstring & id, size_t maxBytes, std::vector<Byte> & outBytes);
    HRESULT  RenameItem          (HWND owner, const std::wstring & path, const std::wstring & newName);
    HRESULT  PlaceOnClipboard    (HWND owner, const std::vector<std::wstring> & paths, bool cut);
    bool     ClipboardHasFiles   () const;
    HRESULT  PasteInto           (HWND owner, const std::wstring & folder, IShellItemVerbs::PasteResult & outResult);
    HRESULT  MoveItemsTo         (HWND owner, const std::vector<std::wstring> & paths, const std::vector<std::wstring> & targets);
    HRESULT  CreateFolder        (HWND owner, const std::wstring & parent, const std::wstring & name);
    HRESULT  Share               (HWND owner, const std::vector<std::wstring> & paths);

    //  The canonical verb of Explorer's Share command, which opens the same
    //  share sheet an app would through the data transfer manager.
    static constexpr const char    * s_kShareVerb      = "Windows.ModernShare";
    static constexpr const wchar_t * s_kShareVerbW     = L"Windows.ModernShare";

    static bool  IsFromFolder (IDataObject * data, const std::wstring & folder);

    //  Virtual files, a descriptor and each file's contents with no file on
    //  disk, as from Casso Explorer's own copy out of a disk or a CassoLink
    //  drive, are pasted through the folder's drop target, as Explorer pastes
    //  them; IFileOperation takes only real items.
    static bool     HasOnlyVirtualFiles (IDataObject * data);
    static HRESULT  DropVirtualFiles    (IDataObject * data, IShellItem * target, const std::wstring & folder, IShellItemVerbs::PasteResult & outResult);

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

private:
    friend class Win32ShellItemVerbs;

    Win32ShellItemVerbs  & m_verbs;
};
