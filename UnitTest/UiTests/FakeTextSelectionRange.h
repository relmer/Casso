#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeTextSelectionRange
//
//  A text view's selection as a test sets it, for the text view's painter.
//
////////////////////////////////////////////////////////////////////////////////

class FakeTextSelectionRange : public IDxuiTextSelectionRange
{
public:
    bool                      HasSelection () const override { return !(from == to); }
    DxuiTextLayout::Position  GetFrom      () const override { return from; }
    DxuiTextLayout::Position  GetTo        () const override { return to; }

    DxuiTextLayout::Position  from;
    DxuiTextLayout::Position  to;
};
