#pragma once

#include "Pch.h"
#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;
class IDxuiPainter;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListBand
//
//  The rubber band a drag from empty space draws, in every view: where the
//  press was, in content pixels so it stays put as the view scrolls, where
//  the pointer is now, the selection a Ctrl press began with, which the
//  band adds to, and its paint.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListBand
{
public:
    explicit DxuiListBand (DxuiListView & list);

    static constexpr uint32_t  s_kFillAlpha = 0x50000000u;   // the band's fill, about a third, as measured off Explorer

    //  A press on empty space begins a band; the drag and release that follow
    //  return whether a band was being drawn.
    void  Begin     (int lx, int ly, bool ctrl);
    bool  OnDrag    (int lx, int ly);
    bool  OnRelease ();
    bool  IsActive  () const { return m_active; }

    //  The band in the list's pixels; empty when none is being drawn.
    RECT  GetRectPx () const;
    void  Paint     (IDxuiPainter & painter, const DxuiListViewTypes::Palette & pal, float x, float y) const;

private:
    void  Update (int lx, int ly);

    DxuiListView  & m_list;

    bool              m_active = false;
    POINT             m_start  = {};
    POINT             m_end    = {};
    std::vector<int>  m_base;
};
