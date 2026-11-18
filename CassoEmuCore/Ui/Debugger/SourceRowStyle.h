#pragma once

#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceRowStyle
//
//  How a source view's PC line and breakpoints are drawn: the disassembly
//  pane's marker color, row fill, breakpoint icons and PC arrow, so the two
//  panes mark a line alike, in the view's glyph margin, with the gray
//  breakpoint on a line with code under the pointer there. No breakpoint
//  icons draws a bullet in the marker column, and no PC arrow a triangle
//  there; with no syntax colors the text is in the view's color.
//
////////////////////////////////////////////////////////////////////////////////

struct SourceRowStyle
{
    uint32_t                               pcMarkerArgb = 0;
    uint32_t                               pcRowArgb    = 0;
    std::shared_ptr<const DxuiIconImage>   enabledIcon;
    std::shared_ptr<const DxuiIconImage>   disabledIcon;
    SourceSyntax::Colors                   syntax;
    uint32_t                               resultArgb   = 0;
    uint32_t                               operandArgb  = 0;
    std::shared_ptr<const DxuiIconImage>   pcIcon;
    std::shared_ptr<const DxuiIconImage>   hoverIcon;

    bool operator== (const SourceRowStyle & other) const = default;
};
