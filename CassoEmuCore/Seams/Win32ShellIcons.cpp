#include "Pch.h"

#include "Core/ThreadName.h"

#include "Seams/Win32ShellIcons.h"
#include "resource.h"

#include "CassoExplorer/Model/LaunchCommand.h"





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
        m_cache[item.key] = std::move (item.image);
    }

    return !loaded.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::Queue
//
//  Asks the loader for a per-path icon, once, and answers for now with the
//  icon of the item's type.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::Queue (const std::wstring & key, const std::wstring & path, bool isDirectory)
{
    bool  added = false;



    {
        std::lock_guard<std::mutex>  guard (m_lock);

        added = m_pending.insert (key).second;

        if (added)
        {
            m_requests.push_back (Request { key, path, isDirectory, m_sizePx });
        }
    }

    if (added)
    {
        m_wake.notify_one();
    }

    return GetForKind (isDirectory ? Kind::Folder : Kind::File);
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
        bool                            post = false;

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

        image = Rasterize (LoadSized (request.path, request.isDirectory, request.sizePx), request.sizePx, m_dpi);

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



    if (FAILED (hr) || side <= 0)
    {
        return;
    }

    hr = DxuiIconImage::FromHicon (info.hIcon, side, shield);
    DestroyIcon (info.hIcon);

    if (FAILED (hr) || shield.width != side)
    {
        return;
    }

    for (int y = 0; y < side; y++)
    {
        for (int x = 0; x < side; x++)
        {
            uint32_t    top   = shield.bgraPremul[(size_t) y * side + x];
            uint32_t &  under = image.bgraPremul[(size_t) (image.height - side + y) * image.width + (image.width - side + x)];
            uint32_t    keep  = 255 - (top >> 24);
            uint32_t    out   = 0;

            for (int shift = 0; shift < 32; shift += 8)
            {
                out |= (((top >> shift) & 0xFF) + ((under >> shift) & 0xFF) * keep / 255) << shift;
            }

            under = out;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::GetForPath
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::GetForPath (const std::wstring & path, bool isDirectory)
{
    std::wstring  key   = GetSizedKey (GetCacheKey (path, isDirectory));
    auto          found = m_cache.find (key);



    if (found != m_cache.end())
    {
        return found->second;
    }

    //  A per-path icon is the slow kind; one by extension is quick, and the
    //  same for every file of its type.
    if (m_loader.joinable() && GetCacheKey (path, isDirectory).starts_with (L"*path:"))
    {
        return Queue (key, path, isDirectory);
    }

    return Remember (key, LoadSized (path, isDirectory, m_sizePx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::LoadSized
//
//  Past the large icon, the shell's bigger image lists have the art.
//
////////////////////////////////////////////////////////////////////////////////

HICON Win32ShellIcons::LoadSized (const std::wstring & path, bool isDirectory, int sizePx)
{
    HICON  preview = nullptr;



    //  An item that is not on a disk has no path for the file calls; the shell
    //  draws its icon from the item itself.
    if (path.starts_with (L"::") || _wcsnicmp (path.c_str(), L"shell:", 6) == 0)
    {
        return LoadShellItemIcon (path, sizePx, SIIGBF_ICONONLY);
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

    return LoadForPath (path, (sizePx <= 16) ? SHGFI_SMALLICON : SHGFI_LARGEICON);
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
    DeleteObject (cropped.hbmColor);
    DeleteObject (cropped.hbmMask);
    DeleteObject (info.hbmColor);
    DeleteObject (info.hbmMask);
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
    UINT          flag  = (m_sizePx <= 16) ? SHGFI_SMALLICON : SHGFI_LARGEICON;
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

    //  An Apple file type's icon is Explorer's page with the type's badge.
    if (kind >= Kind::AppleText)
    {
        m_cache[key] = DrawAppleTypeIcon (kind, Rasterize (LoadForKind (Kind::File, flag), m_sizePx), m_sizePx);
        return m_cache[key];
    }

    return Remember (key, LoadForKind (kind, flag));
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
    if (font != nullptr)
    {
        DeleteObject (font);
    }

    if (bitmap != nullptr)
    {
        DeleteObject (bitmap);
    }

    if (dc != nullptr)
    {
        DeleteDC (dc);
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

std::shared_ptr<const DxuiIconImage> Win32ShellIcons::Remember (const std::wstring & key, HICON icon)
{
    std::shared_ptr<const DxuiIconImage>  image = Rasterize (icon, m_sizePx, m_dpi);



    m_cache[key] = image;

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellIcons::Rasterize
//
//  Takes ownership of the handle and destroys it. No icon, or one that fails
//  to rasterize, is null.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> Win32ShellIcons::Rasterize (HICON icon, int sizePx, UINT dpi)
{
    std::shared_ptr<DxuiIconImage>  image;
    HRESULT                         hr    = S_OK;
    ICONINFO                        info  = {};
    BITMAP                          art   = {};



    //  An icon smaller than the size asked for keeps its own pixel size, as
    //  Explorer draws a type with no art at the list's size, unscaled.
    if (icon != nullptr && dpi != 0 && GetIconInfo (icon, &info))
    {
        if (info.hbmColor != nullptr && GetObjectW (info.hbmColor, sizeof (art), &art) != 0)
        {
            sizePx = (std::min) (sizePx, (int) art.bmWidth);
        }

        DeleteObject (info.hbmColor);
        DeleteObject (info.hbmMask);
    }

    if (icon != nullptr)
    {
        image = std::make_shared<DxuiIconImage>();
        hr    = DxuiIconImage::FromHicon (icon, sizePx, *image);
        DestroyIcon (icon);

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

HICON Win32ShellIcons::LoadForPath (const std::wstring & path, UINT sizeFlag)
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
        //  A file asked about by its own path has its overlays, as the shield
        //  on a program that runs elevated.
        flags |= SHGFI_ADDOVERLAYS;
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
