#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListKeyboard
//
//  Keyboard movement through the details rows: by row, group and page,
//  Home and End, the lines wholly in view, and type-ahead.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListKeyboard
{
public:
    explicit DxuiListKeyboard (DxuiListView & list);

    //  A pause this long between characters starts a new search.
    static constexpr int64_t  s_kTypeAheadResetMs = 1000;
    bool  HandleKeyboardGroupedNav  (WPARAM vk, bool shift, bool ctrl = false);
    bool  HandleKeyboardDetailsJump (WPARAM vk, bool shift, bool ctrl);

    //  Page Up and Page Down, as Explorer's.
    bool         HandleKeyboardPage     (WPARAM vk, bool shift, bool ctrl);
    void         ApplyKeyboardMove      (int row, bool shift, bool ctrl);
    int          GetPageLines           () const;
    bool         TryGetWholeLineSpan    (int & first, int & last) const;
    bool         TryGetWholeRowLineSpan (int & first, int & last) const;
    bool         IsLineWhollyShown      (int line) const;
    int          StepRowLines           (int line, int steps) const;
    int          GetEdgeRowLine         (bool last) const;
    void         ScrollLineToBottom     (int line);
    void         ScrollLineToTop        (int line);

    //  Space on the focused item, as in Explorer: Ctrl toggles it, alone it
    //  becomes the selection; on a focused header, the group's rows do.
    bool  SelectFocused (bool ctrl);

    //  Selects the next row whose first column starts with what has been
    //  typed, as Explorer's list does. True when the character was taken.
    bool  HandleTypeAhead          (wchar_t ch);
    bool  HandleKeyboardBodyRowNav (WPARAM vk, bool shift = false, bool ctrl = false);
    bool  HandleKeyboardRowNav     (WPARAM vk, bool shift, bool ctrl);

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    int                        m_keyMoveRow        = -1;      // a key's move, reported once the key is handled
    bool                       m_keyMovePending    = false;

    //  Characters typed toward a row, and when the last one arrived.
    std::wstring                m_typeAhead;
    int64_t                     m_typeAheadMs = 0;
    DxuiListViewTypes::ClockFn  m_clock;
};
