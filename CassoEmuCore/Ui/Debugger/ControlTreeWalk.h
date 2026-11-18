#pragma once

class DxuiListView;
class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  ControlTreeWalk
//
//  Searches of a tree of Dxui controls: the first control that takes the
//  focus, and the cursor of the controls other than the lists.
//
////////////////////////////////////////////////////////////////////////////////

class ControlTreeWalk
{
public:
    //  The first focusable control in a pane's tree, the pane's content first.
    static IDxuiControl  * FindFirstFocusable    (IDxuiControl * node);
    static LPCWSTR         GetCursorOutsideLists (const IDxuiControl & node, POINT clientPx, const std::vector<DxuiListView *> & lists);
    static bool            HasListWithin         (const IDxuiControl & node, const std::vector<DxuiListView *> & lists);
};
