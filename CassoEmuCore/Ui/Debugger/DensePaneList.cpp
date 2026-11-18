#include "Pch.h"

#include "Ui/Debugger/DensePaneList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DensePaneList::MakeDense
//
//  One setting for every pane, so the panes read as one listing rather than
//  seven lists that happen to share a window.
//
//  A pane narrower than its columns scrolls sideways, as Visual Studio's do:
//  the columns fit their contents, so without the scroll whatever was right
//  of the pane's edge -- the code pane's annotations, the call stack's Found
//  by -- could not be seen at all.
//
////////////////////////////////////////////////////////////////////////////////

void DensePaneList::MakeDense (DxuiListView * list)
{
    list->SetShowHeader              (true);
    list->SetMonospace               (true);
    list->SetFontSizeDip             (kFontDip);
    list->SetRowHeightDip            (kRowDip);
    list->SetHeaderHeightDip         (kHeaderDip);
    list->SetCellPaddingDip          (kPadDip, kPadDip);
    list->SetPreciseAutoFit          (true);
    list->SetRefitOnSetRows          (true);
    list->SetHorizontalScrollEnabled (true);
    list->SetPaneTextInset           (true);

    //  The arrows, the page keys, Home and End move the selection, and Ctrl+C
    //  copies it, so F9, run to cursor and the edit keys can act on a row
    //  chosen without the mouse. A keyboard move is reported as a click is,
    //  so the pane's record of the selection, which each snapshot restores,
    //  follows it.
    list->SetKeyboardColumnNav       (true);
    list->SetKeyboardMoveNotify      (true);
}
