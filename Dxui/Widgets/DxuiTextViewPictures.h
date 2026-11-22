#pragma once

#include "Pch.h"

#include "DxuiPictureMap.h"

class  IDxuiTextPictureHost;
class  IDxuiTextRenderer;
class  IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures
//
//  The pictures in a text view's rows: painting each, finding the picture
//  pixel under a point, outlining a rectangle of one in the accent color,
//  and the map behavior of a picture that asks for it, zoom, pan and ping.
//  The view's lines and geometry are read through IDxuiTextPictureHost.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewPictures
{
public:
    explicit DxuiTextViewPictures (const IDxuiTextPictureHost & host) : m_host (host) {}

    //  The picture under a point, as its row and the pixel of the picture,
    //  through any zoom; false off a picture.
    bool   HitTest       (POINT point, int & outRow, int & outX, int & outY) const;

    //  A rectangle of a row's picture, in its pixels, outlined in the accent
    //  color, as the cell under the pointer is; row -1 for none. Returns
    //  whether the outline moved.
    bool   SetOutline    (int row, const RECT & pictureRect);
    void   ClearOutline  () { m_outlineRow = -1; }

    //  A row's picture, from its first line or from the top line when its
    //  first is scrolled above the view.
    void   Paint         (IDxuiTextRenderer & text, const IDxuiTheme & theme, int row) const;

    //  How many lines a row's picture takes at a text width and line height.
    int    GetLineCount  (int row, float widthPx, int lineHeightPx) const;

    //  Whether a picture with a ping is in the rows, and the ping's
    //  animation: true when a frame changed, so the view needs painting.
    bool   IsPinging     () const;
    bool   TickPing      (int64_t nowMs);

    //  The wheel and a left drag over a zoomable picture; false off one, so
    //  the view scrolls or selects there.
    bool   Zoom          (POINT point, float notches);
    bool   TryStartPan   (POINT point);
    bool   IsPanning     () const { return m_map.IsPanning(); }
    void   Pan           (POINT point);
    void   EndPan        () { m_map.EndPan(); }

    float  GetMapZoom    () const { return m_map.GetZoom(); }
    POINT  GetMapOrigin  () const { return m_map.GetOrigin(); }

private:
    static constexpr int  kOutlineMinCellDip = 8;   // a cell smaller than this is outlined a cell's width out

    bool                 GetFrame (int row, DxuiPictureMap::Frame & outFrame) const;
    DxuiPictureMap::Key  GetKey   (int row) const;

    const IDxuiTextPictureHost  & m_host;
    int                           m_outlineRow  = -1;
    RECT                          m_outlineRect = {};
    DxuiPictureMap                m_map;
};
