#pragma once

#include "Pch.h"

class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDialogHost
//
//  What a DxuiDialogController calls on the window it works for: ending the
//  dialog, repainting, and the two hooks a tabbed or scrolling window
//  overrides.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDialogHost
{
public:
    virtual ~IDxuiDialogHost() = default;

    virtual void  EndDialog          (int result)                 = 0;
    virtual void  Invalidate         ()                           = 0;
    virtual bool  OnDialogTabSwitch  (bool backward)              = 0;
    virtual void  OnDialogKeyHandled (IDxuiControl * focused)     = 0;
};