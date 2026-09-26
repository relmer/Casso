#pragma once

#include "Pch.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRowLook
//
//  How a row in a list or tree is filled and outlined, from its state, as
//  File Explorer draws its own. Measured from Explorer in the dark theme:
//
//    Selected, the row the keyboard is on, its pane focused   #505050, outlined #C3C3C3
//    Selected, another row, its pane focused (multi-select)   #626262, outlined in the accent
//    Selected, its pane not focused                           #333333, no outline
//    Under the pointer                                        #4D4D4D
//
//  The light theme outlines a selected row in a pane without focus as well;
//  the dark theme's outline for it is none.
//
//  A selected row under the pointer while its pane is not focused takes the
//  hover fill and the #C3C3C3 outline; the row the keyboard is on, with its
//  pane focused, is outlined whether it is selected or not.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiRowLook
{
    uint32_t  fill = 0;
    uint32_t  edge = 0;

    static DxuiRowLook  Resolve (const IDxuiTheme & theme, bool selected, bool keyboardRow, bool hovered, bool paneFocused)
    {
        DxuiRowLook  look;



        if (selected && paneFocused)
        {
            look.fill = keyboardRow ? theme.ContentSelection()     : theme.ContentSelectionMulti();
            look.edge = keyboardRow ? theme.ContentSelectionEdge() : theme.ContentSelectionMultiEdge();
        }
        else if (selected)
        {
            look.fill = hovered ? theme.ContentHover()         : theme.ContentSelectionInactive();
            look.edge = hovered ? theme.ContentSelectionEdge() : theme.ContentSelectionInactiveEdge();
        }
        else
        {
            look.fill = hovered ? theme.ContentHover() : 0u;
            look.edge = (keyboardRow && paneFocused) ? theme.ContentSelectionEdge() : 0u;
        }

        return look;
    }
};
