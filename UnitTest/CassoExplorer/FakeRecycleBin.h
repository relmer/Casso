#pragma once

#include "Pch.h"

#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeRecycleBin
//
//  The shell's verbs with only the Recycle Bin's listing answered.
//
////////////////////////////////////////////////////////////////////////////////

class FakeRecycleBin : public IShellItemVerbs
{
public:
    std::vector<RecycledItem>     items;
    std::vector<ShellFolderItem>  shellItems;

    std::vector<ShellFolderItem>  navRoots;
    std::vector<ShellFolderItem>  pinned;
    std::vector<ShellFolderItem>  desktopItems;

    //  The extension the shell opens as a folder, and the bytes any item reads.
    std::wstring                  archiveExtension;

    //  The extension Windows previews.
    std::wstring                  previewExtension;
    std::vector<Byte>             itemBytes;

    HRESULT  ListShellFolder      (const std::wstring &, std::vector<ShellFolderItem> & outItems) override  { outItems = shellItems; return S_OK; }
    HRESULT  ListNavigationRoots  (std::vector<ShellFolderItem> & outRoots) override                   { outRoots = navRoots; return S_OK; }
    HRESULT  ListPinnedFolders    (std::vector<ShellFolderItem> & outFolders) override                 { outFolders = pinned; return S_OK; }
    HRESULT  ListDesktopNamespace (std::vector<ShellFolderItem> & outFolders) override                { outFolders = desktopItems; return S_OK; }

    bool     HasShellPreview (const std::wstring & path) override
    {
        return !previewExtension.empty() && path.ends_with (previewExtension);
    }

    bool     IsArchive (const std::wstring & path) override
    {
        return !archiveExtension.empty() && path.ends_with (archiveExtension);
    }

    HRESULT  ReadItemBytes (const std::wstring &, size_t maxBytes, std::vector<Byte> & outBytes) override
    {
        outBytes.assign (itemBytes.begin(), itemBytes.begin() + (std::min) (maxBytes, itemBytes.size()));
        return S_OK;
    }

    HRESULT  Open                (HWND, const std::wstring &) override                                  { return E_NOTIMPL; }
    HRESULT  GetOpenWithHandlers (const std::wstring &, std::vector<Handler> &) override                 { return E_NOTIMPL; }
    HRESULT  OpenWith            (HWND, const std::wstring &, size_t) override                          { return E_NOTIMPL; }
    HRESULT  ChooseOtherApp      (HWND, const std::wstring &) override                                  { return E_NOTIMPL; }
    HRESULT  ShowShellMenu       (HWND, const std::vector<std::wstring> &, POINT, const std::function<void()> &) override { return E_NOTIMPL; }
    HRESULT  ShowBackgroundMenu  (HWND, const std::wstring &, POINT) override                                     { return E_NOTIMPL; }
    HRESULT  RunItemVerb         (HWND, const std::vector<std::wstring> &, const char *) override       { return E_NOTIMPL; }
    HRESULT  Recycle             (HWND, const std::vector<std::wstring> &) override                     { return E_NOTIMPL; }
    HRESULT  RestoreRecycled     (HWND, const std::vector<std::wstring> &) override                     { return E_NOTIMPL; }
    HRESULT  ListRecycled        (std::vector<RecycledItem> & outItems) override                        { outItems = items; return S_OK; }
    HRESULT  RunRecycledVerb     (HWND, const std::vector<std::wstring> &, RecycledVerb) override       { return E_NOTIMPL; }
    HRESULT  ShowRecycledMenu    (HWND, const std::vector<std::wstring> &, POINT) override              { return E_NOTIMPL; }
    HRESULT  EmptyRecycleBin     (HWND) override                                                        { return E_NOTIMPL; }
    HRESULT  RenameItem          (HWND, const std::wstring &, const std::wstring &) override            { return E_NOTIMPL; }
    HRESULT  PlaceOnClipboard    (HWND, const std::vector<std::wstring> &, bool) override               { return E_NOTIMPL; }
    bool     ClipboardHasFiles   () const override                                                      { return false; }
    HRESULT  PasteInto           (HWND, const std::wstring &, PasteResult &) override                   { return E_NOTIMPL; }
    HRESULT  MoveItemsTo         (HWND, const std::vector<std::wstring> &, const std::vector<std::wstring> &) override { return E_NOTIMPL; }
    HRESULT  CreateFolder        (HWND, const std::wstring &, const std::wstring &) override            { return E_NOTIMPL; }
    HRESULT  Share               (HWND, const std::vector<std::wstring> &) override                     { return E_NOTIMPL; }
};
