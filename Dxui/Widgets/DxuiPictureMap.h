#pragma once

#include "Pch.h"

class  DxuiDpiScaler;
class  IDxuiTextRenderer;
class  IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMap
//
//  A text view picture shown as a map: zoomed by the wheel around the
//  pointer, panned by a drag, and pinged where something is, with rings
//  spreading from the point every few seconds. One picture at a time has a
//  zoom and an origin; a different picture in its place starts unzoomed.
//
//  Points and frames are in the view's pixels; the origin and ping are in
//  the picture's.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPictureMap
{
public:
    //  What identifies a picture: its row, its size and its ping point.
    struct Key
    {
        int  row    = -1;
        int  width  = 0;
        int  height = 0;
        int  pingX  = -1;
        int  pingY  = -1;

        bool  operator== (const Key &) const = default;
    };

    //  Where a picture is drawn at its normal size.
    struct Frame
    {
        float  left  = 0.0f;
        float  top   = 0.0f;
        float  scale = 0.0f;
    };

    static constexpr int  kPingPeriodMs  = 3000;
    static constexpr int  kPingRunMs     = 1500;
    static constexpr int  kPingRings     = 3;
    static constexpr int  kPingRadiusDip = 48;
    static constexpr int  kMaxZoom       = 8;

    bool   IsFor      (const Key & key) const { return key == m_key; }
    float  GetZoom    (const Key & key) const { return IsFor (key) ? m_zoom : 1.0f; }
    float  GetOriginX (const Key & key) const { return IsFor (key) ? m_originX : 0.0f; }
    float  GetOriginY (const Key & key) const { return IsFor (key) ? m_originY : 0.0f; }
    float  GetZoom    () const { return m_zoom; }
    POINT  GetOrigin  () const { return POINT { (LONG) m_originX, (LONG) m_originY }; }
    bool   IsPanning  () const { return m_panning; }

    void   Begin      (const Key & key);
    void   Zoom       (const Key & key, const Frame & frame, POINT point, float notches);
    void   StartPan   (const Key & key, POINT point);
    void   Pan        (const Frame & frame, POINT point);
    void   EndPan     () { m_panning = false; }

    //  The ping's animation, given whether a pinging picture is shown: true
    //  when a frame changed, so the view needs painting.
    bool   TickPing   (int64_t nowMs, bool isPinging);

    //  The rings, around (pingX, pingY) of a picture drawn from (left, top)
    //  at `scale`.
    void   PaintPing  (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler,
                       int pingX, int pingY, float left, float top, float scale) const;

private:
    void   ClampOrigin ();

    Key      m_key;
    float    m_zoom         = 1.0f;
    float    m_originX      = 0.0f;
    float    m_originY      = 0.0f;
    bool     m_panning      = false;
    POINT    m_panFrom      = {};
    float    m_panOriginX   = 0.0f;
    float    m_panOriginY   = 0.0f;
    int64_t  m_pingEpochMs  = -1;
    bool     m_pingDrawn    = false;
};
