#pragma once

#include "Pch.h"

#include "Widgets/HexView/DxuiHexViewTypes.h"

class DxuiHexView;
struct DxuiKeyEvent;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection
//
//  The selection: the caret and the anchor the run extends from, the
//  column the keyboard is in, the press and drag that select with the
//  pointer, the keys that move the caret, and the selection as text.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexSelection
{
public:
    explicit DxuiHexSelection (DxuiHexView & view);

    void      SelectByte        (uint64_t offset, DxuiHexViewTypes::Column column);
    void      ExtendSelectionTo (uint64_t offset);
    void      SelectAll         ();
    void      ClearSelection    ();
    bool      IsByteSelected    (uint64_t offset) const;
    uint64_t  GetSelectionFirst () const;
    uint64_t  GetSelectionLast  () const;
    uint64_t  GetSelectionCount () const;
    void      SetActiveColumn   (DxuiHexViewTypes::Column column);

    bool                      HasSelection    () const { return m_hasSelection; }
    uint64_t                  GetCaret        () const { return m_caret; }
    uint64_t                  GetAnchor       () const { return m_anchor; }
    DxuiHexViewTypes::Column  GetActiveColumn () const { return m_activeColumn; }
    bool                      IsDragging      () const { return m_dragging; }

    void          CopySelection    () const;
    std::wstring  GetSelectionText () const;

    //  Moves the caret to `offset`, extending the run when `extend` is set and
    //  starting a new one otherwise, then brings it into view.
    void  MoveCaretTo            (uint64_t offset, bool extend);
    void  NotifySelectionChanged ();

    //  A left press on a byte, the drag that follows and its release; a right
    //  press, which selects the byte unless it is already selected; and the
    //  keys that move the caret or the column.
    void  OnPress        (const DxuiMouseEvent & ev, const DxuiHexViewTypes::HitResult & hit);
    void  OnContextPress (const DxuiHexViewTypes::HitResult & hit);
    bool  OnDrag         (const DxuiMouseEvent & ev);
    bool  OnRelease      ();
    bool  OnKey          (const DxuiKeyEvent & ev);

private:
    friend class DxuiHexView;

    DxuiHexView  & m_view;

    bool                      m_hasSelection = false;
    uint64_t                  m_caret        = 0;
    uint64_t                  m_anchor       = 0;
    DxuiHexViewTypes::Column  m_activeColumn = DxuiHexViewTypes::Column::Hex;
    bool                      m_dragging     = false;
    uint64_t                  m_pressFirst   = 0;   // the value a press in the value column took
    uint64_t                  m_pressLast    = 0;
};
