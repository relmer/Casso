#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IInfoTips
//
//  The shell's tip for a file or folder, read on a thread: TryGet returns
//  false until the tip is read, and Start's message is posted to the window
//  when it is.
//
////////////////////////////////////////////////////////////////////////////////

class IInfoTips
{
public:
    virtual ~IInfoTips() = default;

    virtual void  Start  (HWND hwnd, UINT message)                         = 0;
    virtual bool  TryGet (const std::wstring & path, std::wstring & outTip) = 0;
    virtual void  Clear  ()                                                = 0;
};