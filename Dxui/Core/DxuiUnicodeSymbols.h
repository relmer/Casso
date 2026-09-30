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
