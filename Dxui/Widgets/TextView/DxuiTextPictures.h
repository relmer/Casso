#pragma once

#include "Pch.h"

#include "Widgets/TextView/DxuiPictureMap.h"

class   DxuiTextLayout;
class   IDxuiTextGeometry;
class   IDxuiTextRenderer;
class   IDxuiTextViewport;
class   IDxuiTheme;
struct  DxuiTextPicture;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures
//
//  The pictures in a text view's rows. A painting helper for the view, not
//  a control: the view routes its paint and input here.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextPictures
{
public:
    DxuiTextPictures (const DxuiTextLayout & layout, const IDxuiTextGeometry & geometry, const IDxuiTextViewport & viewport)
        : m_layout (layout), m_geometry (geometry), m_viewport (viewport) {}

    bool   HitTest       (POINT point, int & outRow, int & outX, int & outY) const;
    bool   IsOverMap     (POINT point) const;
    bool   SetOutline    (int row, const RECT & pictureRect);
    void   ClearOutline  () { m_outlineRow = -1; }
    void   Paint         (IDxuiTextRenderer & text, const IDxuiTheme & theme, int row) const;
    bool   IsPinging     () const;
    bool   TickPing      (int64_t nowMs);
    bool   Zoom          (POINT point, float notches);
    bool   TryStartPan   (POINT point);
    bool   IsPanning     () const { return m_map.IsPanning(); }
    void   Pan           (POINT point);
    void   EndPan        () { m_map.EndPan(); }
    float  GetMapZoom    () const { return m_map.GetZoom(); }
    POINT  GetMapOrigin  () const { return m_map.GetOrigin(); }

    static float  GetPictureScale (const DxuiTextPicture & picture, float widthPx, float pxPerDip);
    static int    GetPictureLines (const DxuiTextPicture & picture, float widthPx, float pxPerDip, int lineHeightPx);
    static void   PaintPicture    (IDxuiTextRenderer & text, const DxuiTextPicture & picture, float left, float top, float widthPx, float pxPerDip);

private:
    static constexpr int  kOutlineMinCellDip = 8;

    const DxuiTextPicture  * GetPicture   (int row) const;
    float                    GetTextLeft  () const;
    float                    GetTextWidth () const;
    bool                     GetFrame     (int row, DxuiPictureMap::Frame & outFrame) const;
    DxuiPictureMap::Key      GetKey       (int row) const;

    const DxuiTextLayout     & m_layout;
    const IDxuiTextGeometry  & m_geometry;
    const IDxuiTextViewport  & m_viewport;
    int                        m_outlineRow  = -1;
    RECT                       m_outlineRect = {};
    DxuiPictureMap             m_map;
};
