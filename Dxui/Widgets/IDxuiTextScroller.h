#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextScroller
//
//  A text view's scrolling, used by its find and its input: the top line
//  and the lines that fit, the first column in view without wrapping, and
//  scrolling a position into view.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextScroller
{
public:
    using Position = DxuiTextPosition;

    virtual       ~IDxuiTextScroller() = default;

    virtual int   GetTopLine       () const       = 0;
    virtual void  SetTopLine       (int line)     = 0;
    virtual void  ScrollLines      (int delta)    = 0;
    virtual int   GetLineCap       () const       = 0;
    virtual int   GetLeftColumn    () const       = 0;
    virtual void  SetLeftColumn    (int column)   = 0;
    virtual void  ScrollColumns    (int delta)    = 0;
    virtual void  ScrollToPosition (Position pos) = 0;
};
