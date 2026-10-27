#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemKeys
//
//  Keyboard movement through the icon, tile, content and list views: by
//  item, line, column, group and page, and the lines and columns wholly
//  in view.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListItemKeys
{
public:
    explicit DxuiListItemKeys (DxuiListView & list);

    bool          HandleKeyboardItemNav   (WPARAM vk, bool shift, bool ctrl = false);
    bool          HandleKeyboardGroupedItemNav (WPARAM vk, bool shift, bool ctrl);
    static int    GetLastItemLine         (const DxuiListViewTypes::ItemLayout & layout);
    static int    StepItemLines           (const DxuiListViewTypes::ItemLayout & layout, int line, int steps);
    bool          TryGetWholeItemLineSpan (const DxuiListViewTypes::ItemLayout & layout, int & first, int & last) const;
    bool          IsItemLineWhollyShown   (const DxuiListViewTypes::ItemLayout & layout, int line) const;
    void          AnchorItemLine          (int line, bool bottom);
    void          ScrollItemLineToBottom  (int line);
    void          ScrollItemLineToTop     (int line);
    bool          HandleKeyboardItemPage  (const DxuiListViewTypes::ItemLayout & layout, WPARAM vk, int line, int slot, bool shift, bool ctrl);
    bool          HandleKeyboardItemJump  (const DxuiListViewTypes::ItemLayout & layout, WPARAM vk, bool shift, bool ctrl);

    bool          HandleKeyboardListGroupNav (WPARAM vk, bool shift, bool ctrl);
    bool                     TryGetWholeListColumnSpan (const std::vector<DxuiListViewTypes::ListColumn> & cols, int & first, int & last) const;
    bool                     IsListColumnWhollyShown   (const DxuiListViewTypes::ListColumn & col) const;
    int                      GetListPageStartColumn    (const std::vector<DxuiListViewTypes::ListColumn> & cols, bool down) const;
    bool                     HandleKeyboardListJump    (WPARAM vk, bool shift, bool ctrl);
    bool                     HandleKeyboardListPage    (const std::vector<DxuiListViewTypes::ListColumn> & cols, bool down, bool shift, bool ctrl);

private:
    friend class DxuiListView;

    DxuiListView  & m_list;
};
