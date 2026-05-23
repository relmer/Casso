#include "Pch.h"

#include "WindowsThemeColors.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Fluent caption-button color tokens
//
//  Token values lifted from the authoritative WinUI XAML resource
//  dictionaries. Each pair maps the canonical Fluent token name to its
//  exact ARGB value for the dark and light system themes.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr uint32_t   s_kSubtleFillColorSecondaryDark  = 0x0FFFFFFFu;
    static constexpr uint32_t   s_kSubtleFillColorTertiaryDark   = 0x0AFFFFFFu;
    static constexpr uint32_t   s_kSubtleFillColorSecondaryLight = 0x09000000u;
    static constexpr uint32_t   s_kSubtleFillColorTertiaryLight  = 0x06000000u;

    // Close-button hover and pressed colors. The WinUI XAML resource
    // dictionary (microsoft/terminal MinMaxCloseControl.xaml) sets
    // both states to the same CloseButtonColor token, but the
    // generic Button visual-state template applies an opacity tweak
    // on Pressed that produces a visibly distinct darker shade in
    // real Windows shell apps (File Explorer, Edge, Terminal). We
    // mirror that observed behavior with discrete ARGB values rather
    // than animating opacity at paint time.
    static constexpr uint32_t   s_kCloseButtonHoverColor         = 0xFFC42B1Cu;
    static constexpr uint32_t   s_kCloseButtonPressedColor       = 0xFFAA1F11u;
    static constexpr uint32_t   s_kCloseButtonGlyphOverColor     = 0xFFFFFFFFu;
    static constexpr uint32_t   s_kCaptionForegroundDark         = 0xFFFFFFFFu;
    static constexpr uint32_t   s_kCaptionForegroundLight        = 0xFF1A1A1Au;

    static constexpr LPCWSTR    s_kpszPersonalizeSubkey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    static constexpr LPCWSTR    s_kpszAppsUseLightTheme = L"AppsUseLightTheme";
}





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

namespace
{
    bool ReadAppsUseLightTheme ()
    {
        HKEY    hKey  = nullptr;
        DWORD   value = 1;
        DWORD   size  = sizeof (value);
        LSTATUS rc    = ERROR_SUCCESS;



        rc = RegOpenKeyExW (HKEY_CURRENT_USER,
                            s_kpszPersonalizeSubkey,
                            0,
                            KEY_READ,
                            &hKey);
        if (rc != ERROR_SUCCESS)
        {
            return true;
        }

        rc = RegQueryValueExW (hKey,
                               s_kpszAppsUseLightTheme,
                               nullptr,
                               nullptr,
                               reinterpret_cast<Byte *> (&value),
                               &size);
        RegCloseKey (hKey);

        if (rc != ERROR_SUCCESS)
        {
            return true;
        }

        return value != 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::Instance
//
////////////////////////////////////////////////////////////////////////////////

WindowsThemeColors & WindowsThemeColors::Instance ()
{
    static WindowsThemeColors  s_instance;

    return s_instance;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::WindowsThemeColors
//
////////////////////////////////////////////////////////////////////////////////

WindowsThemeColors::WindowsThemeColors ()
{
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void WindowsThemeColors::Refresh ()
{
    m_darkMode = !ReadAppsUseLightTheme();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CaptionButtonHoverArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CaptionButtonHoverArgb () const
{
    return m_darkMode ? s_kSubtleFillColorSecondaryDark : s_kSubtleFillColorSecondaryLight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CaptionButtonPressedArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CaptionButtonPressedArgb () const
{
    return m_darkMode ? s_kSubtleFillColorTertiaryDark : s_kSubtleFillColorTertiaryLight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CaptionButtonForegroundArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CaptionButtonForegroundArgb () const
{
    return m_darkMode ? s_kCaptionForegroundDark : s_kCaptionForegroundLight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CloseButtonHoverArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CloseButtonHoverArgb () const
{
    return s_kCloseButtonHoverColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CloseButtonPressedArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CloseButtonPressedArgb () const
{
    return s_kCloseButtonPressedColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CloseButtonForegroundOverArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CloseButtonForegroundOverArgb () const
{
    return s_kCloseButtonGlyphOverColor;
}
