#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerThumbnails.h"
#include "CassoExplorer/Model/DiskImagePictures.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewOutput.h"
#include "Devices/Disk/DiskThumbnailCommand.h"
#include "Seams/IDiskPictures.h"
#include "Seams/IHostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::AddPreviewMenu
//
//  Cut, Copy and Paste as buttons along the top edge, then Set and Remove.
//  Paste is listed and disabled with no picture on the clipboard.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThumbnails::AddPreviewMenu (ExplorerContext & context, std::vector<DxuiPopupMenuItem> & items, const std::wstring & path)
{
    std::vector<std::shared_ptr<const DxuiCommand>>  buttons;
    std::shared_ptr<DxuiCommand>                     command;
    bool                                             hasPicture = HasPicture (context, path);
    static constexpr CassoExplorerActions::Verb      s_kVerbs[] = { CassoExplorerActions::Verb::Cut, CassoExplorerActions::Verb::Copy, CassoExplorerActions::Verb::Paste };



    for (CassoExplorerActions::Verb verb : s_kVerbs)
    {
        command          = std::make_shared<DxuiCommand>();
        command->id      = (int) verb;
        command->label   = ExplorerMenus::GetVerbLabel (verb);
        command->glyph   = ExplorerMenus::GetVerbGlyph (verb);
        command->menuSvg = context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));

        switch (verb)
        {
            case CassoExplorerActions::Verb::Cut:
                command->dispatch  = [&context, path]() { Cut (context, path); };
                command->isEnabled = [hasPicture]() { return hasPicture; };
                break;

            case CassoExplorerActions::Verb::Copy:
                command->dispatch  = [&context, path]() { Copy (context, path); };
                command->isEnabled = [hasPicture]() { return hasPicture; };
                break;

            default:
                command->dispatch  = [&context, path]() { Paste (context, path); };
                command->isEnabled = []() { return CanPaste(); };
                break;
        }

        buttons.push_back (command);
        context.menus.GetMenuCommands().push_back (std::move (command));
    }

    items.push_back (DxuiPopupMenuItem::ForIconRow (std::move (buttons)));
    items.push_back (DxuiPopupMenuItem::ForSeparator());

    items.push_back (DxuiPopupMenuItem::ForCommand (MakeCommand (context, L"&Set thumbnail...", [&context, path]() { SetFromFile (context, path); })));

    command            = MakeCommand (context, L"&Remove thumbnail", [&context, path]() { Remove (context, path); });
    command->isEnabled = [hasPicture]() { return hasPicture; };
    items.push_back (DxuiPopupMenuItem::ForCommand (command));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::AddSubmenu
