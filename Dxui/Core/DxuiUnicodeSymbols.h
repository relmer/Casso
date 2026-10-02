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
static constexpr wchar_t s_kchBlackCircle    = L'\x25CF';       // U+25CF BLACK CIRCLE
static constexpr LPCWSTR s_kpszMdl2ChevronDown = L"\xE70D";     // U+E70D Segoe MDL2 ChevronDown (a tool window's menu)
static constexpr LPCWSTR s_kpszMdl2Pin       = L"\xE718";       // U+E718 Segoe MDL2 Pin (a tool window's auto-hide)
static constexpr LPCWSTR s_kpszMdl2More      = L"\xE712";       // U+E712 Segoe MDL2 More (three dots)
static constexpr LPCWSTR s_kpszMdl2Cancel    = L"\xE711";       // U+E711 Segoe MDL2 Cancel (the clear button's X)
static constexpr LPCWSTR s_kpszMdl2WarningSolid = L"\xE814";    // U+E814 Segoe MDL2 warning triangle, filled
static constexpr LPCWSTR s_kpszTriangleRight = L"\x25B6";       // U+25B6 BLACK RIGHT-POINTING TRIANGLE (▶)
static constexpr LPCWSTR s_kpszTriangleLeft  = L"\x25C0";       // U+25C0 BLACK LEFT-POINTING TRIANGLE
static constexpr LPCWSTR s_kpszMdl2Accept    = L"\xE73E";       // U+E73E Segoe MDL2 Accept (check mark)
