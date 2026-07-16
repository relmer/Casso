#include "Pch.h"

#include "MockDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FillRect
//
////////////////////////////////////////////////////////////////////////////////

void MockDxuiPainter::FillRect (float xPx, float yPx, float widthPx, float heightPx, uint32_t argbColor)
{
    RecordedPaintCall  call;


    call.kind   = RecordedPaintKind::FillRect;
    call.x      = xPx;
    call.y      = yPx;
    call.width  = widthPx;
    call.height = heightPx;
    call.argb   = argbColor;
    m_calls.push_back (call);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillGradientRect
//
////////////////////////////////////////////////////////////////////////////////

void MockDxuiPainter::FillGradientRect (float xPx, float yPx, float widthPx, float heightPx, uint32_t argbTop, uint32_t argbBottom)
{
    RecordedPaintCall  call;


    call.kind       = RecordedPaintKind::FillGradientRect;
    call.x          = xPx;
    call.y          = yPx;
    call.width      = widthPx;
    call.height     = heightPx;
    call.argb       = argbTop;
    call.argbSecond = argbBottom;
    m_calls.push_back (call);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OutlineRect
//
////////////////////////////////////////////////////////////////////////////////

void MockDxuiPainter::OutlineRect (float xPx, float yPx, float widthPx, float heightPx, float thicknessPx, uint32_t argbColor)
{
    RecordedPaintCall  call;


    call.kind      = RecordedPaintKind::OutlineRect;
    call.x         = xPx;
    call.y         = yPx;
    call.width     = widthPx;
    call.height    = heightPx;
    call.thickness = thicknessPx;
    call.argb      = argbColor;
    m_calls.push_back (call);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillCircleApprox
//
////////////////////////////////////////////////////////////////////////////////

void MockDxuiPainter::FillCircleApprox (float cxPx, float cyPx, float radiusPx, uint32_t argbColor)
{
    RecordedPaintCall  call;


    call.kind   = RecordedPaintKind::FillCircleApprox;
    call.x      = cxPx;
    call.y      = cyPx;
    call.width  = radiusPx * 2.0f;
    call.height = radiusPx * 2.0f;
    call.argb   = argbColor;
    m_calls.push_back (call);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Glyph primitives -- each recorded as its bounding box so tests
//  can assert containment uniformly.
//
////////////////////////////////////////////////////////////////////////////////

void MockDxuiPainter::FillConvexQuad (float x0, float y0, float x1, float y1,
                                      float x2, float y2, float x3, float y3, uint32_t argbColor)
{
    RecordedPaintCall  call;
    float  minX = std::min (std::min (x0, x1), std::min (x2, x3));
    float  maxX = std::max (std::max (x0, x1), std::max (x2, x3));
    float  minY = std::min (std::min (y0, y1), std::min (y2, y3));
    float  maxY = std::max (std::max (y0, y1), std::max (y2, y3));

    call.kind   = RecordedPaintKind::FillConvexQuad;
    call.x      = minX;
    call.y      = minY;
    call.width  = maxX - minX;
    call.height = maxY - minY;
    call.argb   = argbColor;
    m_calls.push_back (call);
}


void MockDxuiPainter::FillEllipseApprox (float cxPx, float cyPx, float radiusXPx, float radiusYPx, uint32_t argbColor)
{
    RecordedPaintCall  call;

    call.kind   = RecordedPaintKind::FillEllipseApprox;
    call.x      = cxPx - radiusXPx;
    call.y      = cyPx - radiusYPx;
    call.width  = radiusXPx * 2.0f;
    call.height = radiusYPx * 2.0f;
    call.argb   = argbColor;
    m_calls.push_back (call);
}


void MockDxuiPainter::DrawLineApprox (float x0, float y0, float x1, float y1, float thicknessPx, uint32_t argbColor)
{
    RecordedPaintCall  call;

    call.kind      = RecordedPaintKind::DrawLineApprox;
    call.x         = std::min (x0, x1);
    call.y         = std::min (y0, y1);
    call.width     = std::max (x0, x1) - call.x;
    call.height    = std::max (y0, y1) - call.y;
    call.thickness = thicknessPx;
    call.argb      = argbColor;
    m_calls.push_back (call);
}
