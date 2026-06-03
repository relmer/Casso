#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLabelTruncation
//
//  Pure basename-truncation helper. Used by the drive widget to fit a
//  mounted disk's filename below "Drive N" with a single-character
//  ellipsis (U+2026) when the literal basename is too wide. The
//  measure callback is `DxUiPainter::MeasureTextRunWidth` in
//  production and a deterministic stub in tests.
//
//  Spec rules:
//    - Literal `path.filename()` — no extension stripping, even for
//      basenames with multiple dots or no extension.
//    - Single-character ellipsis (s_kchEllipsis), not three dots.
//    - Degenerate case (basename narrower than the ellipsis itself):
//      return just the ellipsis.
//
////////////////////////////////////////////////////////////////////////////////



std::wstring TruncateToWidth (
    std::wstring_view                                  basename,
    float                                              maxWidthPx,
    const std::function<float (std::wstring_view)>   & measure);
