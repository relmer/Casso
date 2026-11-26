#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerPath
//
//  The controls under the pointer, outermost first. A control the pointer
//  moves off gets a move to kNowherePx and a Leave event.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPointerPath
{
public:
    //  Each control the pointer moves off, and each on the path when the
    //  pointer moves out of the window, is sent a move to kNowherePx and a
    //  DxuiMouseEventKind::Leave event, so a hover can't outlast the pointer,
    //  however the window routes its input.
    static constexpr int  kNowherePx = -32768;

    bool  Update (const IDxuiControl & root, POINT clientPx, bool leftWindow);

private:
    static void  CollectPathAt   (const IDxuiControl & control, POINT clientPx, std::vector<IDxuiControl *> & outPath);
    static bool  IsInTree        (const IDxuiControl & root, const IDxuiControl * control);
    static void  SendPointerAway (IDxuiControl & control);

    std::vector<IDxuiControl *>  m_path;
};