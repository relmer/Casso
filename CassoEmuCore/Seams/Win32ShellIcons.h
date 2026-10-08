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
//  Each size keeps its own, so moving between views reloads nothing.
//
//  A per-path icon can take the shell a millisecond or more, and a folder such
//  as System32 has a thousand of them. With a window to tell, those load on a
//  background thread, as Explorer's do: the item shows its type's icon until
//  its own is ready, and the window is posted a message to take them.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellIcons : public IShellIcons
{
public:
    Win32ShellIcons() = default;
    ~Win32ShellIcons() override;

    Win32ShellIcons (const Win32ShellIcons &)             = delete;
    Win32ShellIcons & operator= (const Win32ShellIcons &) = delete;

    //  The size the icons are drawn at, in pixels.
    void  SetSizePx (int sizePx);

    //  The window's DPI, nonzero once set: an icon with no image at the size
    //  asked for is then drawn at its own pixel size, centered, as Explorer
    //  draws it.
    void  SetDpi    (UINT dpi) { m_dpi = dpi; }

    //  Loads per-path icons in the background and posts `message` to `hwnd`
    //  when some are ready; the window then calls TakeLoaded.
    void  LoadInBackground (HWND hwnd, UINT message);

    //  Moves the icons loaded since the last call into the cache. Whether
    //  there were any.
    bool  TakeLoaded();

    std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring & path, bool isDirectory) override;
    std::shared_ptr<const DxuiIconImage>  GetForKind (Kind kind) override;
    std::wstring                          GetTypeName (const std::wstring & path, bool isDirectory) override;
    bool                                  GetDriveInfo (const std::wstring & root, DriveInfo & outInfo) override;

    static constexpr int  kDefaultSizePx = 16;

private:
    //  File types that have an icon of their own rather than their extension's.
    static constexpr const wchar_t *  kOwnIconExtensions[] = { L".exe", L".lnk", L".ico", L".url" };

    struct Request
    {
        std::wstring  key;
        std::wstring  path;
        bool          isDirectory = false;
        int           sizePx      = kDefaultSizePx;
    };

    struct Loaded
    {
        std::wstring                     key;
        std::shared_ptr<DxuiIconImage>   image;
    };

    std::shared_ptr<const DxuiIconImage>  Remember (const std::wstring & key, HICON icon);
    std::wstring                          GetSizedKey (const std::wstring & key) const { return std::to_wstring (m_sizePx) + L"|" + key; }
    std::shared_ptr<const DxuiIconImage>  Queue (const std::wstring & key, const std::wstring & path, bool isDirectory);
    void                                  RunLoader();

    static constexpr int  s_kLargeIconPx      = 32;
    static constexpr int  s_kExtraLargeIconPx = 48;

    static std::wstring                    GetCacheKey       (const std::wstring & path, bool isDirectory);
    static HICON                           LoadForPath       (const std::wstring & path, UINT sizeFlag);
    static HICON                           LoadLargerForPath (const std::wstring & path, bool isDirectory, int sizePx);
    static HICON                           LoadSized         (const std::wstring & path, bool isDirectory, int sizePx);
    static HICON                           LoadShellItemIcon (const std::wstring & path, int sizePx, SIIGBF flags);
    static std::shared_ptr<DxuiIconImage>  Rasterize         (HICON icon, int sizePx, UINT dpi = 0);
    static HICON                           LoadForKind       (Kind kind, UINT sizeFlag);
    static HICON                           CropToCorner      (HICON icon, bool & outCropped);
    static bool                            RunsElevated      (const std::wstring & path);
    static void                            AddShield         (DxuiIconImage & image);

    static std::shared_ptr<const DxuiIconImage>  DrawAppleTypeIcon  (Kind kind, std::shared_ptr<DxuiIconImage> page, int sizePx);
    static float                                 GetRoundedCoverage (int x, int y, int side, float radius);
    static HRESULT                               DrawLetterCoverage (wchar_t letter, int side, std::vector<uint8_t> & outCoverage);

    int                                                                      m_sizePx = kDefaultSizePx;
    UINT                                                                     m_dpi    = 0;
    std::unordered_map<std::wstring, std::shared_ptr<const DxuiIconImage>>  m_cache;
    std::unordered_map<std::wstring, std::wstring>                          m_typeNames;

    //  The background loader. Requests and results pass under the lock;
    //  m_pending holds the keys asked for and not yet taken.
    HWND                              m_notifyHwnd    = nullptr;
    UINT                              m_notifyMessage = 0;
    std::thread                       m_loader;
    std::mutex                        m_lock;
    std::condition_variable           m_wake;
    std::deque<Request>               m_requests;
    std::vector<Loaded>               m_loaded;
    std::unordered_set<std::wstring>  m_pending;
    bool                              m_stopping      = false;
    bool                              m_posted        = false;
};
