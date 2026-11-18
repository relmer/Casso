#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"


class DxuiScrollbar;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextInputHost
//
//  The text view a DxuiTextViewInput takes the mouse and keys for: what is
//  under a point, its bounds and text area, its clipboard, its focused row,
//  and its context menu and links.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextInputHost
{
public:
    using Position = DxuiTextPosition;

    virtual                    ~IDxuiTextInputHost() = default;

    virtual Position           HitTestInput       (POINT point) const                              = 0;
    virtual bool               TryGetInputLink    (POINT point, int & outRow, int & outSpan) const = 0;
    virtual DxuiScrollbar    * GetInputBarAt      (POINT point)                                    = 0;
    virtual RECT               GetInputBounds     () const                                         = 0;
    virtual int                GetInputTextLeft   () const                                         = 0;
    virtual void               CopyInputSelection () const                                         = 0;
    virtual void               FocusRowOnPress    (int row)                                        = 0;
    virtual bool               TryMoveInputRow    (WPARAM vk, bool ctrl)                           = 0;
    virtual void               ShowInputMenu      (POINT point)                                    = 0;
    virtual void               ClickInputLink     (int row, int span)                              = 0;
};
