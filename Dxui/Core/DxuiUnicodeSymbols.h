#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiUnicodeSymbols
//
//  Named wide-char constants for the non-ASCII glyphs Dxui's widgets draw.
//  Consumers keep their own glyphs in their own header.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr wchar_t s_kchEllipsis       = L'\x2026';       // U+2026 HORIZONTAL ELLIPSIS (…)
static constexpr LPCWSTR s_kpszCheckMark     = L"\x2713";       // U+2713 CHECK MARK (✓)
static constexpr LPCWSTR s_kpszTriangleUp    = L"\x25B2";       // U+25B2 BLACK UP-POINTING TRIANGLE (▲)
static constexpr LPCWSTR s_kpszTriangleDown  = L"\x25BC";       // U+25BC BLACK DOWN-POINTING TRIANGLE (▼)
static constexpr LPCWSTR s_kpszTriangleRight = L"\x25B6";       // U+25B6 BLACK RIGHT-POINTING TRIANGLE
static constexpr LPCWSTR s_kpszTriangleLeft  = L"\x25C0";       // U+25C0 BLACK LEFT-POINTING TRIANGLE

// Segoe MDL2 Assets icon-font glyphs (private use area; render only with
// the "Segoe MDL2 Assets" family).
static constexpr LPCWSTR s_kpszMdl2Add          = L"\xE710";    // U+E710 Segoe MDL2 Add
static constexpr LPCWSTR s_kpszMdl2More         = L"\xE712";    // U+E712 Segoe MDL2 More (three dots)
static constexpr LPCWSTR s_kpszMdl2Cancel       = L"\xE711";    // U+E711 Segoe MDL2 Cancel (the clear button's X)
static constexpr LPCWSTR s_kpszMdl2ChevronDown  = L"\xE70D";    // U+E70D Segoe MDL2 ChevronDown
static constexpr LPCWSTR s_kpszMdl2ChevronLeft  = L"\xE76B";    // U+E76B Segoe MDL2 ChevronLeft
static constexpr LPCWSTR s_kpszMdl2ChevronRight = L"\xE76C";    // U+E76C Segoe MDL2 ChevronRight
static constexpr LPCWSTR s_kpszMdl2Calendar     = L"\xE787";    // U+E787 Segoe MDL2 Calendar
static constexpr LPCWSTR s_kpszMdl2ChromeClose  = L"\xE8BB";    // U+E8BB Segoe MDL2 ChromeClose (a window or tab close X)
static constexpr LPCWSTR s_kpszMdl2WarningSolid = L"\xE814";    // U+E814 Segoe MDL2 warning triangle, filled
static constexpr LPCWSTR s_kpszMdl2Pinned       = L"\xE840";    // U+E840 Segoe MDL2 Pinned (outline, with its needle)
static constexpr LPCWSTR s_kpszMdl2PinnedFill   = L"\xE842";    // U+E842 Segoe MDL2 PinnedFill (the head, filled)
