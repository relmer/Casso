#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeTextViewport
//
//  A text view's scroll position for the text view's components: the top
//  line and left column as a test sets them, no scrollbars, and a count of
//  the lines and columns scrolled.
//
////////////////////////////////////////////////////////////////////////////////

class FakeTextViewport : public IDxuiTextViewport
{
public:
    int   GetTopLine             () const override     { return topLine; }
    int   GetLeftColumn          () const override     { return leftColumn; }
    int   GetLineCap             () const override     { return lineCap; }
    bool  IsScrollbarVisible     () const override     { return false; }
    bool  IsHorzScrollbarVisible () const override     { return false; }
    void  ScrollLines            (int delta) override  { linesScrolled   += delta; }
    void  ScrollColumns          (int delta) override  { columnsScrolled += delta; }

    int  topLine         = 0;
    int  leftColumn      = 0;
    int  lineCap         = 10;
    int  linesScrolled   = 0;
    int  columnsScrolled = 0;
};
