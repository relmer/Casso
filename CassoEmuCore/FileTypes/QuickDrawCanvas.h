#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawCanvas
//
//  A QuickDraw II port drawn into a bitmap of pixel values, 0 to 15 in 320
//  mode and 0 to 3 in 640 mode, with the port's pen, patterns, pen mode,
//  origin and clip. Points and rectangles are in the port's coordinates,
//  which the frame and the origin map to the bitmap. A figure is drawn by
//  testing each pixel of its rectangle against the figure, so frames, paints,
//  erases, inverts and fills of every figure go through one path.
//
////////////////////////////////////////////////////////////////////////////////

class QuickDrawCanvas
{
public:
    struct Point { int v = 0; int h = 0; };
    struct Rect  { int top = 0; int left = 0; int bottom = 0; int right = 0; };

    //  A region's rows from its top, each the h values where inside and
    //  outside switch, and the list is empty for a rectangular region.
    struct Region
    {
        Rect                           bounds;
        std::vector<std::vector<int>>  rows;
    };

    enum class Verb { Frame, Paint, Erase, Invert, Fill };

    using Pattern = std::array<Byte, 32>;
    using PenMask = std::array<Byte, 8>;

    static constexpr int  kModeCopy = 0x0000;
    static constexpr int  kModeOr   = 0x0001;
    static constexpr int  kModeXor  = 0x0002;
    static constexpr int  kModeBic  = 0x0003;
    static constexpr int  kModeNot  = 0x8000;

    QuickDrawCanvas (const Rect & frame, bool mode640);

    void  SetPenSize   (Point size)                         { m_penSize = size; }
    void  SetPenMode   (int mode)                           { m_penMode = mode; }
    void  SetPenPat    (const Pattern & pattern)            { m_penPat  = pattern; }
    void  SetBkPat     (const Pattern & pattern)            { m_bkPat   = pattern; }
    void  SetFillPat   (const Pattern & pattern)            { m_fillPat = pattern; }
    void  SetOvalSize  (Point size)                         { m_ovalSize = size; }
    void  SetOrigin    (Point origin)                       { m_origin   = origin; }
    void  SetClip      (const Region & clip)                { m_clip = clip; m_hasClip = true; }
    void  SetPenMask   (const PenMask & mask)               { m_penMask = mask; }
    void  SetArcRot    (int arcRot)                         { m_arcRot  = arcRot; }

    void  DrawLine     (Point from, Point to);
    void  DrawRect     (Verb verb, const Rect & rect);
    void  DrawRRect    (Verb verb, const Rect & rect);
    void  DrawOval     (Verb verb, const Rect & rect);
    void  DrawArc      (Verb verb, const Rect & rect, int startAngle, int arcAngle);
    void  DrawPoly     (Verb verb, const std::vector<Point> & points);
    void  DrawRegion   (Verb verb, const Region & region);
    void  DrawPixels   (std::span<const Byte> pixels, int rowBytes, bool mode640, const Rect & bounds, const Rect & source, const Rect & dest, int mode, const Region * mask = nullptr);

    void  WriteBgra    (std::vector<uint32_t> & outBgra) const;
    void  PlotPixel    (int h, int v, Byte source, int mode) { Plot (h, v, source, mode); }

    //  A text or pen color's pixel value at a column of the port.
    Byte  GetColorValue (int color, int h) const;

    int   GetWidth     () const                             { return m_width; }
    int   GetHeight    () const                             { return m_height; }
    Byte  GetPixel     (int x, int y) const                 { return m_pixels[(size_t) y * m_width + x]; }

    static bool  IsInRegion (const Region & region, int h, int v);

private:
    using Inside = std::function<bool (int h, int v)>;

    void  DrawFigure   (Verb verb, const Rect & bounds, const Inside & inside, const Inside & inner);
    void  Plot         (int h, int v, Byte source, int mode);
    bool  IsInPenMask  (int h, int v) const;
    Byte  GetPatternAt (const Pattern & pattern, int h, int v) const;
    bool  IsInOval     (const Rect & rect, int h, int v) const;
    bool  IsInRRect    (const Rect & rect, int h, int v) const;
    bool  IsInArc      (const Rect & rect, int startAngle, int arcAngle, int h, int v) const;
    bool  IsInPoly     (const std::vector<Point> & points, int h, int v) const;
    Rect  Inset        (const Rect & rect) const;

    static Pattern  MakeSolid (Byte value);

    Rect               m_frame;
    bool               m_mode640  = false;
    int                m_width    = 0;
    int                m_height   = 0;
    Byte               m_mask     = 0x0F;
    std::vector<Byte>  m_pixels;
    Point              m_penSize  = { 1, 1 };
    int                m_penMode  = kModeCopy;
    Pattern            m_penPat   = {};
    Pattern            m_bkPat    = {};
    Pattern            m_fillPat  = {};
    Point              m_ovalSize = {};
    Point              m_origin   = {};
    Region             m_clip;
    bool               m_hasClip  = false;
    int                m_arcRot   = 0;
    PenMask            m_penMask  = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
};
