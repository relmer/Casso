#pragma once

#include "Pch.h"





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

// U+2022 BULLET (•)
static constexpr wchar_t s_kchBullet  = L'\x2022';

// U+2014 EM DASH (—)
static constexpr wchar_t s_kchEmDash  = L'\x2014';

// U+2026 HORIZONTAL ELLIPSIS (…)
static constexpr wchar_t s_kchEllipsis = L'\x2026';

// U+2248 ALMOST EQUAL TO (≈)
static constexpr wchar_t s_kchAlmostEqual = L'\x2248';