//
//  A disk image's Thumbnail submenu: Copy, Paste, Set and Remove.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThumbnails::AddSubmenu (ExplorerContext & context, std::vector<DxuiPopupMenuItem> & items, const std::wstring & path)
{
    std::vector<DxuiPopupMenuItem>  children;
    std::shared_ptr<DxuiCommand>    command;
    std::shared_ptr<DxuiCommand>    parent     = std::make_shared<DxuiCommand>();
    bool                            hasPicture = HasPicture (context, path);



    command            = MakeCommand (context, L"&Copy", [&context, path]() { Copy (context, path); });
    command->isEnabled = [hasPicture]() { return hasPicture; };
    children.push_back (DxuiPopupMenuItem::ForCommand (command));

    command            = MakeCommand (context, L"&Paste", [&context, path]() { Paste (context, path); });
    command->isEnabled = []() { return CanPaste(); };
    children.push_back (DxuiPopupMenuItem::ForCommand (command));

    children.push_back (DxuiPopupMenuItem::ForSeparator());
    children.push_back (DxuiPopupMenuItem::ForCommand (MakeCommand (context, L"&Set thumbnail...", [&context, path]() { SetFromFile (context, path); })));

    command            = MakeCommand (context, L"&Remove thumbnail", [&context, path]() { Remove (context, path); });
    command->isEnabled = [hasPicture]() { return hasPicture; };
    children.push_back (DxuiPopupMenuItem::ForCommand (command));

    parent->label = L"Thum&bnail";
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (children)));
    context.menus.GetMenuCommands().push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::MakeCommand
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> ExplorerThumbnails::MakeCommand (ExplorerContext & context, const wchar_t * label, std::function<void()> dispatch)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->label    = label;
    command->dispatch = std::move (dispatch);

    context.menus.GetMenuCommands().push_back (command);

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::IsDiskImage
//
//  A floppy image, by its extension and size, as only those have thumbnails.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerThumbnails::IsDiskImage (const std::wstring & path)
{
    std::error_code  failed;
    uint64_t         bytes = std::filesystem::file_size (path, failed);



    return !failed && DiskImagePictures::IsFloppySize (std::filesystem::path (path).extension().wstring(), bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::HasPicture
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerThumbnails::HasPicture (ExplorerContext & context, const std::wstring & path)
{
    return context.app.pictures != nullptr && context.app.pictures->HasThumbnail (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::CanPaste
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerThumbnails::CanPaste()
{
    return IsClipboardFormatAvailable (RegisterClipboardFormatW (L"PNG")) != FALSE || IsClipboardFormatAvailable (CF_DIB) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::Copy
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThumbnails::Copy (ExplorerContext & context, const std::wstring & path)
{
    std::shared_ptr<const DxuiIconImage>  picture;
    std::wstring                          result;



    if (context.app.pictures == nullptr || !context.app.pictures->Find (path, picture, result) || picture == nullptr)
    {
        return;
    }

    ExplorerPreviewOutput::CopyPicture (context, *picture);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::Cut
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThumbnails::Cut (ExplorerContext & context, const std::wstring & path)
{
    Copy   (context, path);
    Remove (context, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::Paste
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThumbnails::Paste (ExplorerContext & context, const std::wstring & path)
{
    HRESULT            hr      = S_OK;
    std::vector<Byte>  picture;



    hr = ReadClipboardPicture (context.window.GetHwnd(), picture);

    if (FAILED (hr))
    {
        context.window.ShowMessage (L"The clipboard doesn't have a picture that can be pasted.", MB_ICONERROR);
        return;
    }

    SetPicture (context, path, picture, L"The pasted picture couldn't be set as the thumbnail.");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::SetFromFile
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThumbnails::SetFromFile (ExplorerContext & context, const std::wstring & path)
{
    HRESULT                hr      = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  picked;
    std::vector<Byte>      picture;
    bool                   chosen  = false;



    spec.title      = L"Set thumbnail";
    spec.clientGuid = s_kSetGuid;
    spec.filters    = { FileDialogFilter { L"Pictures (*.png;*.jpg;*.jpeg;*.bmp;*.gif)", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif" },
                        FileDialogFilter { L"All files (*.*)", L"*.*" } };

    hr = context.dialogs.PickFileToOpen (context.window.GetHwnd(), spec, picked, chosen);

    if (FAILED (hr) || !chosen)
    {
        return;
    }

    {
        std::ifstream  in (picked, std::ios::binary);

        picture.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
    }

    SetPicture (context, path, picture, L"The picture couldn't be set as the thumbnail.");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::Remove
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThumbnails::Remove (ExplorerContext & context, const std::wstring & path)
{
    HRESULT  hr = S_OK;



    if (context.app.pictures == nullptr)
    {
        return;
    }

    hr = context.app.pictures->RemoveThumbnail (path);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::SetPicture
//
//  Any picture Windows decodes is stored as a PNG.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThumbnails::SetPicture (ExplorerContext & context, const std::wstring & path, const std::vector<Byte> & picture, const wchar_t * failure)
{
    HRESULT            hr      = S_OK;
    std::vector<Byte>  png;
    bool               isReady = context.app.pictures != nullptr && !picture.empty();



    CBR (isReady);

    hr = DiskThumbnailCommand::ConvertToPng (picture, png);
    CHR (hr);

    hr = context.app.pictures->SetThumbnail (path, png);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        context.window.ShowMessage (failure, MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::ReadClipboardPicture
//
//  The clipboard's PNG, or its bitmap made into a BMP file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerThumbnails::ReadClipboardPicture (HWND owner, std::vector<Byte> & outPicture)
{
    HRESULT       hr        = S_OK;
    BOOL          opened    = OpenClipboard (owner);
    UINT          pngFormat = RegisterClipboardFormatW (L"PNG");
    bool          isPng     = false;
    HANDLE        data      = nullptr;
    const Byte  * locked    = nullptr;
    size_t        bytes     = 0;



    outPicture.clear();

    CBREx (opened, HRESULT_FROM_WIN32 (GetLastError()));

    isPng = IsClipboardFormatAvailable (pngFormat) != FALSE;
    data  = GetClipboardData (isPng ? pngFormat : CF_DIB);
    CBREx (data != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    locked = (const Byte *) GlobalLock (data);
    CBREx (locked != nullptr, E_OUTOFMEMORY);

    bytes = GlobalSize (data);

    if (isPng)
    {
        outPicture.assign (locked, locked + bytes);
    }
    else
    {
        hr = WrapDib (locked, bytes, outPicture);
    }

    GlobalUnlock (data);
    CHR (hr);

Error:
    if (opened)
    {
        CloseClipboard();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThumbnails::WrapDib
//
//  A BMP file is a file header, then the DIB. The pixels start after the
//  info header, its bit-field masks and its color table.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerThumbnails::WrapDib (const Byte * dib, size_t bytes, std::vector<Byte> & outBmp)
{
    HRESULT             hr      = S_OK;
    BITMAPINFOHEADER    info    = {};
    BITMAPFILEHEADER    file    = {};
    size_t              masks   = 0;
    size_t              colors  = 0;



    CBREx (bytes >= sizeof (info), HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    std::memcpy (&info, dib, sizeof (info));

    masks  = (info.biSize == sizeof (BITMAPINFOHEADER) && info.biCompression == BI_BITFIELDS) ? 3 * sizeof (DWORD) : 0;
    colors = (info.biClrUsed != 0) ? info.biClrUsed : (info.biBitCount <= 8 ? ((size_t) 1 << info.biBitCount) : 0);

    file.bfType    = 0x4D42;
    file.bfSize    = (DWORD) (sizeof (file) + bytes);
    file.bfOffBits = (DWORD) (sizeof (file) + info.biSize + masks + colors * sizeof (RGBQUAD));

    outBmp.resize (sizeof (file) + bytes);
    std::memcpy (outBmp.data(), &file, sizeof (file));
    std::memcpy (outBmp.data() + sizeof (file), dib, bytes);

Error:
    return hr;
}
