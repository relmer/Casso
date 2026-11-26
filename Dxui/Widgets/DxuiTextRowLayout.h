#pragma once

#include "Pch.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "DxuiTextRow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRowLayout
//
//  The parts of DxuiTextView's layout that depend on a row alone: the indent
//  of a row's wrapped lines, and the font weight of a run's styles.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextRowLayout
{
public:
    //  A row's wrap indent in a last cell `avail` characters wide, or zero
    //  when less than half of that width would remain for the wrapped text.
    static int             GetWrapIndent (const DxuiTextRow & row, int avail);

    //  The weight of a run bold, italic, both or neither.
    static DxuiFontWeight  GetWeight     (bool bold, bool italic);
};
