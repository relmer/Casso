#pragma once

#include "Pch.h"


class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerTracker
//
//  The controls under the pointer in a window's tree, outermost first. Each
//  control the pointer moves off, and each on the path when the pointer
//  moves out of the window, is sent a move to kNowherePx and a
//  DxuiMouseEventKind::Leave event, so a hover can't outlast the pointer,
//  however the window routes its input.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPointerTracker
{
public:
    static constexpr int  kNowherePx = -32768;

    explicit DxuiPointerTracker (const IDxuiControl & root) : m_root (root) {}

    //  The path found again at the pointer, or emptied when it left the
    //  window; true when a control was sent the pointer away.
    bool  Update (POINT clientPx, bool leftWindow);

    //  The scrollbar menu of the control under the point, the control itself
    //  first and then down through its visible children; false when no bar
    //  is under the point.
    static bool  ShowScrollbarMenuIn (IDxuiControl & control, HWND hwnd, POINT clientPx);

private:
    static void  CollectPathAt   (const IDxuiControl & control, POINT clientPx, std::vector<IDxuiControl *> & outPath);
    static bool  IsInTree        (const IDxuiControl & root, const IDxuiControl * control);
    static void  SendPointerAway (IDxuiControl & control);

    const IDxuiControl           & m_root;
    std::vector<IDxuiControl *>    m_path;
};
