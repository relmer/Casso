#pragma once

#include "Pch.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails
//
//  A disk image's thumbnail commands: Cut, Copy and Paste through the
//  clipboard, Set from a picture file, and Remove. The preview's picture menu
//  lists them with the clipboard's buttons along its top edge; a disk
//  image's menu lists them in a Thumbnail submenu.
//
//  A picture pasted or set becomes the thumbnail the user set. Cut copies the
//  thumbnail, then removes it.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerThumbnails
{
public:
    static void  AddPreviewMenu (ExplorerContext & context, std::vector<DxuiPopupMenuItem> & items, const std::wstring & path);
    static void  AddSubmenu     (ExplorerContext & context, std::vector<DxuiPopupMenuItem> & items, const std::wstring & path);

    static bool  IsDiskImage  (const std::wstring & path);
    static bool  HasPicture   (ExplorerContext & context, const std::wstring & path);
    static bool  CanPaste     ();
    static void  Copy         (ExplorerContext & context, const std::wstring & path);
    static void  Cut          (ExplorerContext & context, const std::wstring & path);
    static void  Paste        (ExplorerContext & context, const std::wstring & path);
    static void  SetFromFile  (ExplorerContext & context, const std::wstring & path);
    static void  Remove       (ExplorerContext & context, const std::wstring & path);

private:
    static std::shared_ptr<DxuiCommand>  MakeCommand          (ExplorerContext & context, const wchar_t * label, std::function<void()> dispatch);
    static HRESULT                       ReadClipboardPicture (HWND owner, std::vector<Byte> & outPicture);
    static HRESULT                       WrapDib              (const Byte * dib, size_t bytes, std::vector<Byte> & outBmp);
    static void                          SetPicture           (ExplorerContext & context, const std::wstring & path, const std::vector<Byte> & picture, const wchar_t * failure);

    static constexpr GUID  s_kSetGuid = { 0x2c7e91a4, 0x5d08, 0x4b63, { 0xa1, 0x7f, 0x3e, 0x96, 0xc2, 0x58, 0x0b, 0xd4 } };
};
