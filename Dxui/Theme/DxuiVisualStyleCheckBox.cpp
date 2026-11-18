#include "Pch.h"

#include "DxuiVisualStyleCheckBox.h"

#pragma comment(lib, "uxtheme.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiVisualStyleCheckBox::GetImage
//
//  The box for a DPI and state, rendered on first use. The image is empty
//  when no visual style is in effect, as in a high contrast theme, and the
//  caller then draws a plain box.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiIconImage & DxuiVisualStyleCheckBox::GetImage (UINT dpi, State state)
{
    HRESULT               hr    = S_OK;
    std::vector<Entry>  & cache = GetCache();
    Entry                 entry;



    for (const Entry & cached : cache)
    {
        if (cached.dpi == dpi && cached.state == state)
        {
            return cached.image;
        }
    }

    entry.dpi   = dpi;
    entry.state = state;

    //  A failed render is cached as an empty image, so the render runs once.
    hr = Render (dpi, state, entry.image);
    IGNORE_RETURN_VALUE (hr, S_OK);

    cache.push_back (std::move (entry));

    return cache.back().image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiVisualStyleCheckBox::GetSizePx
//
//  The box's width and height: the visual style's size for the DPI, or 13
//  DIPs, Windows' classic box, with no visual style.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiVisualStyleCheckBox::GetSizePx (UINT dpi)
{
    constexpr int          kClassicBoxDip = 13;
    constexpr int          kDefaultDpi    = 96;
    const DxuiIconImage &  image          = GetImage (dpi, State::Unchecked);



    return (image.width > 0) ? image.width : MulDiv (kClassicBoxDip, (int) dpi, kDefaultDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiVisualStyleCheckBox::Render
//
//  Draws the visual style's box into a cleared 32-bit bitmap. The style's art
//  is alpha-blended, and blending over zero produces premultiplied pixels,
//  the format of DxuiIconImage.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiVisualStyleCheckBox::Render (UINT dpi, State state, DxuiIconImage & outImage)
{
    HRESULT      hr       = S_OK;
    HTHEME       theme    = nullptr;
    HDC          dc       = nullptr;
    HBITMAP      dib      = nullptr;
    HGDIOBJ      previous = nullptr;
    void       * raw      = nullptr;
    uint32_t   * bits     = nullptr;
    BITMAPINFO   bmi      = {};
    SIZE         size     = {};
    RECT         bounds   = {};
    size_t       count    = 0;
    int          part     = GetPartState (state);



    theme = OpenThemeDataForDpi (nullptr, L"Button", dpi);
    CBR (theme != nullptr);

    hr = GetThemePartSize (theme, nullptr, BP_CHECKBOX, part, nullptr, TS_DRAW, &size);
    CHR (hr);
    CBR (size.cx > 0 && size.cy > 0);

    count = (size_t) size.cx * (size_t) size.cy;
    dc    = CreateCompatibleDC (nullptr);
    CPR (dc);

    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = size.cx;
    bmi.bmiHeader.biHeight      = -size.cy;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    dib = CreateDIBSection (dc, &bmi, DIB_RGB_COLORS, &raw, nullptr, 0);
    CPR (dib);
    CPR (raw);

    bits     = (uint32_t *) raw;
    previous = SelectObject (dc, dib);
    bounds   = RECT { 0, 0, size.cx, size.cy };

    std::fill (bits, bits + count, 0u);

    hr = DrawThemeBackground (theme, dc, BP_CHECKBOX, part, &bounds, nullptr);
    CHR (hr);

    GdiFlush();

    outImage.width  = size.cx;
    outImage.height = size.cy;
    outImage.bgraPremul.assign (bits, bits + count);

Error:
    if (previous != nullptr)
    {
        SelectObject (dc, previous);
    }

    if (dib != nullptr)
    {
        DeleteObject (dib);
    }

    if (dc != nullptr)
    {
        DeleteDC (dc);
    }

    if (theme != nullptr)
    {
        CloseThemeData (theme);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiVisualStyleCheckBox::GetPartState
//
////////////////////////////////////////////////////////////////////////////////

int DxuiVisualStyleCheckBox::GetPartState (State state)
{
    switch (state)
    {
    case State::Checked:
        return CBS_CHECKEDNORMAL;

    case State::CheckedDisabled:
        return CBS_CHECKEDDISABLED;

    default:
        return CBS_UNCHECKEDNORMAL;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiVisualStyleCheckBox::GetCache
//
//  The rendered boxes. Painting happens on the UI thread only, so the cache
//  has no lock.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiVisualStyleCheckBox::Entry> & DxuiVisualStyleCheckBox::GetCache()
{
    static std::vector<Entry>  entries;



    return entries;
}
