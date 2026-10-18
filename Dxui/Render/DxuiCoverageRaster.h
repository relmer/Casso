#pragma once

#include "Pch.h"
#include "Render/IDxuiTextRenderer.h"

struct DxuiIconImage;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRect
//
//  A rectangle in pixels, its top corners rounded to one radius and its
//  bottom corners to another. A radius of 0 leaves those corners square.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiCoverageRect
{
    float  left         = 0.0f;
    float  top          = 0.0f;
    float  right        = 0.0f;
    float  bottom       = 0.0f;
    float  topRadius    = 0.0f;
    float  bottomRadius = 0.0f;

    bool operator== (const DxuiCoverageRect &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRaster
//
//  Draws anti-aliased polygons, rounded rectangles and rings into a
//  premultiplied BGRA image on the CPU, so a picture is built once and then
//  drawn by any renderer, or read back by a test. Coordinates are pixels
//  from the image's top left.
//
//  Each pixel takes 16 samples, one in each cell of a 4 by 4 grid, placed so
//  no two share a row or a column; an edge at any angle then covers a pixel
//  in steps of 1/16, and a 45-degree edge through a pixel's corners covers
//  exactly half of it. Each fill lays its color over what is already there,
//  source-over, weighted by how many of the pixel's samples it covers.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiCoverageRaster
{
public:
    static constexpr int  kSamplesPerAxis = 4;
    static constexpr int  kSampleCount    = kSamplesPerAxis * kSamplesPerAxis;

    //  A transparent image.
    static DxuiIconImage  MakeImage (int width, int height);

    //  Each lays a straight-alpha color over the area it covers. A polygon's
    //  points run in order around it, and a point is inside by the even-odd
    //  rule.
    static void  FillPolygon     (DxuiIconImage & image, const std::vector<DxuiPointF> & points, uint32_t argb);
    static void  FillRoundedRect (DxuiIconImage & image, const DxuiCoverageRect & rect, uint32_t argb);
    static void  FillRects       (DxuiIconImage & image, const std::vector<DxuiCoverageRect> & rects, uint32_t argb);

    //  The outer area less the inner one.
    static void  FillPolygonRing (DxuiIconImage & image, const std::vector<DxuiPointF> & outer, const std::vector<DxuiPointF> & inner, uint32_t argb);
    static void  FillRoundedRing (DxuiIconImage & image, const DxuiCoverageRect & outer, const DxuiCoverageRect & inner, uint32_t argb);

    //  The polygon each of whose edges lies `distance` pixels inside the
    //  matching edge of `points`, and whether a point lies inside a polygon.
    static std::vector<DxuiPointF>  InsetPolygon    (const std::vector<DxuiPointF> & points, float distance);
    static bool                     IsInsidePolygon (const std::vector<DxuiPointF> & points, float x, float y);

    //  Whole pictures: one laid over another of the same size at an opacity,
    //  a picture faded to an opacity, and a translucent color laid over what
    //  a picture covers within a rectangle, as if the color lay above it.
    static void  DrawImage   (DxuiIconImage & image, const DxuiIconImage & layer, float opacity);
    static void  FadeImage   (DxuiIconImage & image, float opacity);
    static void  TintCovered (DxuiIconImage & image, const DxuiCoverageRect & rect, uint32_t argb);

private:
    //  Where a fill covers one row of samples, from left up to right.
    struct Span
    {
        float  left  = 0.0f;
        float  right = 0.0f;
    };

    using SpanFn = std::function<void (float y, std::vector<Span> & spans)>;

    static void  Fill            (DxuiIconImage & image, const RECT & bounds, const SpanFn & getSpans, uint32_t argb);
    static void  AddPolygonSpans (const std::vector<DxuiPointF> & points, float y, std::vector<Span> & spans);
    static void  AddRoundedSpans (const DxuiCoverageRect & rect, float y, std::vector<Span> & spans);
    static void  MergeSpans      (std::vector<Span> & spans);
    static void  SubtractSpans   (const std::vector<Span> & holes, std::vector<Span> & spans);
    static RECT  GetBounds       (const std::vector<DxuiPointF> & points);
    static RECT  GetBounds       (const DxuiCoverageRect & rect);
    static void  Blend           (uint32_t & pixel, uint32_t argb, int samplesInside);

    static uint32_t  ScalePixel (uint32_t pixel, float factor);
    static uint32_t  LayPixel   (uint32_t under, uint32_t over, float opacity);
};
