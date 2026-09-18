#pragma once

#include "Pch.h"

#include "Seams/IShellIcons.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons
//
//  IShellIcons through SHGetFileInfo, rasterized once at the row's pixel size
//  and cached.
//
//  THE CACHE KEY DETERMINES WHICH ICONS ARE SHARED. Every .dsk file has the
//  same icon, so an ordinary file is cached by extension; folders, drives and
//  file types with per-file icons (.exe, .lnk, .ico, .url) are cached by path.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellIcons : public IShellIcons
{
public:
    //  The size the icons are drawn at, in pixels. A change drops the cache.
    void  SetSizePx (int sizePx);

    std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring & path, bool isDirectory) override;
    std::shared_ptr<const DxuiIconImage>  GetForKind (Kind kind) override;

    static constexpr int  kDefaultSizePx = 16;

private:
    //  File types that carry an icon of their own rather than their extension's.
    static constexpr const wchar_t *  kOwnIconExtensions[] = { L".exe", L".lnk", L".ico", L".url" };

    std::shared_ptr<const DxuiIconImage>  Remember (const std::wstring & key, HICON icon);
    HICON  LoadLargerForPath (const std::wstring & path, bool isDirectory);

    static constexpr int  s_kLargeIconPx      = 32;
    static constexpr int  s_kExtraLargeIconPx = 48;

    static std::wstring  GetCacheKey  (const std::wstring & path, bool isDirectory);
    static HICON         LoadForPath  (const std::wstring & path, UINT sizeFlag);
    static HICON         LoadForKind  (Kind kind, UINT sizeFlag);

    int                                                                      m_sizePx = kDefaultSizePx;
    std::unordered_map<std::wstring, std::shared_ptr<const DxuiIconImage>>  m_cache;
};
