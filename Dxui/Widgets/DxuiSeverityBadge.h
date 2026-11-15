#pragma once

#include "Pch.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSeverity
//
//  How serious a notice or a finding is, least first, so the most severe of
//  several is their maximum.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiSeverity
{
    Info,
    Warning,
    Error,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSeverityBadge
//
//  The mark for a severity, drawn from primitives: an error as a disc in the
//  theme's error color with an exclamation cut out of it, a warning as
//  DxuiWarningBadge's triangle, and information as a ring with an "i" in the
//  theme's accent. A count badge is a disc, or a pill for more digits, in
//  the severity's color with the count in it.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSeverityBadge
{
public:
    //  Draws the mark into the square box at (left, top).
    static void  Draw (IDxuiPainter & painter, DxuiSeverity severity, float left, float top, float size, const IDxuiTheme & theme);

    //  The count badge's width for its text, and the badge drawn with its
    //  left edge at left, centered on cy.
    static float  GetCountWidth (IDxuiTextRenderer & text, const std::wstring & count, float height);
    static void   DrawCount     (IDxuiPainter & painter, IDxuiTextRenderer & text, DxuiSeverity severity, const std::wstring & count,
                                 float left, float cy, float height, const IDxuiTheme & theme);

    //  The severity's color: the error color, the warning accent, or the accent.
    static uint32_t  GetColor (DxuiSeverity severity, const IDxuiTheme & theme);

private:
    static constexpr float  s_kStrokeEm = 0.085f;   // the ring's and the "i"'s pen, as a fraction of the box
    static constexpr float  s_kCountEm  = 0.72f;    // the count's face, as a fraction of the badge's height
};
