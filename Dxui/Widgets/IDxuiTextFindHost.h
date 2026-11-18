#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextFindHost
//
//  The text view a DxuiTextViewFind searches: each row's text, the selection
//  and the lines in view.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextFindHost
{
public:
    using Position = DxuiTextPosition;

    virtual                            ~IDxuiTextFindHost() = default;

    virtual std::vector<std::wstring>  GetFindRowTexts       () const                                   = 0;
    virtual void                       GetFindSelection      (Position & anchor, Position & caret) const = 0;
    virtual void                       SelectFound           (Position anchor, Position caret)          = 0;
    virtual int                        GetFindLineOfPosition (Position pos) const                       = 0;
    virtual int                        GetFindTopLine        () const                                   = 0;
    virtual int                        GetFindLineCap        () const                                   = 0;
};
