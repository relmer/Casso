#include "Pch.h"

#include "Ui/Debugger/ControlTreeWalk.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControlTreeWalk::FindFirstFocusable
//
//  The first focusable control in a pane's tree, the pane's content first.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * ControlTreeWalk::FindFirstFocusable (IDxuiControl * node)
{
    IDxuiControl  * found = nullptr;



    if (node == nullptr)
    {
        return nullptr;
    }

    if (node->IsFocusable() && node->IsEnabled())
    {
        return node;
    }

    for (size_t i = 0; i < node->GetChildCount() && found == nullptr; i++)
    {
        found = FindFirstFocusable (node->GetChild (i));
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControlTreeWalk::GetCursorOutsideLists
//
//  The cursor of the controls under `node` other than the lists. A list takes
//  a point relative to itself, which the walk of the controls does not
//  translate, so with the window's point every list with text selection had
//  the I-beam over the whole window. GetCursorForPoint calls the lists first.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ControlTreeWalk::GetCursorOutsideLists (
    const IDxuiControl                 & node,
    POINT                                clientPx,
    const std::vector<DxuiListView *>  & lists)
{
    LPCWSTR  cursor = nullptr;



    for (size_t i = node.GetChildCount(); i > 0 && cursor == nullptr; i--)
    {
        const IDxuiControl  * child  = node.GetChild (i - 1);
        bool                  isList = std::ranges::find (lists, child) != lists.end();

        if (child == nullptr || isList || !child->IsVisible())
        {
            continue;
        }

        cursor = HasListWithin (*child, lists) ? GetCursorOutsideLists (*child, clientPx, lists) : child->GetCursorForPoint (clientPx);
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControlTreeWalk::HasListWithin
//
////////////////////////////////////////////////////////////////////////////////

bool ControlTreeWalk::HasListWithin (const IDxuiControl & node, const std::vector<DxuiListView *> & lists)
{
    for (size_t i = 0; i < node.GetChildCount(); i++)
    {
        const IDxuiControl  * child = node.GetChild (i);

        if (child != nullptr && (std::ranges::find (lists, child) != lists.end() || HasListWithin (*child, lists)))
        {
            return true;
        }
    }

    return false;
}
