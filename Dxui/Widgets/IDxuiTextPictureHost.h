#pragma once

#include "Pch.h"


class DxuiDpiScaler;
struct DxuiTextPicture;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextPictureHost
//
//  The text view a DxuiTextPictures draws and zooms pictures for: its lines
//  and rows, where a line is drawn, the line under a point, and the place
//  and width of its text.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextPictureHost
{
public:
    virtual                          ~IDxuiTextPictureHost() = default;

    virtual int                      GetPictureLineCount () const          = 0;
    virtual int                      GetPictureLineRow   (int line) const  = 0;
    virtual int                      GetPictureLineTopPx (int line) const  = 0;
    virtual int                      GetPictureLineAtPx  (int y) const     = 0;
    virtual int                      GetPictureRowCount  () const          = 0;
    virtual const DxuiTextPicture  * GetRowPicture       (int row) const   = 0;
    virtual float                    GetPictureLeftPx    () const          = 0;
    virtual float                    GetPictureWidthPx   () const          = 0;
    virtual const DxuiDpiScaler    & GetPictureScaler    () const          = 0;
};
