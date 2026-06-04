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
