#include "Pch.h"

#include "DxuiWindowsThemeColors.h"





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
    m_darkMode = !ReadAppsUseLightTheme();
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
