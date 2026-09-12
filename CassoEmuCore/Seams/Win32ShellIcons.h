#pragma once

#include "Pch.h"

#include "Seams/IShellIcons.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons
//
//  IShellIcons through SHGetFileInfo, rasterized once at the row's pixel size
//  and kept.
//
//  CACHED BY WHAT DECIDES THE ICON. Every .dsk in a folder shows the same
//  picture, so an ordinary file is cached by its extension; a folder, a drive
//  and the few file types that carry an icon of their own are cached by path.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellIcons : public IShellIcons
{
public:
    //  The size the icons are drawn at, in pixels. A change drops the cache.
    void  SetSizePx (int sizePx);

    std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring & path) override;
    std::shared_ptr<const DxuiIconImage>  GetForKind (Kind kind) override;

    static constexpr int  kDefaultSizePx = 16;

private:
    //  File types that carry an icon of their own rather than their extension's.
    static constexpr const wchar_t *  kOwnIconExtensions[] = { L".exe", L".lnk", L".ico", L".url" };

    std::shared_ptr<const DxuiIconImage>  Remember (const std::wstring & key, HICON icon);

    static std::wstring  GetCacheKey  (const std::wstring & path);
    static HICON         LoadForPath  (const std::wstring & path, UINT sizeFlag);
    static HICON         LoadForKind  (Kind kind, UINT sizeFlag);
    static bool          Rasterize    (HICON icon, int sizePx, DxuiIconImage & outImage);
    static void          ApplyMask    (HDC dc, HICON icon, int sizePx, uint32_t * bits, DxuiIconImage & image);

    int                                                                      m_sizePx = kDefaultSizePx;
    std::unordered_map<std::wstring, std::shared_ptr<const DxuiIconImage>>  m_cache;
};
