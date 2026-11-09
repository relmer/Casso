#pragma once

#include "Pch.h"
#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTextSelect
//
//  Text selection in Details, for a list that reads as text rather than as
//  rows, as a disassembly does: a drag selects characters across cells and
//  rows, a Ctrl+drag only in the column it began in, and a double-click
//  selects the word under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListTextSelect
{
public:
    explicit DxuiListTextSelect (DxuiListView & list);

    void          SetEnabled   (bool enabled);
    bool          IsEnabled    () const { return m_enabled; }
    bool          HasSelection () const { return m_hasSel; }
    bool          IsPressed    () const { return m_pressed; }
    void          Clear        ();

    //  The first and last rows the selection touches; false without one.
    bool          GetRows      (int & first, int & last) const;

    //  The row, the cell and the caret position in the cell's text under a
    //  point in the list's pixels; false off the rows or the cells' text.
    bool          TryHitTestCellChar (POINT localPx, int & row, size_t & column, int & ch) const;

    //  The caret position in a cell's text nearest an x in the list's pixels.
    int           HitTestCharInCell  (int row, size_t col, int lx) const;

    //  Whether a point is over a text column's text, where the pointer is an
    //  I-beam.
    bool          IsOverCellText     (POINT localPx) const;

    //  The selected characters of a cell of `length` characters; false when
    //  none are.
    bool          GetCellSelection   (int row, size_t col, int length, int & start, int & end) const;
    bool          IsCellSelected     (int row, size_t col) const;

    //  The selected characters, cells separated by tabs and rows by line ends.
    std::wstring  GetText            () const;

    //  A press on a row, the drag that follows and the release. OnRelease
    //  returns whether the press became a drag that selected text, which is
    //  then not also a click.
    void          OnPress            (int lx, int ly, bool ctrl);
    bool          OnDrag             (int lx, int ly);
    bool          OnRelease          ();

    //  A double-click selects the word under the pointer, as in an editor.
    void          SelectWordAt       (int lx, int ly);

private:
    static constexpr int  s_kDragSlopDip = 3;

    DxuiListViewTypes::TextPos  HitTestText (int lx, int ly) const;
    static bool                 IsWordChar  (wchar_t ch);

    DxuiListView  & m_list;

    bool                        m_enabled     = false;
    bool                        m_pressed     = false;
    bool                        m_hasSel      = false;
    bool                        m_isColumnSel = false;
    DxuiListViewTypes::TextPos  m_anchor;
    DxuiListViewTypes::TextPos  m_focus;
    POINT                       m_pressPx     = {};
};
