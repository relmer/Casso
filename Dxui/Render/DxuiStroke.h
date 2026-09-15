#pragma once

#include "Pch.h"
#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStroke
//
//  Lines drawn as filled quads along them. The painter smooths the edges of a
//  quad and not those of a stroked line, so a diagonal drawn this way comes
//  out smooth rather than stepped.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiStroke
{
public:
    static void  Segment (IDxuiPainter & painter, float x0, float y0, float x1, float y1, float thickness, uint32_t argb)
    {
        float  dx     = x1 - x0;
        float  dy     = y1 - y0;
        float  length = std::sqrt (dx * dx + dy * dy);
        float  nx     = 0.0f;
        float  ny     = 0.0f;



        if (length <= 0.0f)
        {
            return;
        }

        nx = -dy / length * thickness * 0.5f;
        ny =  dx / length * thickness * 0.5f;

        painter.FillConvexQuad (x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, argb);
    }
};
