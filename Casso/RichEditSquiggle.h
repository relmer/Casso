#pragma once

#include "Pch.h"

#include "TrackSectorPredicate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RichEditSquiggle
//
//  Spec-006 FR-014e helpers. ApplyRejectedTokenSquiggles paints a red
//  wavy underline over each malformed token in a RichEdit 4.1 input;
//  SetIgnoredTokensLabel updates the read-only static beneath the
//  input with "Ignored: tok1, tok2" (empty string when no rejects).
//
//  BuildIgnoredTokensLabel is the pure helper backing the label and
//  is the unit under test in UnitTest/Casso/RichEditSquiggleTests.cpp.
//  It slices substrings out of the original expression using the
//  recorded UTF-16 offsets -- no re-tokenization, no whitespace
//  inclusion.
//
////////////////////////////////////////////////////////////////////////////////

void          ApplyRejectedTokenSquiggles (
    HWND                                                    hRichEdit,
    const std::vector<TrackSectorPredicate::RejectedSpan> & spans);

void          SetIgnoredTokensLabel (
    HWND                                                    hStatic,
    std::wstring_view                                       originalExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & spans);

std::wstring  BuildIgnoredTokensLabel (
    std::wstring_view                                       originalExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & spans);

// Spec-006 bug-fix. Builds a single combined "Invalid track ..." /
// "Invalid sector ..." string for the merged label below the filter
// controls. Returns "" when neither input has rejects.
std::wstring  BuildCombinedInvalidLabel (
    std::wstring_view                                       trackExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & trackSpans,
    std::wstring_view                                       sectorExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & sectorSpans);

void          SetCombinedInvalidLabel (
    HWND                                                    hStatic,
    std::wstring_view                                       trackExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & trackSpans,
    std::wstring_view                                       sectorExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & sectorSpans);
