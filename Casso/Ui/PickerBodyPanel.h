#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"


class DxuiSearchBox;
class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  PickerBodyPanel
//
//  Dxui content panel for the boot-disk picker: a search box docked at the
//  top, a list filling the rest. Lays out in physical pixels (the hosted
//  dialog passes a px content rect) so the fixed search-strip height scales
//  with DPI. Does not own either child -- Adopt only wires them into the
//  panel tree.
//
////////////////////////////////////////////////////////////////////////////////
class PickerBodyPanel : public DxuiPanel
{
public:
    void  Init (DxuiSearchBox * search, DxuiListView * list, int searchHeightDip, int gapDip);

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

    //
    //  DxuiListView::OnMouse expects widget-LOCAL (0-based) coordinates,
    //  but the panel fan-out delivers absolute client-px, so a plain
    //  DxuiPanel::OnMouse would hand the list mis-offset points (wrong
    //  row selected, column-resize divider never hit). Translate to
    //  list-local and dispatch to the list first (it owns scroll / drag
    //  / resize / select + consumes any press inside itself); anything
    //  the list declines falls through to the search box, which hit-
    //  tests against its own absolute bounds.
    //
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;


private:
    DxuiSearchBox *  m_search          = nullptr;
    DxuiListView  *  m_list            = nullptr;
    int              m_searchHeightDip = 0;
    int              m_gapDip          = 0;
};
