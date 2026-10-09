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
//  The ellipsis, check mark and sort triangles are Dxui's and come from
//  Dxui/Core/DxuiUnicodeSymbols.h through Dxui.h.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr wchar_t s_kchBullet         = L'\x2022';       // U+2022 BULLET (•)
static constexpr wchar_t s_kchEmDash         = L'\x2014';       // U+2014 EM DASH (—)
static constexpr wchar_t s_kchAlmostEqual    = L'\x2248';       // U+2248 ALMOST EQUAL TO (≈)
static constexpr wchar_t s_kchDegree         = L'\x00B0';       // U+00B0 DEGREE SIGN (°)
static constexpr LPCWSTR s_kpszDegree        = L"\x00B0";       // U+00B0 DEGREE SIGN (°)
static constexpr LPCWSTR s_kpszMicro         = L"\x00B5";       // U+00B5 MICRO SIGN (µ)
static constexpr LPCWSTR s_kpszBallotX       = L"\x2717";       // U+2717 BALLOT X (✗)
static constexpr LPCWSTR s_kpszEmptySet      = L"\x2205";       // U+2205 EMPTY SET (∅)
static constexpr LPCWSTR s_kpszTriangleRight = L"\x25B6";       // U+25B6 BLACK RIGHT-POINTING TRIANGLE (▶)
static constexpr LPCWSTR s_kpszRightArrow    = L"\x2192";       // U+2192 RIGHTWARDS ARROW
static constexpr LPCWSTR s_kpszMultiplyX     = L"\x00D7";       // U+00D7 MULTIPLICATION SIGN (×), window-close glyph
static constexpr LPCWSTR s_kpszRocket        = L"\U0001F680";   // U+1F680 ROCKET (🚀)
static constexpr LPCWSTR s_kpszStar          = L"\x2B50";       // U+2B50 WHITE MEDIUM STAR (gold via color-emoji font)
static constexpr LPCWSTR s_kpszLock          = L"\U0001F512";   // U+1F512 LOCK (brass via color-emoji font)
static constexpr wchar_t s_kchThinSpace      = L'\x2009';       // U+2009 THIN SPACE
static constexpr LPCWSTR s_kpszGreenCheck    = L"\x2705";       // U+2705 WHITE HEAVY CHECK MARK (✅, green via color-emoji font)
static constexpr LPCWSTR s_kpszRedCross      = L"\x274C";       // U+274C CROSS MARK (❌, red via color-emoji font)
static constexpr LPCWSTR s_kpszGrayQuestion  = L"\x2754";       // U+2754 WHITE QUESTION MARK ORNAMENT (❔, gray via color-emoji font)

// Segoe MDL2 Assets icon-font glyphs (private use area; render only with
// the "Segoe MDL2 Assets" family).
static constexpr LPCWSTR s_kpszMdl2Play      = L"\xE768";       // U+E768 Segoe MDL2 Play
static constexpr LPCWSTR s_kpszMdl2Copy      = L"\xE8C8";       // U+E8C8 Segoe MDL2 Copy
static constexpr LPCWSTR s_kpszMdl2Accept    = L"\xE73E";       // U+E73E Segoe MDL2 Accept (check mark)
static constexpr LPCWSTR s_kpszMdl2Info      = L"\xE946";       // U+E946 Segoe MDL2 Info ("i" in a ring)
static constexpr LPCWSTR s_kpszMdl2Warning   = L"\xE7BA";       // U+E7BA Segoe MDL2 Warning (outlined triangle)
static constexpr LPCWSTR s_kpszMdl2Download  = L"\xE896";       // U+E896 Segoe MDL2 Download (arrow down onto a line)

// Xbox controller inputs, as Segoe MDL2 Assets draws them.
static constexpr LPCWSTR s_kpszMdl2ButtonA       = L"\xF093";   // U+F093 Segoe MDL2 ButtonA (circled A)
static constexpr LPCWSTR s_kpszMdl2ButtonB       = L"\xF094";   // U+F094 Segoe MDL2 ButtonB (circled B)
static constexpr LPCWSTR s_kpszMdl2ButtonY       = L"\xF095";   // U+F095 Segoe MDL2 ButtonY (circled Y)
static constexpr LPCWSTR s_kpszMdl2ButtonX       = L"\xF096";   // U+F096 Segoe MDL2 ButtonX (circled X)
static constexpr LPCWSTR s_kpszMdl2LeftStick     = L"\xF108";   // U+F108 Segoe MDL2 LeftStick
static constexpr LPCWSTR s_kpszMdl2RightStick    = L"\xF109";   // U+F109 Segoe MDL2 RightStick
static constexpr LPCWSTR s_kpszMdl2TriggerLeft   = L"\xF10A";   // U+F10A Segoe MDL2 TriggerLeft (LT)
static constexpr LPCWSTR s_kpszMdl2TriggerRight  = L"\xF10B";   // U+F10B Segoe MDL2 TriggerRight (RT)
static constexpr LPCWSTR s_kpszMdl2BumperLeft    = L"\xF10C";   // U+F10C Segoe MDL2 BumperLeft (LB)
static constexpr LPCWSTR s_kpszMdl2BumperRight   = L"\xF10D";   // U+F10D Segoe MDL2 BumperRight (RB)
static constexpr LPCWSTR s_kpszMdl2Dpad          = L"\xF10E";   // U+F10E Segoe MDL2 Dpad (plus shape)
static constexpr LPCWSTR s_kpszMdl2ButtonMenu    = L"\xEDE3";   // U+EDE3 Segoe MDL2 ButtonMenu (lines in a circle)
static constexpr LPCWSTR s_kpszMdl2ButtonView    = L"\xEECA";   // U+EECA Segoe MDL2 ButtonView (squares in a circle)

// Casso's own symbol font (Resources/Fonts/CassoSymbols.ttf, embedded and
// registered by AssetBootstrap::RegisterSymbolFont). These need no family at
// the call site: the renderer maps the range below onto that font through
// DirectWrite fallback, so they read as ordinary characters in any string.
//
// The two Apple keys of a //e or //c keyboard. Both keycaps carry the same
// apple and only the fill tells them apart, which is why no shipping font
// has the pair.
static constexpr LPCWSTR s_kpszOpenApple     = L"\xE000";       // U+E000 open Apple (outline)
static constexpr LPCWSTR s_kpszClosedApple   = L"\xE001";       // U+E001 closed Apple (filled)

// The range registered for that font. Keep it tight: the private use area is
// shared with Segoe MDL2 above, and a mapping that reached those codepoints
// would steal them.
static constexpr uint32_t s_kSymbolFontFirst = 0xE000;
static constexpr uint32_t s_kSymbolFontLast  = 0xE001;
