#pragma once

#include "Pch.h"

#include "Widgets/DxuiDockSite.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragOverlay
//
//  A click-through layered window that shows a dock site's drag marks above
//  every other window of the application, floating windows included, which
//  would otherwise cover the drop targets the site paints in its own window.
//
//  It is topmost and transparent to the pointer, so the drag underneath goes
//  on unchanged; Hide takes it down when the drag ends.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDragOverlay
{
public:
    DxuiDragOverlay  () = default;
    ~DxuiDragOverlay ();

    DxuiDragOverlay (const DxuiDragOverlay &)             = delete;
    DxuiDragOverlay & operator= (const DxuiDragOverlay &) = delete;

    //  Shows `marks`, given in pixels from the top left of `screenRect`, over
    //  that rectangle of the screen.
    HRESULT  Show      (HWND owner, const RECT & screenRect, const std::vector<DxuiDockDragMark> & marks);
    void     Hide      ();
    bool     IsShowing () const { return m_hwnd != nullptr && m_showing; }

    //  Draws `marks` into a top-down, premultiplied 32-bit image `width`
    //  pixels wide, in order, each blended over what is already there.
    static void  RenderMarks (const std::vector<DxuiDockDragMark> & marks, int width, int height, uint32_t * pixels);

private:
    static void  BlendRect (const RECT & rect, uint32_t argb, int width, int height, uint32_t * pixels);

    HRESULT  CreateHwnd (HWND owner);

    HWND  m_hwnd    = nullptr;
    bool  m_showing = false;
};
