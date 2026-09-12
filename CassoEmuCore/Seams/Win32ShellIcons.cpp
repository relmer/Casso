#include "Pch.h"

#include "Seams/Win32ShellIcons.h"

#include "Cassque/Model/LaunchCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::SetSizePx
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::SetSizePx (int sizePx)
{
    int  size = (sizePx > 0) ? sizePx : kDefaultSizePx;



    if (size != m_sizePx)
    {
        m_cache.clear();
    }

    m_sizePx = size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetForPath
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetForPath (const std::wstring & path)
{
    std::wstring  key   = GetCacheKey (path);
    UINT          flag  = (m_sizePx <= 16) ? SHGFI_SMALLICON : SHGFI_LARGEICON;
    auto          found = m_cache.find (key);



    if (found != m_cache.end())
    {
        return found->second;
    }

    return Remember (key, LoadForPath (path, flag));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetForKind
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetForKind (Kind kind)
{
    std::wstring  key   = L"*kind:" + std::to_wstring ((int) kind);
    UINT          flag  = (m_sizePx <= 16) ? SHGFI_SMALLICON : SHGFI_LARGEICON;
    auto          found = m_cache.find (key);



    if (found != m_cache.end())
    {
        return found->second;
    }

    return Remember (key, LoadForKind (kind, flag));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::Remember
//
//  Takes ownership of the handle: it is rasterized, destroyed, and the pixels
//  kept. A failure is remembered too, as no icon, so a path the shell cannot
//  answer for is not asked again on every repaint.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::Remember (const std::wstring & key, HICON icon)
{
    std::shared_ptr<DxuiIconImage>  image;
    bool                            drawn = false;



    if (icon != nullptr)
    {
        image = std::make_shared<DxuiIconImage>();
        drawn = Rasterize (icon, m_sizePx, *image);
        DestroyIcon (icon);

        if (!drawn)
        {
            image.reset();
        }
    }

    m_cache[key] = image;

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetCacheKey
//
//  A folder or a drive can carry its own icon, and so can a program, a
//  shortcut and an icon file; anything else looks like every other file with
//  its extension.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShellIcons::GetCacheKey (const std::wstring & path)
{
    DWORD         attributes = GetFileAttributesW (path.c_str());
    bool          directory  = (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
    size_t        dot        = path.rfind (L'.');
    size_t        slash      = path.find_last_of (L"\\/");
    std::wstring  extension;



    if (directory || dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash))
    {
        return L"*path:" + path;
    }

    extension = path.substr (dot);
    std::transform (extension.begin(), extension.end(), extension.begin(), towlower);

    for (const wchar_t * own : kOwnIconExtensions)
    {
        if (extension == own)
        {
            return L"*path:" + path;
        }
    }

    return L"*ext:" + extension;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadForPath
//
//  A path that is not there -- a known folder since removed -- is answered by
//  what it would look like, which is the only thing the shell can say.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadForPath (const std::wstring & path, UINT sizeFlag)
{
    SHFILEINFOW  info       = {};
    DWORD        attributes = GetFileAttributesW (path.c_str());
    UINT         flags      = SHGFI_ICON | sizeFlag;
    DWORD_PTR    result     = 0;



    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        attributes  = (path.rfind (L'.') == std::wstring::npos) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        flags      |= SHGFI_USEFILEATTRIBUTES;
    }

    result = SHGetFileInfoW (path.c_str(), attributes, &info, sizeof (info), flags);

    return (result != 0) ? info.hIcon : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadForKind
//
//  The Casso node takes the emulator's own icon from the executable beside
//  this one, which is where the shell would find it too.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadForKind (Kind kind, UINT sizeFlag)
{
    SHFILEINFOW       info             = {};
    PIDLIST_ABSOLUTE  pidl             = nullptr;
    HRESULT           hr               = S_OK;
    DWORD_PTR         result           = 0;
    wchar_t           module[MAX_PATH] = {};
    std::wstring      folder;
    size_t            slash            = 0;



    switch (kind)
    {
    case Kind::ThisPc:
        hr = SHGetKnownFolderIDList (FOLDERID_ComputerFolder, 0, nullptr, &pidl);

        if (FAILED (hr))
        {
            return nullptr;
        }

        result = SHGetFileInfoW ((LPCWSTR) pidl, 0, &info, sizeof (info), SHGFI_PIDL | SHGFI_ICON | sizeFlag);
        CoTaskMemFree (pidl);
        break;

    case Kind::Folder:
        result = SHGetFileInfoW (L"folder", FILE_ATTRIBUTE_DIRECTORY, &info, sizeof (info),
                                 SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | sizeFlag);
        break;

    case Kind::File:
        result = SHGetFileInfoW (L"file", FILE_ATTRIBUTE_NORMAL, &info, sizeof (info),
                                 SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | sizeFlag);
        break;

    case Kind::Casso:
        GetModuleFileNameW (nullptr, module, MAX_PATH);
        folder = module;
        slash  = folder.find_last_of (L"\\/");

        if (slash != std::wstring::npos)
        {
            folder.resize (slash);
        }

        return LoadForPath (LaunchCommand::GetSiblingPath (folder, LaunchCommand::kCassoExe), sizeFlag);

    default:
        break;
    }

    return (result != 0) ? info.hIcon : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::Rasterize
//
//  Into a top-down 32-bit DIB. An icon with an alpha channel draws there
//  premultiplied, which is what the renderer wants; one without leaves every
//  alpha at zero, and its mask then says which pixels are really there.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellIcons::Rasterize (HICON icon, int sizePx, DxuiIconImage & outImage)
{
    BITMAPINFO   bmi      = {};
    HDC          dc       = CreateCompatibleDC (nullptr);
    void       * raw      = nullptr;
    HBITMAP      dib      = nullptr;
    HGDIOBJ      previous = nullptr;
    bool         drawn    = false;
    bool         anyAlpha = false;
    size_t       count    = (size_t) sizePx * (size_t) sizePx;



    if (dc == nullptr)
    {
        return false;
    }

    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = sizePx;
    bmi.bmiHeader.biHeight      = -sizePx;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    dib = CreateDIBSection (dc, &bmi, DIB_RGB_COLORS, &raw, nullptr, 0);

    if (dib != nullptr && raw != nullptr)
    {
        uint32_t *  bits = (uint32_t *) raw;

        previous = SelectObject (dc, dib);
        drawn    = DrawIconEx (dc, 0, 0, icon, sizePx, sizePx, 0, nullptr, DI_NORMAL) != FALSE;

        if (drawn)
        {
            outImage.width  = sizePx;
            outImage.height = sizePx;
            outImage.bgraPremul.assign (bits, bits + count);

            for (uint32_t pixel : outImage.bgraPremul)
            {
                if ((pixel >> 24) != 0)
                {
                    anyAlpha = true;
                    break;
                }
            }

            if (!anyAlpha)
            {
                ApplyMask (dc, icon, sizePx, bits, outImage);
            }
        }

        SelectObject (dc, previous);
        DeleteObject (dib);
    }

    DeleteDC (dc);

    return drawn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::ApplyMask
//
//  The mask draws black where the icon is and white where it is not.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::ApplyMask (HDC dc, HICON icon, int sizePx, uint32_t * bits, DxuiIconImage & image)
{
    size_t  count = (size_t) sizePx * (size_t) sizePx;



    std::fill (bits, bits + count, 0u);

    if (DrawIconEx (dc, 0, 0, icon, sizePx, sizePx, 0, nullptr, DI_MASK) == FALSE)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        bool  opaque = (bits[i] & 0x00FFFFFFu) == 0;

        image.bgraPremul[i] = opaque ? (image.bgraPremul[i] | 0xFF000000u) : 0u;
    }
}
