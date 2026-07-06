#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  UnicodeSymbols
//
//  Named wide-char constants for non-ASCII codepoints used in
//  user-facing strings (window titles, message-box bodies, menu
//  text, etc.). Add a new entry here whenever you need a glyph
//  outside the basic ASCII range — never inline `\xNNNN` /
//  `\uNNNN` escapes at the call site.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr wchar_t s_kchBullet         = L'\x2022';       // U+2022 BULLET (•)
static constexpr wchar_t s_kchEmDash         = L'\x2014';       // U+2014 EM DASH (—)
static constexpr wchar_t s_kchEllipsis       = L'\x2026';       // U+2026 HORIZONTAL ELLIPSIS (…)
static constexpr wchar_t s_kchAlmostEqual    = L'\x2248';       // U+2248 ALMOST EQUAL TO (≈)
static constexpr wchar_t s_kchDegree         = L'\x00B0';       // U+00B0 DEGREE SIGN (°)
static constexpr LPCWSTR s_kpszDegree        = L"\x00B0";       // U+00B0 DEGREE SIGN (°)
static constexpr LPCWSTR s_kpszCheckMark     = L"\x2713";       // U+2713 CHECK MARK (✓)
static constexpr LPCWSTR s_kpszTriangleUp    = L"\x25B2";       // U+25B2 BLACK UP-POINTING TRIANGLE (▲)
static constexpr LPCWSTR s_kpszTriangleDown  = L"\x25BC";       // U+25BC BLACK DOWN-POINTING TRIANGLE (▼)
static constexpr LPCWSTR s_kpszTriangleRight = L"\x25B6";       // U+25B6 BLACK RIGHT-POINTING TRIANGLE (▶)
static constexpr LPCWSTR s_kpszMultiplyX     = L"\x00D7";       // U+00D7 MULTIPLICATION SIGN (×), window-close glyph
static constexpr LPCWSTR s_kpszRocket        = L"\U0001F680";   // U+1F680 ROCKET (🚀)

// Segoe MDL2 Assets icon-font glyphs (private use area; render only with
// the "Segoe MDL2 Assets" family).
static constexpr LPCWSTR s_kpszMdl2Play      = L"\xE768";       // U+E768 Segoe MDL2 Play
static constexpr LPCWSTR s_kpszMdl2Copy      = L"\xE8C8";       // U+E8C8 Segoe MDL2 Copy
static constexpr LPCWSTR s_kpszMdl2Accept    = L"\xE73E";       // U+E73E Segoe MDL2 Accept (check mark)
