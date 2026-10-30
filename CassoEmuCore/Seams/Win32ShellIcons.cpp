#include "Pch.h"

#include "Core/PathResolver.h"
#include "Core/ThreadName.h"

#include "Seams/Win32ShellIcons.h"
#include "resource.h"

#include "CassoExplorer/Model/LaunchCommand.h"
#include "Devices/Printer/PngCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::SetSizePx
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::SetSizePx (int sizePx)
{
    int  size = (sizePx > 0) ? sizePx : kDefaultSizePx;



    m_sizePx = size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::~Win32ShellIcons
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellIcons::~Win32ShellIcons()
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_stopping = true;
    }

    m_wake.notify_all();

    if (m_loader.joinable())
    {
        m_loader.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadInBackground
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::LoadInBackground (HWND hwnd, UINT message)
{
    m_notifyHwnd    = hwnd;
    m_notifyMessage = message;

    if (!m_loader.joinable())
    {
        m_loader = std::thread ([this] { HRESULT hrName = ThreadName::SetForCurrentThread (L"Casso Explorer icon loader"); IGNORE_RETURN_VALUE (hrName, S_OK); RunLoader(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::TakeLoaded
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellIcons::TakeLoaded()
{
    std::vector<Loaded>  loaded;



    {
        std::lock_guard<std::mutex>  guard (m_lock);

        loaded.swap (m_loaded);
        m_posted = false;

        for (const Loaded & item : loaded)
        {
            m_pending.erase (item.key);
        }
    }

    for (Loaded & item : loaded)
    {
        StoreIcon (item.key, std::move (item.image));
    }

    return !loaded.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::StoreIcon
//
//  Per-path icons are the only ones whose number grows with what is browsed,
//  so past s_kMaxCachedIcons they are all dropped and read again as they are
//  drawn. A failure is stored too, with when it happened, so it is read
//  again after s_kRetryFailedMs rather than on every repaint. An icon by
//  extension is retried the same way; otherwise, after one failed read,
//  every file of its type has no icon.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::StoreIcon (const std::wstring & key, std::shared_ptr<const DxuiIconImage> image)
{
    if (image == nullptr)
    {
        m_failedAt[key] = GetTickCount64();
    }
    else
    {
        m_failedAt.erase (key);
    }

    m_cache[key] = std::move (image);

    if (m_cache.size() > s_kMaxCachedIcons)
    {
        std::erase_if (m_cache, [] (const auto & entry) { return entry.first.find (L"*path:") != std::wstring::npos; });
        m_failedAt.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::IsFailureExpired
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellIcons::IsFailureExpired (const std::wstring & key) const
{
    auto  failed = m_failedAt.find (key);



    return failed != m_failedAt.end() && GetTickCount64() - failed->second >= s_kRetryFailedMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::Queue
//
//  Asks the loader for a per-path icon, once, and answers for now with the
//  icon of the item's type.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::Queue (const std::wstring & key, const std::wstring & path, bool isDirectory, bool overlays, bool thumbnail)
{
    bool  added = false;



    {
        std::lock_guard<std::mutex>  guard (m_lock);

        added = m_pending.insert (key).second;

        if (added)
        {
            m_requests.push_back (Request { key, path, isDirectory, m_sizePx, overlays, thumbnail });
        }
    }

    if (added)
    {
        m_wake.notify_one();
    }

    //  A thumbnail's file has its type's icon until the thumbnail is made.
    if (thumbnail)
    {
        return GetForExtension (fs::path (path).extension().wstring());
    }

    return GetForKind (isDirectory ? Kind::Folder : Kind::File);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::HasThumbnail
//
//  Whether the shell has a thumbnail provider for the type, as it has for
//  pictures and videos; read once a type.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellIcons::HasThumbnail (const std::wstring & extension)
{
    static constexpr const wchar_t *  s_kThumbnailProvider = L"{E357FCCD-A995-4576-B01F-234630154E96}";
    auto                              found                = m_thumbnailTypes.find (extension);
    wchar_t                           handler[64]          = {};
    DWORD                             length               = (DWORD) std::size (handler);
    HRESULT                           hr                   = S_OK;



    if (found != m_thumbnailTypes.end())
    {
        return found->second;
    }

    hr = AssocQueryStringW (ASSOCF_NONE, ASSOCSTR_SHELLEXTENSION, extension.c_str(), s_kThumbnailProvider, handler, &length);

    return m_thumbnailTypes[extension] = SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::RunLoader
//
//  The newest request first: the folder just opened matters more than one
//  left behind.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::RunLoader()
{
    HRESULT  hrCom = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);



    for (;;)
    {
        Request                         request;
        std::shared_ptr<DxuiIconImage>  image;
        bool                            post    = false;
        int                             overlay = 0;

        {
            std::unique_lock<std::mutex>  guard (m_lock);

            m_wake.wait (guard, [this] { return m_stopping || !m_requests.empty(); });

            if (m_stopping)
            {
                break;
            }

            request = std::move (m_requests.back());
            m_requests.pop_back();
        }

        if (request.thumbnail)
        {
            HICON  thumbnail = LoadThumbnail (request.path, request.sizePx);

            image = Rasterize ((thumbnail != nullptr) ? thumbnail : LoadSized (request.path, false, request.sizePx, request.overlays, &overlay), request.sizePx, m_dpi);
        }
        else
        {
            image = Rasterize (LoadSized (request.path, request.isDirectory, request.sizePx, request.overlays, &overlay), request.sizePx, m_dpi);
        }

        if (image != nullptr && overlay > 0)
        {
            AddOverlay (*image, overlay);
        }

        if (image != nullptr && !request.isDirectory && RunsElevated (request.path))
        {
            AddShield (*image);
        }

        {
            std::lock_guard<std::mutex>  guard (m_lock);

            m_loaded.push_back (Loaded { request.key, std::move (image) });
            post     = !m_posted;
            m_posted = true;
        }

        if (post && m_notifyHwnd != nullptr)
        {
            PostMessageW (m_notifyHwnd, m_notifyMessage, 0, 0);
        }
    }

    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::RunsElevated
//
//  A program whose manifest asks for administrator rights, as the shell
//  decides to draw its shield.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellIcons::RunsElevated (const std::wstring & path)
{
    HMODULE      module   = nullptr;
    HRSRC        found    = nullptr;
    HGLOBAL      loaded   = nullptr;
    const char * text     = nullptr;
    DWORD        size     = 0;
    bool         elevated = false;



    if (!path.ends_with (L".exe") && !path.ends_with (L".EXE"))
    {
        return false;
    }

    module = LoadLibraryExW (path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);

    if (module == nullptr)
    {
        return false;
    }

    found  = FindResourceW (module, MAKEINTRESOURCEW (1), RT_MANIFEST);
    loaded = (found != nullptr) ? LoadResource (module, found) : nullptr;
    text   = (loaded != nullptr) ? static_cast<const char *> (LockResource (loaded)) : nullptr;
    size   = (found != nullptr) ? SizeofResource (module, found) : 0;

    if (text != nullptr)
    {
        std::string_view  manifest (text, size);

        elevated = manifest.find ("requireAdministrator") != std::string_view::npos
                || manifest.find ("highestAvailable")     != std::string_view::npos;
    }

    FreeLibrary (module);
    return elevated;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::AddShield
//
//  Windows' own shield over the image's lower right, half its size.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::AddShield (DxuiIconImage & image)
{
    SHSTOCKICONINFO  info   = { sizeof (info) };
    DxuiIconImage    shield;
    HRESULT          hr     = SHGetStockIconInfo (SIID_SHIELD, SHGSI_ICON | SHGSI_LARGEICON, &info);
    int              side   = image.width / 2;



    if (FAILED (hr) || side <= 0 || info.hIcon == nullptr)
    {
        return;
    }

    hr = DxuiIconImage::FromHicon (info.hIcon, side, shield);
    DestroyIcon (info.hIcon);

    if (FAILED (hr) || shield.width != side)
    {
        return;
    }

    DrawOver (image, shield, image.width - side, image.height - side);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::AddOverlay
//
//  The shell's overlay image, a badge in a corner of an icon's square, from
//  its small image list scaled to the image's size, as Explorer's list draws
//  it over a row's icon rather than scaled down with the icon.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::AddOverlay (DxuiIconImage & image, int overlay)
{
    HRESULT               hr      = S_OK;
    ComPtr<IImageList>    list;
    int                   index   = -1;
    HICON                 icon    = nullptr;
    ICONINFO              info    = {};
    BITMAP                art     = {};
    BOOL                  hasInfo = FALSE;
    DxuiIconImage         badge;



    hr = SHGetImageList (SHIL_SMALL, IID_PPV_ARGS (&list));
    CHR (hr);

    hr = list->GetOverlayImage (overlay, &index);
    CHR (hr);

    hr = list->GetIcon (index, ILD_TRANSPARENT, &icon);
    CHR (hr);
    CBR (icon != nullptr);

    hasInfo = GetIconInfo (icon, &info);
    CBR (hasInfo);

    if (info.hbmColor != nullptr)
    {
        GetObjectW (info.hbmColor, sizeof (art), &art);
    }

    DeleteIconBitmaps (info);
    CBR (art.bmWidth > 0);

    hr = DxuiIconImage::FromHicon (icon, art.bmWidth, badge);
    CHR (hr);

    DrawOver (image, DxuiIconImage::Resample (badge, image.width), 0, 0);

Error:
    if (icon != nullptr)
    {
        DestroyIcon (icon);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::DrawOver
//
//  `top` drawn over the image at `left`, `topY`, premultiplied as both are;
//  what falls outside the image is cut off.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::DrawOver (DxuiIconImage & image, const DxuiIconImage & top, int left, int topY)
{
    for (int y = 0; y < top.height; y++)
    {
        for (int x = 0; x < top.width; x++)
        {
            int       ix   = left + x;
            int       iy   = topY + y;
            bool      in   = ix >= 0 && iy >= 0 && ix < image.width && iy < image.height;
            uint32_t  over = top.bgraPremul[(size_t) y * top.width + x];
            uint32_t  keep = 255 - (over >> 24);
            uint32_t  out  = 0;

            for (int shift = 0; in && shift < 32; shift += 8)
            {
                out |= (((over >> shift) & 0xFF) + ((image.bgraPremul[(size_t) iy * image.width + ix] >> shift) & 0xFF) * keep / 255) << shift;
            }

            if (in)
            {
                image.bgraPremul[(size_t) iy * image.width + ix] = out;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetOverlayFlags
//
//  With `outOverlay` the overlay's index, for AddOverlay; without it the
//  overlay drawn into the icon itself.
//
////////////////////////////////////////////////////////////////////////////////

UINT Win32ShellIcons::GetOverlayFlags (bool overlays, const int * outOverlay)
{
    return !overlays              ? 0u
         : (outOverlay != nullptr) ? (UINT) SHGFI_OVERLAYINDEX
                                   : (UINT) SHGFI_ADDOVERLAYS;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetForPath
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetForPath (const std::wstring & path, bool isDirectory)
{
    return GetIconForPath (path, isDirectory, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetForTreePath
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetForTreePath (const std::wstring & path, bool isDirectory)
{
    return GetIconForPath (path, isDirectory, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetForIconResource
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetForIconResource (const std::wstring & resource)
{
    std::wstring  key            = GetSizedKey (L"*resource:" + resource);
    auto          found          = m_cache.find (key);
    wchar_t       path[MAX_PATH] = {};
    int           index          = 0;
    HICON         bigIcon        = nullptr;
    HICON         rowIcon        = nullptr;
    HRESULT       hr             = S_OK;



    if (found != m_cache.end())
    {
        return found->second;
    }

    if (resource.empty() || resource.size() >= std::size (path))
    {
        return nullptr;
    }

    //  A package's logo is a picture file, not an icon resource.
    if (_wcsicmp (fs::path (resource).extension().c_str(), L".png") == 0)
    {
        return LoadPicture (key, resource);
    }

    wcscpy_s (path, resource.c_str());
    index = PathParseIconLocationW (path);

    hr = SHDefExtractIconW (path, index, 0, &bigIcon, &rowIcon, MAKELONG (m_sizePx, m_sizePx));

    if (rowIcon != nullptr)
    {
        DestroyIcon (rowIcon);
    }

    return (hr == S_OK && bigIcon != nullptr) ? Remember (key, bigIcon) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetForExtension
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetForExtension (const std::wstring & extension)
{
    std::wstring  key   = GetSizedKey (L"*type:" + extension);
    auto          found = m_cache.find (key);



    if (found != m_cache.end())
    {
        return found->second;
    }

    return Remember (key, LoadByAttributes (L"New" + extension, false, m_sizePx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadPicture
//
//  A PNG decoded, premultiplied and scaled to the row's size.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::LoadPicture (const std::wstring & key, const std::wstring & file)
{
    HRESULT                         hr     = S_OK;
    std::ifstream                   input  (file, std::ios::binary);
    bool                            opened = input.good();
    std::vector<Byte>               bytes;
    RgbaImage                       rgba;
    DxuiIconImage                   source;
    std::shared_ptr<DxuiIconImage>  image;



    CBREx (opened, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    bytes.assign (std::istreambuf_iterator<char> (input), std::istreambuf_iterator<char>());

    hr = PngCodec::DecodeRgba (bytes, rgba);
    CHR (hr);

    source.width  = rgba.width;
    source.height = rgba.height;
    source.bgraPremul.resize ((size_t) rgba.width * (size_t) rgba.height);

    for (size_t i = 0; i < source.bgraPremul.size(); i++)
    {
        const Byte  * px    = &rgba.rgba[i * 4];
        uint32_t      alpha = px[3];

        source.bgraPremul[i] = (alpha << 24) | ((px[0] * alpha / 255) << 16) | ((px[1] * alpha / 255) << 8) | (px[2] * alpha / 255);
    }

    image = std::make_shared<DxuiIconImage> (DxuiIconImage::Resample (source, m_sizePx));

    m_cache[key] = image;

Error:
    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetIconForPath
//
//  A per-path icon without overlays is cached apart from the one with them;
//  an icon by extension has none either way.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetIconForPath (const std::wstring & path, bool isDirectory, bool overlays)
{
    std::wstring  cacheKey = GetCacheKey (path, isDirectory);
    std::wstring  key      = GetSizedKey ((overlays || !cacheKey.starts_with (L"*path:")) ? cacheKey : L"*tree" + cacheKey);
    auto          found    = m_cache.find (key);



    //  A failure old enough is read again, as a share that was offline may
    //  answer now.
    if (found != m_cache.end() && found->second == nullptr && IsFailureExpired (key))
    {
        m_cache.erase (found);
        m_failedAt.erase (key);
        found = m_cache.end();
    }

    if (found != m_cache.end())
    {
        return found->second;
    }

    //  A picture or a video in an icon view larger than the large icon has a
    //  thumbnail, made in the background, as File Explorer draws it.
    if (m_loader.joinable() && overlays && m_sizePx > s_kLargeIconPx && cacheKey.starts_with (L"*ext:") && HasThumbnail (cacheKey.substr (5)))
    {
        key   = GetSizedKey (L"*thumb:" + path);
        found = m_cache.find (key);

        return (found != m_cache.end()) ? found->second : Queue (key, path, false, overlays, true);
    }

    //  A per-path icon is the slow kind; one by extension is quick, and the
    //  same for every file of its type.
    if (m_loader.joinable() && GetCacheKey (path, isDirectory).starts_with (L"*path:"))
    {
        return Queue (key, path, isDirectory, overlays);
    }

    //  With no loader, a network item's own icon would be read on this thread
    //  from a share that may not answer; it takes its kind's icon instead.
    if (GetCacheKey (path, isDirectory).starts_with (L"*path:") && PathResolver::IsNetworkPath (path))
    {
        key = GetSizedKey (isDirectory ? std::wstring (L"*network:<folder>") : L"*network:" + GetCacheKey (path, false));
        found = m_cache.find (key);

        return (found != m_cache.end()) ? found->second : Remember (key, LoadByAttributes (path, isDirectory, m_sizePx));
    }

    {
        int    overlay = 0;
        HICON  icon    = LoadSized (path, isDirectory, m_sizePx, overlays, &overlay);

        return Remember (key, icon, overlay);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadByAttributes
//
//  The icon of every folder, or of every file of the path's extension, from
//  the attributes alone, so the path is never accessed. The large icon at
//  every size it serves, scaled down as it is rasterized, as Explorer's own.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadByAttributes (const std::wstring & path, bool isDirectory, int sizePx)
{
    SHFILEINFOW  info   = {};
    UINT         flags  = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON;
    DWORD_PTR    result = SHGetFileInfoW (path.c_str(), isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL,
                                          &info, sizeof (info), flags);



    return (result != 0) ? info.hIcon : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadSized
//
//  Past the large icon, the shell's bigger image lists have the art. Up to
//  it, the large icon at the system's size, which Rasterize scales down, as
//  Explorer draws its rows: an icon's smallest art can differ from its
//  larger art, as OneDrive's has a cloud alone where the rest have a folder.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadSized (const std::wstring & path, bool isDirectory, int sizePx, bool overlays, int * outOverlay)
{
    HICON  preview = nullptr;



    //  An item that is not on a disk has no path for the file calls; the shell
    //  draws its icon from the item itself. So does one inside a zip, whose
    //  path the file system does not know.
    if (path.starts_with (L"::") || _wcsnicmp (path.c_str(), L"shell:", 6) == 0
        || (path.size() > 2 && path[1] == L':' && !PathResolver::IsNetworkPath (path) && GetFileAttributesW (path.c_str()) == INVALID_FILE_ATTRIBUTES))
    {
        return (sizePx <= s_kLargeIconPx) ? LoadForShellName (path, overlays, outOverlay) : LoadShellItemIcon (path, sizePx, SIIGBF_ICONONLY);
    }

    //  A folder at a large size shows what is in it, as Explorer's does.
    if (isDirectory && sizePx > s_kLargeIconPx)
    {
        preview = LoadShellItemIcon (path, sizePx, SIIGBF_RESIZETOFIT);

        if (preview != nullptr)
        {
            return preview;
        }
    }

    if (sizePx > s_kLargeIconPx)
    {
        return LoadLargerForPath (path, isDirectory, sizePx);
    }

    return LoadForPath (path, SHGFI_LARGEICON, overlays, outOverlay);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadForShellName
//
//  An item by the shell's name for it, with the large icon its entry draws,
//  and with `overlays` its overlay, such as a cloud folder's sync state: as
//  an index in `outOverlay` when there is one to fill, for AddOverlay to draw
//  at the row's size, and drawn in otherwise. The shell item's own image when
//  the name does not parse.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadForShellName (const std::wstring & name, bool overlays, int * outOverlay)
{
    HRESULT           hr     = S_OK;
    PIDLIST_ABSOLUTE  pidl   = nullptr;
    SHFILEINFOW       info   = {};
    DWORD_PTR         result = 0;



    hr = SHParseDisplayName (name.c_str(), nullptr, &pidl, 0, nullptr);
    CHR (hr);

    result = SHGetFileInfoW ((LPCWSTR) pidl, 0, &info, sizeof (info), SHGFI_PIDL | SHGFI_ICON | SHGFI_LARGEICON | GetOverlayFlags (overlays, outOverlay));

    if (result != 0 && overlays && outOverlay != nullptr)
    {
        *outOverlay = info.iIcon >> 24;
    }

Error:
    CoTaskMemFree (pidl);

    return FAILED (hr)    ? LoadShellItemIcon (name, GetSystemMetrics (SM_CXICON), SIIGBF_ICONONLY)
         : (result != 0)  ? info.hIcon
                          : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadShellItemIcon
//
//  The image the shell draws for an item at this size, as an icon so it is
//  rasterized as every other one is: a folder's with the files in it showing,
//  or any item's own icon by the shell's name for it. None when the shell has
//  no image for it.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadShellItemIcon (const std::wstring & path, int sizePx, SIIGBF flags)
{
    HRESULT                         hr      = S_OK;
    ComPtr<IShellItemImageFactory>  factory;
    HBITMAP                         color   = nullptr;
    HBITMAP                         mask    = nullptr;
    ICONINFO                        info    = {};
    HICON                           icon    = nullptr;



    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&factory));
    CHR (hr);

    hr = factory->GetImage (SIZE { sizePx, sizePx }, flags, &color);
    CHR (hr);

    //  The bitmap's own alpha does the masking; the mask only has to exist.
    mask = CreateBitmap (sizePx, sizePx, 1, 1, nullptr);
    CWR (mask != nullptr);

    info.fIcon    = TRUE;
    info.hbmColor = color;
    info.hbmMask  = mask;

    icon = CreateIconIndirect (&info);
    CWR (icon != nullptr);

Error:
    if (color != nullptr)
    {
        DeleteObject (color);
    }

    if (mask != nullptr)
    {
        DeleteObject (mask);
    }

    return icon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadThumbnail
//
//  The shell's thumbnail, fitted into the square and set on its bottom
//  edge, centered, as File Explorer lays one out in its icon views. A
//  picture's thumbnail usually has no alpha, and is then made opaque.
//  Null when the shell fails to make a thumbnail of the item.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadThumbnail (const std::wstring & path, int sizePx)
{
    HRESULT                         hr       = S_OK;
    ComPtr<IShellItemImageFactory>  factory;
    HBITMAP                         thumb    = nullptr;
    HBITMAP                         color    = nullptr;
    HBITMAP                         mask     = nullptr;
    HDC                             screen   = nullptr;
    BITMAP                          size     = {};
    BITMAPINFO                      bmi      = {};
    uint32_t                      * square   = nullptr;
    std::vector<uint32_t>           pixels;
    ICONINFO                        info     = {};
    HICON                           icon     = nullptr;
    bool                            hasAlpha = false;
    int                             left     = 0;
    int                             top      = 0;
    int                             read     = 0;



    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&factory));
    CHR (hr);

    hr = factory->GetImage (SIZE { sizePx, sizePx }, SIIGBF_THUMBNAILONLY, &thumb);
    CHR (hr);

    read = GetObjectW (thumb, sizeof (size), &size);
    CBR (read != 0);
    CBR (size.bmWidth > 0 && size.bmHeight > 0 && size.bmWidth <= sizePx && size.bmHeight <= sizePx);

    bmi.bmiHeader.biSize        = sizeof (bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = size.bmWidth;
    bmi.bmiHeader.biHeight      = -size.bmHeight;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    pixels.resize ((size_t) size.bmWidth * (size_t) size.bmHeight);
    screen = GetDC (nullptr);
    CWR (screen != nullptr);
    read = GetDIBits (screen, thumb, 0, (UINT) size.bmHeight, pixels.data(), &bmi, DIB_RGB_COLORS);
    CBR (read != 0);

    bmi.bmiHeader.biWidth  = sizePx;
    bmi.bmiHeader.biHeight = -sizePx;

    color = CreateDIBSection (screen, &bmi, DIB_RGB_COLORS, (void **) &square, nullptr, 0);
    CWR (color != nullptr && square != nullptr);

    hasAlpha = std::any_of (pixels.begin(), pixels.end(), [] (uint32_t px) { return (px >> 24) != 0; });
    left     = (sizePx - size.bmWidth) / 2;
    top      = sizePx - size.bmHeight;

    std::fill (square, square + (size_t) sizePx * (size_t) sizePx, 0u);

    for (int y = 0; y < size.bmHeight; y++)
    {
        for (int x = 0; x < size.bmWidth; x++)
        {
            uint32_t  px = pixels[(size_t) y * (size_t) size.bmWidth + (size_t) x];

            square[(size_t) (top + y) * (size_t) sizePx + (size_t) (left + x)] = hasAlpha ? px : (px | 0xFF000000u);
        }
    }

    //  The bitmap's alpha does the masking; the mask only has to exist.
    mask = CreateBitmap (sizePx, sizePx, 1, 1, nullptr);
    CWR (mask != nullptr);

    info.fIcon    = TRUE;
    info.hbmColor = color;
    info.hbmMask  = mask;

    icon = CreateIconIndirect (&info);
    CWR (icon != nullptr);

Error:
    if (screen != nullptr)
    {
        ReleaseDC (nullptr, screen);
    }

    for (HBITMAP bitmap : { thumb, color, mask })
    {
        if (bitmap != nullptr)
        {
            DeleteObject (bitmap);
        }
    }

    return icon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadLargerForPath
//
//  The extra-large (48) or jumbo (256) image of the item's system image list
//  entry, whichever the size calls for; the large icon when neither loads.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadLargerForPath (const std::wstring & path, bool isDirectory, int sizePx)
{
    SHFILEINFOW          info       = {};
    DWORD                attributes = isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    UINT                 flags      = SHGFI_SYSICONINDEX;
    DWORD_PTR            result     = 0;
    ComPtr<IImageList>   list;
    HICON                icon       = nullptr;
    HRESULT              hr         = S_OK;
    int                  which      = (sizePx > s_kExtraLargeIconPx) ? SHIL_JUMBO : SHIL_EXTRALARGE;
    bool                 cropped    = false;



    //  An ordinary file shares its type's image, as the cache assumes. One
    //  asked about by its own path has its overlay too, as the shield on a
    //  program that runs elevated; the shell gives that only with the icon.
    if (!isDirectory && path.rfind (L'.') != std::wstring::npos && GetCacheKey (path, isDirectory).starts_with (L"*ext:"))
    {
        flags |= SHGFI_USEFILEATTRIBUTES;
    }
    else
    {
        flags |= SHGFI_ICON | SHGFI_OVERLAYINDEX;
    }

    result = SHGetFileInfoW (path.c_str(), attributes, &info, sizeof (info), flags);

    if (info.hIcon != nullptr)
    {
        DestroyIcon (info.hIcon);
    }

    if (result != 0)
    {
        int  image   = info.iIcon & 0x00FFFFFF;
        int  overlay = (info.iIcon >> 24) & 0xFF;

        hr = SHGetImageList (which, IID_PPV_ARGS (&list));

        if (SUCCEEDED (hr))
        {
            hr = list->GetIcon (image, ILD_TRANSPARENT, &icon);
        }

        icon = CropToCorner (icon, cropped);

        //  The overlay sits where the full image's corner is, so it is added
        //  only to an image the crop left whole.
        if (SUCCEEDED (hr) && overlay != 0 && !cropped)
        {
            DestroyIcon (icon);
            icon = nullptr;
            hr   = list->GetIcon (image, ILD_TRANSPARENT | INDEXTOOVERLAYMASK (overlay), &icon);
        }
    }

    return (icon != nullptr) ? icon : LoadForPath (path, SHGFI_LARGEICON);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::DeleteIconBitmaps
//
//  The color and mask bitmaps GetIconInfo makes, each deleted when present:
//  a monochrome icon has no color bitmap.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellIcons::DeleteIconBitmaps (const ICONINFO & info)
{
    if (info.hbmColor != nullptr)
    {
        DeleteObject (info.hbmColor);
    }

    if (info.hbmMask != nullptr)
    {
        DeleteObject (info.hbmMask);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::CropToCorner
//
//  A type with no image at a list's size gets a smaller one in the image's
//  top-left corner. Explorer draws that one scaled up to fill the cell, so the
//  icon is cut to the standard size square that holds what shows, and the
//  caller's scaling does the rest. An icon that fills its image is returned
//  as it is; one that is replaced is destroyed.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::CropToCorner (HICON icon, bool & outCropped)
{
    static constexpr int  kSizes[] = { 16, 20, 24, 32, 48, 64, 96, 128, 256 };



    HRESULT                hr      = S_OK;
    ICONINFO               info    = {};
    ICONINFO               cropped = {};
    BITMAP                 bitmap  = {};
    BITMAPINFO             format  = {};
    std::vector<uint32_t>  pixels;
    HDC                    dc      = GetDC (nullptr);
    void                 * bits    = nullptr;
    HICON                  result  = icon;
    int                    extent  = 0;
    int                    side    = 0;
    int                    lines   = 0;



    outCropped = false;

    BAIL_OUT_IF (icon == nullptr || !GetIconInfo (icon, &info) || info.hbmColor == nullptr, S_OK);
    BAIL_OUT_IF (GetObjectW (info.hbmColor, sizeof (bitmap), &bitmap) == 0, S_OK);

    format.bmiHeader.biSize        = sizeof (format.bmiHeader);
    format.bmiHeader.biWidth       = bitmap.bmWidth;
    format.bmiHeader.biHeight      = -bitmap.bmHeight;
    format.bmiHeader.biPlanes      = 1;
    format.bmiHeader.biBitCount    = 32;
    format.bmiHeader.biCompression = BI_RGB;

    pixels.resize ((size_t) bitmap.bmWidth * bitmap.bmHeight);
    lines = GetDIBits (dc, info.hbmColor, 0, (UINT) bitmap.bmHeight, pixels.data(), &format, DIB_RGB_COLORS);

    for (int y = 0; lines == bitmap.bmHeight && y < bitmap.bmHeight; y++)
    {
        for (int x = 0; x < bitmap.bmWidth; x++)
        {
            if ((pixels[(size_t) y * bitmap.bmWidth + x] >> 24) != 0)
            {
                extent = (std::max) (extent, (std::max) (x, y) + 1);
            }
        }
    }

    for (int size : kSizes)
    {
        if (side == 0 && size >= extent)
        {
            side = size;
        }
    }

    //  Most of the image shows: it is the icon at this size already.
    BAIL_OUT_IF (extent == 0 || side * 2 > bitmap.bmWidth, S_OK);

    format.bmiHeader.biWidth  = side;
    format.bmiHeader.biHeight = -side;

    cropped.fIcon    = TRUE;
    cropped.hbmColor = CreateDIBSection (dc, &format, DIB_RGB_COLORS, &bits, nullptr, 0);
    cropped.hbmMask  = CreateBitmap (side, side, 1, 1, nullptr);

    BAIL_OUT_IF (cropped.hbmColor == nullptr || cropped.hbmMask == nullptr || bits == nullptr, S_OK);

    for (int y = 0; y < side; y++)
    {
        memcpy (static_cast<uint32_t *> (bits) + (size_t) y * side, pixels.data() + (size_t) y * bitmap.bmWidth, (size_t) side * sizeof (uint32_t));
    }

    result = CreateIconIndirect (&cropped);

    if (result != nullptr)
    {
        outCropped = true;
        DestroyIcon (icon);
    }
    else
    {
        result = icon;
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
    DeleteIconBitmaps (cropped);
    DeleteIconBitmaps (info);
    ReleaseDC (nullptr, dc);

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetForKind
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetForKind (Kind kind)
{
    std::wstring  key   = GetSizedKey (L"*kind:" + std::to_wstring ((int) kind));
    UINT          flag  = SHGFI_LARGEICON;
    auto          found = m_cache.find (key);



    if (found != m_cache.end())
    {
        return found->second;
    }

    //  A disk image has Casso's own floppy, from this program's resources.
    if (kind == Kind::DiskImage)
    {
        return Remember (key, (HICON) LoadImageW (GetModuleHandleW (nullptr), MAKEINTRESOURCEW (IDI_DISK_IMAGE), IMAGE_ICON, m_sizePx, m_sizePx, 0));
    }

    //  An Apple II on a link is an apple, from this program's resources, in
    //  grays while it is not connected.
    if (kind == Kind::AppleMachine || kind == Kind::AppleMachineOffline)
    {
        return Remember (key, (HICON) LoadImageW (GetModuleHandleW (nullptr), MAKEINTRESOURCEW ((kind == Kind::AppleMachine) ? IDI_APPLE_MACHINE : IDI_APPLE_MACHINE_OFFLINE),
                                                  IMAGE_ICON, m_sizePx, m_sizePx, 0));
    }

    //  An Applesoft program is an Apple II monitor with BASIC on its screen.
    if (kind == Kind::AppleApplesoft)
    {
        return Remember (key, (HICON) LoadImageW (GetModuleHandleW (nullptr), MAKEINTRESOURCEW (IDI_APPLESOFT_FILE), IMAGE_ICON, m_sizePx, m_sizePx, 0));
    }

    //  Any other Apple file type's icon is Explorer's page with the type's
    //  badge, the page drawn at the icon's own size.
    if (kind >= Kind::AppleText && kind <= Kind::AppleRelocatable)
    {
        HICON  page = (m_sizePx > s_kLargeIconPx) ? LoadLargerByAttributes (false, m_sizePx) : LoadForKind (Kind::File, flag);

        m_cache[key] = DrawAppleTypeIcon (kind, Rasterize (page, m_sizePx), m_sizePx);
        return m_cache[key];
    }

    //  Past the large icon, the generic folder and file come from the shell's
    //  bigger image lists, as a host folder's own icon does.
    if ((kind == Kind::Folder || kind == Kind::File) && m_sizePx > s_kLargeIconPx)
    {
        return Remember (key, LoadLargerByAttributes (kind == Kind::Folder, m_sizePx));
    }

    return Remember (key, LoadForKind (kind, flag));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadLargerByAttributes
//
//  The generic folder or file at an extra large or jumbo size, from its
//  attributes alone, cut to the art when the image list pads it.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadLargerByAttributes (bool isDirectory, int sizePx)
{
    SHFILEINFOW         info    = {};
    ComPtr<IImageList>  list;
    HICON               icon    = nullptr;
    HRESULT             hr      = S_OK;
    bool                cropped = false;
    int                 which   = (sizePx > s_kExtraLargeIconPx) ? SHIL_JUMBO : SHIL_EXTRALARGE;
    DWORD_PTR           result  = SHGetFileInfoW (isDirectory ? L"folder" : L"file", isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL,
                                                  &info, sizeof (info), SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);



    if (result != 0)
    {
        hr = SHGetImageList (which, IID_PPV_ARGS (&list));
    }

    if (result != 0 && SUCCEEDED (hr))
    {
        hr = list->GetIcon (info.iIcon & 0x00FFFFFF, ILD_TRANSPARENT, &icon);
    }

    icon = CropToCorner (icon, cropped);

    return (icon != nullptr) ? icon : LoadForKind (isDirectory ? Kind::Folder : Kind::File, SHGFI_LARGEICON);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::DrawAppleTypeIcon
//
//  A rounded badge at the page's lower right in the type's color, holding
//  the letter DOS 3.3's catalog shows for it, so a long catalog reads at a
//  glance. Drawn here, from no one's artwork.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::DrawAppleTypeIcon (Kind kind, std::shared_ptr<DxuiIconImage> page, int sizePx)
{
    struct Badge
    {
        Kind      kind;
        wchar_t   letter;
        uint32_t  rgb;
    };

    static constexpr Badge  kBadges[] =
    {
        { Kind::AppleText,        L'T', 0x5A6B7D },
        { Kind::AppleApplesoft,   L'A', 0x2E8B3E },
        { Kind::AppleInteger,     L'I', 0x1A8A8A },
        { Kind::AppleBinary,      L'B', 0xD9741C },
        { Kind::AppleSystem,      L'S', 0xC4323A },
        { Kind::AppleRelocatable, L'R', 0x7A4CC2 },
    };

    std::vector<uint8_t>  letter;
    Badge                 badge   = kBadges[0];
    int                   side    = std::max (6, (sizePx * 5 + 4) / 8);
    int                   left    = sizePx - side;
    int                   top     = sizePx - side;
    float                 radius  = side * 0.22f;
    HRESULT               hr      = S_OK;



    if (page == nullptr || page->width != sizePx || page->height != sizePx)
    {
        page = std::make_shared<DxuiIconImage>();
        page->width  = sizePx;
        page->height = sizePx;
        page->bgraPremul.assign ((size_t) sizePx * sizePx, 0);
    }

    for (const Badge & each : kBadges)
    {
        if (each.kind == kind)
        {
            badge = each;
        }
    }

    hr = DrawLetterCoverage (badge.letter, side, letter);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int y = 0; y < side; y++)
    {
        for (int x = 0; x < side; x++)
        {
            float      cover  = GetRoundedCoverage (x, y, side, radius);
            float      ink    = letter.empty() ? 0.0f : letter[(size_t) y * side + x] / 255.0f;
            uint32_t & pixel  = page->bgraPremul[(size_t) (top + y) * sizePx + (left + x)];
            float      r      = ((badge.rgb >> 16) & 0xFF) * (1.0f - ink) + 255.0f * ink;
            float      g      = ((badge.rgb >>  8) & 0xFF) * (1.0f - ink) + 255.0f * ink;
            float      b      = ( badge.rgb        & 0xFF) * (1.0f - ink) + 255.0f * ink;
            float      keep   = 1.0f - cover;

            pixel = ((uint32_t) (cover * 255.0f + ((pixel >> 24) & 0xFF) * keep + 0.5f) << 24)
                  | ((uint32_t) (cover * r      + ((pixel >> 16) & 0xFF) * keep + 0.5f) << 16)
                  | ((uint32_t) (cover * g      + ((pixel >>  8) & 0xFF) * keep + 0.5f) <<  8)
                  |  (uint32_t) (cover * b      + ( pixel        & 0xFF) * keep + 0.5f);
        }
    }

    return page;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetRoundedCoverage
//
//  How much of one pixel a rounded square covers, sampled four by four.
//
////////////////////////////////////////////////////////////////////////////////

float Win32ShellIcons::GetRoundedCoverage (int x, int y, int side, float radius)
{
    int  inside = 0;



    for (int sy = 0; sy < 4; sy++)
    {
        for (int sx = 0; sx < 4; sx++)
        {
            float  px = x + (sx + 0.5f) / 4.0f;
            float  py = y + (sy + 0.5f) / 4.0f;
            float  cx = std::clamp (px, radius, side - radius);
            float  cy = std::clamp (py, radius, side - radius);
            float  dx = px - cx;
            float  dy = py - cy;

            inside += (dx * dx + dy * dy <= radius * radius) ? 1 : 0;
        }
    }

    return inside / 16.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::DrawLetterCoverage
//
//  The letter in Segoe UI Semibold, centered in a square of side pixels, as
//  one coverage byte a pixel.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellIcons::DrawLetterCoverage (wchar_t letter, int side, std::vector<uint8_t> & outCoverage)
{
    HRESULT     hr     = S_OK;
    HDC         dc     = CreateCompatibleDC (nullptr);
    HBITMAP     bitmap = nullptr;
    HFONT       font   = nullptr;
    void      * bits   = nullptr;
    BITMAPINFO  info   = {};
    RECT        box    = { 0, 0, side, side };



    outCoverage.clear();
    CWR (dc != nullptr);

    info.bmiHeader.biSize        = sizeof (info.bmiHeader);
    info.bmiHeader.biWidth       = side;
    info.bmiHeader.biHeight      = -side;
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = 32;
    info.bmiHeader.biCompression = BI_RGB;

    bitmap = CreateDIBSection (dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    CWR (bitmap != nullptr && bits != nullptr);

    font = CreateFontW (-(side * 3 / 4), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    CWR (font != nullptr);

    SelectObject (dc, bitmap);
    SelectObject (dc, font);
    SetBkMode    (dc, TRANSPARENT);
    SetTextColor (dc, RGB (255, 255, 255));
    DrawTextW    (dc, &letter, 1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();

    outCoverage.resize ((size_t) side * side);

    for (size_t i = 0; i < outCoverage.size(); i++)
    {
        outCoverage[i] = (uint8_t) ((static_cast<const uint32_t *> (bits)[i] >> 8) & 0xFF);
    }

Error:
    //  The DC first: an object selected into a DC cannot be deleted.
    if (dc != nullptr)
    {
        DeleteDC (dc);
    }

    if (font != nullptr)
    {
        DeleteObject (font);
    }

    if (bitmap != nullptr)
    {
        DeleteObject (bitmap);
    }

    return hr;
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

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::Remember (const std::wstring & key, HICON icon, int overlay)
{
    std::shared_ptr<DxuiIconImage>  image = Rasterize (icon, m_sizePx, m_dpi);



    if (image != nullptr && overlay > 0)
    {
        AddOverlay (*image, overlay);
    }

    StoreIcon (key, image);

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::Rasterize
//
//  Takes ownership of the handle and destroys it. No icon, or one that fails
//  to rasterize, is null.
//
//  A larger icon for a size up to the large icon's is drawn at its own size
//  and scaled down with a bilinear filter, which matches Explorer's rows
//  where GDI's stretch does not.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> Win32ShellIcons::Rasterize (HICON icon, int sizePx, UINT dpi)
{
    std::shared_ptr<DxuiIconImage>  image;
    HRESULT                         hr    = S_OK;
    ICONINFO                        info  = {};
    BITMAP                          art   = {};
    int                             artPx = 0;



    if (icon != nullptr && GetIconInfo (icon, &info))
    {
        if (info.hbmColor != nullptr && GetObjectW (info.hbmColor, sizeof (art), &art) != 0)
        {
            artPx = (int) art.bmWidth;
        }

        DeleteIconBitmaps (info);
    }

    //  An icon smaller than the requested size is drawn at its own pixel
    //  size, as Explorer draws a type with no art at the list's size, unscaled.
    if (dpi != 0 && artPx > 0)
    {
        sizePx = (std::min) (sizePx, artPx);
    }

    if (icon != nullptr)
    {
        bool  scaled = artPx > sizePx && sizePx <= s_kLargeIconPx;

        image = std::make_shared<DxuiIconImage>();
        hr    = DxuiIconImage::FromHicon (icon, scaled ? artPx : sizePx, *image);
        DestroyIcon (icon);

        if (SUCCEEDED (hr) && scaled)
        {
            *image = DxuiIconImage::Resample (*image, sizePx);
        }

        if (FAILED (hr))
        {
            image.reset();
        }
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetCacheKey
//
//  A folder or a drive can have its own icon, and so can a program, a
//  shortcut and an icon file; anything else looks like every other file with
//  its extension.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShellIcons::GetCacheKey (const std::wstring & path, bool isDirectory)
{
    size_t        dot   = path.rfind (L'.');
    size_t        slash = path.find_last_of (L"\\/");
    std::wstring  extension;



    if (isDirectory || dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash))
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

HICON Win32ShellIcons::LoadForPath (const std::wstring & path, UINT sizeFlag, bool overlays, int * outOverlay)
{
    SHFILEINFOW  info       = {};
    DWORD        attributes = GetFileAttributesW (path.c_str());
    UINT         flags      = SHGFI_ICON | sizeFlag;
    DWORD_PTR    result     = 0;



    //  A file that looks like every other of its type is asked about by type
    //  alone, without touching it: a cloud file not on the disk can fail the
    //  lookup, or wait on its download, and the answer would be the same.
    if (attributes == INVALID_FILE_ATTRIBUTES || (!(attributes & FILE_ATTRIBUTE_DIRECTORY) && GetCacheKey (path, false).starts_with (L"*ext:")))
    {
        attributes  = (attributes != INVALID_FILE_ATTRIBUTES) ? FILE_ATTRIBUTE_NORMAL
                    : (path.rfind (L'.') == std::wstring::npos) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        flags      |= SHGFI_USEFILEATTRIBUTES;
    }
    else
    {
        //  A file read by its own path has its overlay, such as a cloud
        //  file's sync state, as LoadForShellName has it.
        flags |= GetOverlayFlags (overlays, outOverlay);
    }

    result = SHGetFileInfoW (path.c_str(), attributes, &info, sizeof (info), flags);

    if (result != 0 && (flags & SHGFI_OVERLAYINDEX) != 0)
    {
        *outOverlay = info.iIcon >> 24;
    }

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
    SHSTOCKICONINFO   stock            = {};



    switch (kind)
    {
    case Kind::ThisPc:
    case Kind::RecycleBin:
        hr = SHGetKnownFolderIDList ((kind == Kind::ThisPc) ? FOLDERID_ComputerFolder : FOLDERID_RecycleBinFolder, 0, nullptr, &pidl);

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

    case Kind::Search:
        stock.cbSize = sizeof (stock);
        hr           = SHGetStockIconInfo (SIID_FIND, SHGSI_ICON | sizeFlag, &stock);

        return SUCCEEDED (hr) ? stock.hIcon : nullptr;

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
//  Win32ShellIcons::GetTypeName
//
//  From the file's attributes and extension alone, so no file is opened: every
//  file of one extension has one type name, which is also why the names are
//  cached by extension.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShellIcons::GetTypeName (const std::wstring & path, bool isDirectory)
{
    SHFILEINFOW   info  = {};
    size_t        slash = path.find_last_of (L"\\/");
    size_t        dot   = path.rfind (L'.');
    std::wstring  key   = isDirectory ? std::wstring (L"<folder>") : std::wstring (L".");
    DWORD_PTR     found = 0;



    if (!isDirectory && dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash))
    {
        key = path.substr (dot);

        for (wchar_t & c : key)
        {
            c = (wchar_t) towlower (c);
        }
    }

    auto  cached = m_typeNames.find (key);

    if (cached != m_typeNames.end())
    {
        return cached->second;
    }

    found = SHGetFileInfoW (path.c_str(), isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL,
                            &info, sizeof (info), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);

    m_typeNames[key] = (found != 0) ? std::wstring (info.szTypeName) : std::wstring();

    return m_typeNames[key];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetDriveInfo
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellIcons::GetDriveInfo (const std::wstring & root, DriveInfo & outInfo)
{
    SHFILEINFOW     info      = {};
    ULARGE_INTEGER  available = {};
    ULARGE_INTEGER  total     = {};
    DWORD_PTR       found     = SHGetFileInfoW (root.c_str(), 0, &info, sizeof (info), SHGFI_DISPLAYNAME | SHGFI_TYPENAME);



    if (found == 0)
    {
        return false;
    }

    outInfo.name     = info.szDisplayName;
    outInfo.typeName = info.szTypeName;

    if (GetDiskFreeSpaceExW (root.c_str(), &available, &total, nullptr))
    {
        outInfo.freeBytes  = available.QuadPart;
        outInfo.totalBytes = total.QuadPart;
    }

    return true;
}
