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
//  The ellipsis, check mark, the four triangles, and the Segoe MDL2 glyphs
//  Dxui's widgets draw (Add, More, Cancel, ChevronDown, ChromeClose and
//  WarningSolid) are Dxui's and come from Dxui/Core/DxuiUnicodeSymbols.h
//  through Dxui.h.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr wchar_t s_kchBullet         = L'\x2022';       // U+2022 BULLET (•)
static constexpr wchar_t s_kchEmDash         = L'\x2014';       // U+2014 EM DASH (—)
static constexpr wchar_t s_kchEnDash         = L'\x2013';       // U+2013 EN DASH, for ranges
static constexpr wchar_t s_kchAlmostEqual    = L'\x2248';       // U+2248 ALMOST EQUAL TO (≈)
static constexpr wchar_t s_kchDegree         = L'\x00B0';       // U+00B0 DEGREE SIGN (°)
static constexpr LPCWSTR s_kpszDegree        = L"\x00B0";       // U+00B0 DEGREE SIGN (°)
static constexpr LPCWSTR s_kpszBullet        = L"\x2022";       // U+2022 BULLET, the mark of a chosen radio row
static constexpr LPCWSTR s_kpszChevronRight  = L"\x203A";       // U+203A SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
static constexpr LPCWSTR s_kpszRightArrow    = L"\x2192";       // U+2192 RIGHTWARDS ARROW
static constexpr LPCWSTR s_kpszMultiplyX     = L"\x00D7";       // U+00D7 MULTIPLICATION SIGN (×), window-close glyph
static constexpr LPCWSTR s_kpszRocket        = L"\U0001F680";   // U+1F680 ROCKET (🚀)
static constexpr LPCWSTR s_kpszStar          = L"\x2B50";       // U+2B50 WHITE MEDIUM STAR (gold via color-emoji font)
static constexpr LPCWSTR s_kpszLock          = L"\U0001F512";   // U+1F512 LOCK (brass via color-emoji font)

// Segoe MDL2 Assets icon-font glyphs (private use area; render only with
// the "Segoe MDL2 Assets" family).
static constexpr LPCWSTR s_kpszMdl2Play      = L"\xE768";       // U+E768 Segoe MDL2 Play
static constexpr LPCWSTR s_kpszMdl2Copy      = L"\xE8C8";       // U+E8C8 Segoe MDL2 Copy
static constexpr LPCWSTR s_kpszMdl2Cut       = L"\xE8C6";       // U+E8C6 Segoe MDL2 Cut (scissors)
static constexpr LPCWSTR s_kpszMdl2Paste     = L"\xE77F";       // U+E77F Segoe MDL2 Paste (clipboard)
static constexpr LPCWSTR s_kpszMdl2Rename    = L"\xE8AC";       // U+E8AC Segoe MDL2 Rename (A in a text box)
static constexpr LPCWSTR s_kpszMdl2Delete    = L"\xE74D";       // U+E74D Segoe MDL2 Delete (trash can)
static constexpr LPCWSTR s_kpszMdl2Share     = L"\xE72D";       // U+E72D Segoe MDL2 Share (box with arrow)
static constexpr LPCWSTR s_kpszMdl2ThisPc    = L"\xE7F4";       // U+E7F4 Segoe MDL2 TVMonitor (Explorer's This PC in the address bar)
static constexpr LPCWSTR s_kpszMdl2Sort      = L"\xE8CB";       // U+E8CB Segoe MDL2 Sort (up and down arrows)
static constexpr LPCWSTR s_kpszMdl2List      = L"\xE8FD";       // U+E8FD Segoe MDL2 BulletedList (Explorer's View)
static constexpr LPCWSTR s_kpszMdl2Palette   = L"\xE790";       // U+E790 Segoe MDL2 Color (palette)
static constexpr LPCWSTR s_kpszMdl2Accept    = L"\xE73E";       // U+E73E Segoe MDL2 Accept (check mark)
static constexpr LPCWSTR s_kpszMdl2Info      = L"\xE946";       // U+E946 Segoe MDL2 Info ("i" in a ring)
static constexpr LPCWSTR s_kpszMdl2Warning   = L"\xE7BA";       // U+E7BA Segoe MDL2 Warning (outlined triangle)
static constexpr LPCWSTR s_kpszMdl2Download  = L"\xE896";       // U+E896 Segoe MDL2 Download (arrow down onto a line)
static constexpr LPCWSTR s_kpszMdl2Back      = L"\xE72B";       // U+E72B Segoe MDL2 Back
static constexpr LPCWSTR s_kpszMdl2Forward   = L"\xE72A";       // U+E72A Segoe MDL2 Forward
static constexpr LPCWSTR s_kpszMdl2Up        = L"\xE74A";       // U+E74A Segoe MDL2 Up
static constexpr LPCWSTR s_kpszMdl2Refresh   = L"\xE72C";       // U+E72C Segoe MDL2 Refresh
static constexpr LPCWSTR s_kpszMdl2Restore   = L"\xE777";       // U+E777 Segoe MDL2 UpdateRestore (arrow turning back)
static constexpr LPCWSTR s_kpszMdl2OpenFile        = L"\xE8E5";       // U+E8E5 OpenFile, Explorer's Open
static constexpr LPCWSTR s_kpszMdl2OpenWith        = L"\xE7AC";       // U+E7AC OpenWith
static constexpr LPCWSTR s_kpszMdl2CopyPath        = L"\xE62F";       // U+E62F, Explorer's Copy as path
static constexpr LPCWSTR s_kpszMdl2Repair          = L"\xE90F";       // U+E90F Repair (wrench), Explorer's Properties
static constexpr LPCWSTR s_kpszMdl2OpenInNewWindow = L"\xE8A7";       // U+E8A7 OpenInNewWindow
static constexpr LPCWSTR s_kpszMdl2Save            = L"\xE74E";       // U+E74E Save (a floppy disk)
static constexpr LPCWSTR s_kpszMdl2HardDrive       = L"\xEDA2";       // U+EDA2 HardDrive
static constexpr LPCWSTR s_kpszMdl2Setting         = L"\xE713";       // U+E713 Setting (gear)
static constexpr LPCWSTR s_kpszMdl2Preview   = L"\xE8A1";       // U+E8A1 Segoe MDL2 PreviewLink
static constexpr LPCWSTR s_kpszMdl2ChevronRight = L"\xE76C";  // U+E76C Segoe MDL2 ChevronRight
static constexpr LPCWSTR s_kpszMdl2Search = L"\xE721";       // U+E721 Segoe MDL2 Search (magnifying glass)

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
