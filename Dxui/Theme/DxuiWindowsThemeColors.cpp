#include "Pch.h"

#include "DxuiWindowsThemeColors.h"

#pragma comment(lib, "uxtheme.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Fluent caption-button color tokens
//
//  Token values lifted from the authoritative WinUI XAML resource
//  dictionaries. Each pair maps the canonical Fluent token name to its
//  exact ARGB value for the dark and light system themes.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  ReadAppsUseLightTheme
//
//  Returns true when the per-user "AppsUseLightTheme" flag is set
//  (light mode). Absent value defaults to true (light) to match the
//  Windows default and avoid mis-rendering on systems that have never
//  toggled the setting.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindowsThemeColors::ReadAppsUseLightTheme()
{
    HKEY    hKey  = nullptr;
    DWORD   value = 1;
    DWORD   size  = sizeof (value);
    LSTATUS rc    = ERROR_SUCCESS;



    // `value` is pre-seeded to 1 (light), so a missing key or a failed query
    // both fall through to the Windows default rather than needing their own
    // return. Only a successful read overwrites it.
    rc = RegOpenKeyExW (HKEY_CURRENT_USER,
                        kpszPersonalizeSubkey,
                        0,
                        KEY_READ,
                        &hKey);

    if (rc == ERROR_SUCCESS)
    {
        rc = RegQueryValueExW (hKey,
                               kpszAppsUseLightTheme,
                               nullptr,
                               nullptr,
                               reinterpret_cast<BYTE *> (&value),
                               &size);
        RegCloseKey (hKey);

        // A failed query may still have scribbled on `value`, so restore the
        // default explicitly rather than trusting it.
        if (rc != ERROR_SUCCESS)
        {
            value = 1;
        }
    }

    return value != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::Instance
//
////////////////////////////////////////////////////////////////////////////////

DxuiWindowsThemeColors & DxuiWindowsThemeColors::Instance()
{
    static DxuiWindowsThemeColors  s_instance;



    return s_instance;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::DxuiWindowsThemeColors
//
////////////////////////////////////////////////////////////////////////////////

DxuiWindowsThemeColors::DxuiWindowsThemeColors()
{
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindowsThemeColors::Refresh()
{
    SystemColors  colors;



    m_darkMode = !ReadAppsUseLightTheme();

    ReadAccentPalette   (colors);
    ReadItemsViewColors (colors);

    m_system = colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::ToArgb
//
//  A COLORREF is 0x00BBGGRR; Dxui paints in opaque 0xAARRGGBB.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::ToArgb (COLORREF color)
{
    return 0xFF000000u
         | ((uint32_t) GetRValue (color) << 16)
         | ((uint32_t) GetGValue (color) << 8)
         |  (uint32_t) GetBValue (color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::DecodeAccentPalette
//
//  Fluent uses the accent one step lighter on a dark surface and one step
//  darker on a light one, so a dark theme's accent is Light2 and a light
//  theme's is Dark1, each with the next step for hover. UISettings returns the
//  same values; the registry is read instead so the library does not use WinRT.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindowsThemeColors::DecodeAccentPalette (std::span<const BYTE> bytes, SystemColors & colors)
{
    static constexpr size_t  kLight3 = 0;
    static constexpr size_t  kLight2 = 1;
    static constexpr size_t  kDark1  = 4;
    static constexpr size_t  kDark2  = 5;



    if (bytes.size() < kAccentPaletteBytes)
    {
        return false;
    }

    colors.accentLight3 = GetPaletteEntry (bytes, kLight3);
    colors.accentLight2 = GetPaletteEntry (bytes, kLight2);
    colors.accentDark1  = GetPaletteEntry (bytes, kDark1);
    colors.accentDark2  = GetPaletteEntry (bytes, kDark2);
    colors.hasAccent    = true;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::GetPaletteEntry
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::GetPaletteEntry (std::span<const BYTE> bytes, size_t index)
{
    const BYTE *  at = bytes.data() + index * 4;



    return 0xFF000000u | ((uint32_t) at[0] << 16) | ((uint32_t) at[1] << 8) | (uint32_t) at[2];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::ReadAccentPalette
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindowsThemeColors::ReadAccentPalette (SystemColors & colors)
{
    HKEY     hKey                       = nullptr;
    BYTE     bytes[kAccentPaletteBytes] = {};
    DWORD    size                       = sizeof (bytes);
    LSTATUS  rc                         = ERROR_SUCCESS;
    bool     decoded                    = false;



    rc = RegOpenKeyExW (HKEY_CURRENT_USER, kpszAccentSubkey, 0, KEY_READ, &hKey);

    if (rc != ERROR_SUCCESS)
    {
        return;
    }

    rc = RegQueryValueExW (hKey, kpszAccentPalette, nullptr, nullptr, bytes, &size);
    RegCloseKey (hKey);

    if (rc == ERROR_SUCCESS)
    {
        decoded = DecodeAccentPalette (std::span<const BYTE> (bytes, size), colors);
        IGNORE_RETURN_VALUE (decoded, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::ReadItemsViewColors
//
//  Both modes are read regardless of the active mode, so a window set to Light
//  while Windows is dark still gets the system's light surface color. Measured
//  2026-09-12: the dark class returns #191919, the value measured in Explorer.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindowsThemeColors::ReadItemsViewColors (SystemColors & colors)
{
    bool  dark  = ReadThemeFill (kpszItemsViewDark,  colors.contentDark);
    bool  light = ReadThemeFill (kpszItemsViewLight, colors.contentLight);



    colors.hasSurfaces = dark && light;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::ReadThemeFill
//
//  The list item's fill as the visual style declares it. No window is needed
//  to open the data, which is what lets this run before the first one exists.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindowsThemeColors::ReadThemeFill (LPCWSTR themeClass, uint32_t & outArgb)
{
    HTHEME    theme  = OpenThemeData (nullptr, themeClass);
    COLORREF  fill   = 0;
    HRESULT   hrFill = E_FAIL;



    if (theme == nullptr)
    {
        return false;
    }

    hrFill = GetThemeColor (theme, LVP_LISTITEM, LISS_NORMAL, TMT_FILLCOLOR, &fill);
    CloseThemeData (theme);

    if (FAILED (hrFill))
    {
        return false;
    }

    outArgb = ToArgb (fill);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::CaptionButtonHoverArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::CaptionButtonHoverArgb() const
{
    return m_darkMode ? kSubtleFillColorSecondaryDark : kSubtleFillColorSecondaryLight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::CaptionButtonPressedArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::CaptionButtonPressedArgb() const
{
    return m_darkMode ? kSubtleFillColorTertiaryDark : kSubtleFillColorTertiaryLight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::CaptionButtonForegroundArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::CaptionButtonForegroundArgb() const
{
    return m_darkMode ? kCaptionForegroundDark : kCaptionForegroundLight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::CloseButtonHoverArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::CloseButtonHoverArgb() const
{
    return kCloseButtonColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::CloseButtonPressedArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::CloseButtonPressedArgb() const
{
    return kCloseButtonColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::CloseButtonGlyphHoverArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::CloseButtonGlyphHoverArgb() const
{
    return kCloseButtonGlyphHoverColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors::CloseButtonGlyphPressedArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiWindowsThemeColors::CloseButtonGlyphPressedArgb() const
{
    return kCloseButtonGlyphPressedColor;
}
