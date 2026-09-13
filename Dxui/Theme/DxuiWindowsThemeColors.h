#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowsThemeColors
//
//  Runtime accessor for Windows-system theme colors used by the custom
//  chrome (caption buttons, title-bar accents). Reads the current
//  Windows light/dark mode from the per-user theme registry value and
//  surfaces the authoritative Fluent token values for the active theme.
//
//  Sources for the token values used here:
//   * microsoft/microsoft-ui-xaml -- Common_themeresources_any.xaml
//     defines `SubtleFillColorSecondary` (hover) and
//     `SubtleFillColorTertiary` (pressed) for both light and dark.
//   * microsoft/terminal -- MinMaxCloseControl.xaml wires the WinUI
//     tokens to the caption buttons and pins `CloseButtonColor` to
//     #C42B1C for both hover and pressed in both light and dark.
//
//  Refresh model: `Refresh()` re-reads the registry. Cheap; safe to
//  call in response to `WM_SETTINGCHANGE` with `lParam` pointing at
//  the `"ImmersiveColorSet"` string.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiWindowsThemeColors
{
public:
    static DxuiWindowsThemeColors & Instance();

    //  Colors Windows provides for the signed-in user: the accent ramp from the
    //  registry and the list surface from the visual style. Each half has its
    //  own flag, since either can be unavailable (no visual styles, a remote
    //  session, no accent ever chosen), and a theme then uses its built-in
    //  values for that half.
    struct SystemColors
    {
        bool      hasAccent    = false;
        uint32_t  accentLight2 = 0;
        uint32_t  accentLight3 = 0;
        uint32_t  accentDark1  = 0;
        uint32_t  accentDark2  = 0;
        bool      hasSurfaces  = false;
        uint32_t  contentDark  = 0;
        uint32_t  contentLight = 0;
    };

    void  Refresh();
    bool  IsDarkMode() const { return m_darkMode; }

    const SystemColors &  GetSystemColors() const { return m_system; }

    //  The registry's AccentPalette is eight RGBA entries, lightest first:
    //  Light3, Light2, Light1, the accent, Dark1, Dark2, Dark3. Public so the
    //  decoding can be checked against bytes read off a real machine.
    static bool      DecodeAccentPalette (std::span<const BYTE> bytes, SystemColors & colors);
    static uint32_t  ToArgb (COLORREF color);

    uint32_t  CaptionButtonHoverArgb        () const;
    uint32_t  CaptionButtonPressedArgb      () const;
    uint32_t  CaptionButtonForegroundArgb   () const;
    uint32_t  CloseButtonHoverArgb           () const;
    uint32_t  CloseButtonPressedArgb         () const;
    uint32_t  CloseButtonGlyphHoverArgb      () const;
    uint32_t  CloseButtonGlyphPressedArgb    () const;

    // The Fluent tokens, public so a palette can be built from the same
    // values the caption buttons use.
    static constexpr uint32_t   kSubtleFillColorSecondaryDark  = 0x0FFFFFFFu;
    static constexpr uint32_t   kSubtleFillColorTertiaryDark   = 0x0AFFFFFFu;
    static constexpr uint32_t   kSubtleFillColorSecondaryLight = 0x09000000u;
    static constexpr uint32_t   kSubtleFillColorTertiaryLight  = 0x06000000u;

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
    static constexpr uint32_t   kCloseButtonColor              = 0xFFC42B1Cu;
    static constexpr uint32_t   kCloseButtonGlyphHoverColor    = 0xFFFFFFFFu;
    static constexpr uint32_t   kCloseButtonGlyphPressedColor  = 0xCCFFFFFFu;
    static constexpr uint32_t   kCaptionForegroundDark         = 0xFFFFFFFFu;
    static constexpr uint32_t   kCaptionForegroundLight        = 0xFF1A1A1Au;

private:
    DxuiWindowsThemeColors();

    static constexpr LPCWSTR    kpszPersonalizeSubkey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    static constexpr LPCWSTR    kpszAppsUseLightTheme = L"AppsUseLightTheme";
    static constexpr LPCWSTR    kpszAccentSubkey      =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent";
    static constexpr LPCWSTR    kpszAccentPalette     = L"AccentPalette";

    //  The visual-style classes Explorer's own list is drawn from. The dark
    //  one is undocumented but has been stable since 1809.
    static constexpr LPCWSTR    kpszItemsViewLight    = L"Explorer::ItemsView";
    static constexpr LPCWSTR    kpszItemsViewDark     = L"DarkMode_Explorer::ItemsView";

    static constexpr size_t     kAccentPaletteBytes   = 32;

    // Returns true when the per-user "AppsUseLightTheme" flag is set
    // (light mode). Absent value defaults to true (light) to match the
    // Windows default and avoid mis-rendering on systems that have never
    // toggled the setting.
    static bool  ReadAppsUseLightTheme();

    static void  ReadAccentPalette   (SystemColors & colors);
    static void  ReadItemsViewColors (SystemColors & colors);
    static bool      ReadThemeFill       (LPCWSTR themeClass, uint32_t & outArgb);
    static uint32_t  GetPaletteEntry     (std::span<const BYTE> bytes, size_t index);

    bool          m_darkMode = true;
    SystemColors  m_system;
};
