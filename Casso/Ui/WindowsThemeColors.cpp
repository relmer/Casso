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

    // Close-button hover and pressed share the same background red
    // (microsoft/terminal MinMaxCloseControl.xaml binds both
    // CloseButtonBackgroundPointerOver and CloseButtonBackgroundPressed
    // to the same CloseButtonColor token), but the WinUI button
    // visual-state template applies a press-state opacity tweak to
    // the glyph, producing a perceptibly darker X on click. We mirror
    // that visible behavior by dropping the glyph from opaque white
    // to a partially-transparent white on press; the alpha goes
    // through the painter's premultiplied-alpha blend over the red
    // background so the glyph reads as a slightly desaturated white.
    static constexpr uint32_t   s_kCloseButtonColor              = 0xFFC42B1Cu;
    static constexpr uint32_t   s_kCloseButtonGlyphHoverColor    = 0xFFFFFFFFu;
    static constexpr uint32_t   s_kCloseButtonGlyphPressedColor  = 0xCCFFFFFFu;
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
    return s_kCloseButtonColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CloseButtonPressedArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CloseButtonPressedArgb () const
{
    return s_kCloseButtonColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CloseButtonGlyphHoverArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CloseButtonGlyphHoverArgb () const
{
    return s_kCloseButtonGlyphHoverColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors::CloseButtonGlyphPressedArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WindowsThemeColors::CloseButtonGlyphPressedArgb () const
{
    return s_kCloseButtonGlyphPressedColor;
}
