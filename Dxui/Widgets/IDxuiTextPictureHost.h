#pragma once

#include "Pch.h"

class   DxuiDpiScaler;
struct  DxuiTextPicture;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextPictureHost
//
//  What a text view's pictures read from the view: its laid-out lines and
//  the row each is part of, each row's picture, and where its text is. A
//  picture's row is the picture alone, on as many lines as it is tall.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextPictureHost
{
public:
    virtual ~IDxuiTextPictureHost() = default;

    virtual int                      GetPictureLineCount  () const          = 0;
    virtual int                      GetPictureLineRow    (int line) const  = 0;
    virtual int                      GetPictureRowCount   () const          = 0;
    virtual const DxuiTextPicture  * GetRowPicture        (int row) const   = 0;
    virtual int                      GetPictureTopLine    () const          = 0;
    virtual int                      GetPictureLineHeight () const          = 0;
    virtual int                      GetPictureTextTop    () const          = 0;
    virtual float                    GetPictureTextLeft   () const          = 0;
    virtual float                    GetPictureTextWidth  () const          = 0;
    virtual const DxuiDpiScaler    & GetPictureScaler     () const          = 0;
};
