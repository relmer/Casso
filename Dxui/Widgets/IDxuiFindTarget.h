#pragma once

#include "Pch.h"

class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFindResult
//
//  How a search ended: no match, a match, or a match found only by going
//  round past the end of the text (the start, searching backward).
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiFindResult
{
    NotFound,
    Found,
    Wrapped,
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiFindTarget
//
//  What a find box searches: a text view's text, or a list's cells. The box
//  calls these as Visual Studio Code's find widget does its editor.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiFindTarget
{
public:
    virtual ~IDxuiFindTarget() = default;

    //  The control the text is drawn in, for the focus; where it is drawn;
    //  and whether it is on screen.
    virtual IDxuiControl  * GetFindControl   () = 0;
    virtual RECT            GetFindBounds    () const = 0;
    virtual bool            IsFindVisible    () const = 0;

    //  The text selected, to start a search with; empty for none.
    virtual std::wstring    GetSelectionText () const = 0;

    //  Selects the next match, or the one before it, and scrolls it into view;
    //  outIndex counts from 1 and outCount is every match, both 0 with none.
    virtual DxuiFindResult  SelectMatch      (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount) = 0;

    //  The search as it is typed: every match highlighted, and the first one
    //  in view selected, or the first after the top of the view.
    virtual DxuiFindResult  SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount) = 0;

    virtual void  SetFindScopeToSelection () = 0;
    virtual void  ClearFindScope          () = 0;
    virtual void  ClearFindHighlights     () = 0;

    //  The fills under the matches and under the selected one.
    virtual void            SetFindColors    (uint32_t matchArgb, uint32_t currentArgb) = 0;

    //  How far down from the top of the find bounds the find box covers, so
    //  a match brought into view is placed below it; 0 with the box closed.
    virtual void            SetFindCoveredPx (int coveredPx) { (void) coveredPx; }
};