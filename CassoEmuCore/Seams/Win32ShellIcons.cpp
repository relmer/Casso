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
//  Takes ownership of the handle: the icon is rasterized, the handle destroyed
//  and the pixels cached. A failure is cached too, as no icon, so a path with
//  no shell icon is not looked up again on every repaint.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::Remember (const std::wstring & key, HICON icon)
{
    std::shared_ptr<DxuiIconImage>  image;
    HRESULT                         hr    = S_OK;



    if (icon != nullptr)
    {
        image = std::make_shared<DxuiIconImage>();
        hr    = DxuiIconImage::FromHicon (icon, m_sizePx, *image);
        DestroyIcon (icon);

        if (FAILED (hr))
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
//  For a path that no longer exists, such as a removed known folder, the icon
//  comes from its attributes alone (SHGFI_USEFILEATTRIBUTES).
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
//  The Casso node uses the icon of Casso.exe in the same directory as this
//  executable.
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
