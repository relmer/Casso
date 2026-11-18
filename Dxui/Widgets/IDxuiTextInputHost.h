#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"


class DxuiScrollbar;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextInputHost
//
//  The text view a DxuiTextViewInput takes the mouse and keys for: what is
//  under a point, its scroll bars and scrolling, its selection and focused
//  row, and its context menu and links.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextInputHost
{
public:
    using Position = DxuiTextPosition;

    virtual                    ~IDxuiTextInputHost() = default;

    virtual Position           HitTestInput        (POINT point) const                                      = 0;
    virtual bool               TryGetInputLink     (POINT point, int & outRow, int & outSpan) const         = 0;
    virtual void               GetInputWordBounds  (Position pos, Position & outFirst, Position & outLast) const = 0;
    virtual RECT               GetInputBounds      () const                                                 = 0;
    virtual int                GetInputTextLeft    () const                                                 = 0;
    virtual bool               IsInputNoWrap       () const                                                 = 0;
    virtual bool               HasInputRows        () const                                                 = 0;
    virtual bool               HasInputSelection   () const                                                 = 0;
    virtual DxuiScrollbar    * GetInputVertBar     (POINT point)                                            = 0;
    virtual DxuiScrollbar    * GetInputHorzBar     (POINT point)                                            = 0;
    virtual DxuiScrollbar    & GetVertBar          ()                                                       = 0;
    virtual DxuiScrollbar    & GetHorzBar          ()                                                       = 0;
    virtual void               ScrollInputLines    (int delta)                                              = 0;
    virtual void               ScrollInputColumns  (int delta)                                              = 0;
    virtual void               SetInputTopLine     (int line)                                               = 0;
    virtual void               SetInputLeftColumn  (int column)                                             = 0;
    virtual int                GetInputLeftColumn  () const                                                 = 0;
    virtual int                GetInputLineCap     () const                                                 = 0;
    virtual int                GetInputLineCount   () const                                                 = 0;
    virtual void               SetInputSelection   (Position anchor, Position caret)                        = 0;
    virtual void               SetInputCaret       (Position caret)                                         = 0;
    virtual void               SelectAllInput      ()                                                       = 0;
    virtual void               CopyInputSelection  () const                                                 = 0;
    virtual void               FocusRowOnPress     (int row)                                                = 0;
    virtual bool               TryMoveInputRow     (WPARAM vk, bool ctrl)                                   = 0;
    virtual void               ShowInputMenu       (POINT point)                                            = 0;
    virtual void               ClickInputLink      (int row, int span)                                      = 0;
};
