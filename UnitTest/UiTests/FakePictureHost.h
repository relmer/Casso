#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakePictureHost
//
//  A view's lines and geometry for DxuiTextViewPictures: a text row, then a
//  picture's row on lines 1 to 3, the text 6 pixels in from the left and
//  top, 16-pixel lines, and room for 400 pixels of picture at 96 DPI.
//
////////////////////////////////////////////////////////////////////////////////

class FakePictureHost : public IDxuiTextPictureHost
{
public:
    FakePictureHost()
    {
        m_scaler.SetDpi (96);
    }

    int                      GetPictureLineCount  () const override         { return (int) m_lineRows.size(); }
    int                      GetPictureLineRow    (int line) const override { return m_lineRows[(size_t) line]; }
    int                      GetPictureRowCount   () const override         { return 2; }
    const DxuiTextPicture  * GetRowPicture        (int row) const override  { return (row == 1) ? &picture : nullptr; }
    int                      GetPictureTopLine    () const override         { return 0; }
    int                      GetPictureLineHeight () const override         { return 16; }
    int                      GetPictureTextTop    () const override         { return 6; }
    float                    GetPictureTextLeft   () const override         { return 6.0f; }
    float                    GetPictureTextWidth  () const override         { return 400.0f; }
    const DxuiDpiScaler    & GetPictureScaler     () const override         { return m_scaler; }

    DxuiTextPicture  picture;

private:
    std::vector<int>  m_lineRows { 0, 1, 1, 1 };
    DxuiDpiScaler     m_scaler;
};
