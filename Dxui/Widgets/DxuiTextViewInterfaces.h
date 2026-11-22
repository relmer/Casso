#pragma once

#include "Pch.h"
#include "DxuiTextLayout.h"

class  DxuiDpiScaler;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextGeometry
//
//  A text view's bounds and the place of its text in them.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextGeometry
{
public:
    virtual ~IDxuiTextGeometry() = default;

    virtual const RECT           & GetViewBounds  () const               = 0;
    virtual POINT                  GetTextOrigin  () const               = 0;
    virtual int                    GetTextColumns (bool scrollbar) const = 0;
    virtual int                    GetPadPx       () const               = 0;
    virtual int                    GetBarWidthPx  () const               = 0;
    virtual const DxuiDpiScaler  & GetScaler      () const               = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextViewport
//
//  The lines and columns of a text view in sight, and its scrollbars'
//  visibility.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextViewport
{
public:
    virtual ~IDxuiTextViewport() = default;

    virtual int   GetTopLine             () const     = 0;
    virtual int   GetLeftColumn          () const     = 0;
    virtual int   GetLineCap             () const     = 0;
    virtual bool  IsScrollbarVisible     () const     = 0;
    virtual bool  IsHorzScrollbarVisible () const     = 0;
    virtual void  ScrollLines            (int delta)  = 0;
    virtual void  ScrollColumns          (int delta)  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextSelectionRange
//
//  The selected characters of a text view, from the first to past the last.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextSelectionRange
{
public:
    virtual ~IDxuiTextSelectionRange() = default;

    virtual bool                      HasSelection () const = 0;
    virtual DxuiTextLayout::Position  GetFrom      () const = 0;
    virtual DxuiTextLayout::Position  GetTo        () const = 0;
};