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
//  The pictures in a text view's rows. A painting helper for the view, not
//  a control: the view routes its paint and input here.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewPictures
{
public:
    explicit DxuiTextViewPictures (const IDxuiTextPictureHost & host) : m_host (host) {}

    bool   HitTest       (POINT point, int & outRow, int & outX, int & outY) const;
    bool   SetOutline    (int row, const RECT & pictureRect);
    void   ClearOutline  () { m_outlineRow = -1; }
    void   Paint         (IDxuiTextRenderer & text, const IDxuiTheme & theme, int row) const;
    int    GetLineCount  (int row, float widthPx, int lineHeightPx) const;
    bool   IsPinging     () const;
    bool   TickPing      (int64_t nowMs);
    bool   Zoom          (POINT point, float notches);
    bool   TryStartPan   (POINT point);
    bool   IsPanning     () const { return m_map.IsPanning(); }
    void   Pan           (POINT point);
    void   EndPan        () { m_map.EndPan(); }
    float  GetMapZoom    () const { return m_map.GetZoom(); }
    POINT  GetMapOrigin  () const { return m_map.GetOrigin(); }

private:
    static constexpr int  kOutlineMinCellDip = 8;

    bool                 GetFrame (int row, DxuiPictureMap::Frame & outFrame) const;
    DxuiPictureMap::Key  GetKey   (int row) const;

    const IDxuiTextPictureHost  & m_host;
    int                           m_outlineRow  = -1;
    RECT                          m_outlineRect = {};
    DxuiPictureMap                m_map;
};
