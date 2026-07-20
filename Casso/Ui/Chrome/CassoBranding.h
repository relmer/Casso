#pragma once

#include "Pch.h"

class IDxuiPainter;




//
//  Shared Casso brand mark. The rainbow cassowary is Casso's period-Apple
//  analog of the rainbow Apple logo, stamped on the skeuomorphic hardware
//  chrome -- the Disk ][ faceplate (DriveWidget) and the CRT monitor chin
//  (MonitorFrame). Drawn procedurally through IDxuiPainter from a baked
//  silhouette so it stays crisp at any size and needs no image asset.
//
class CassoBranding
{
public:
    // Fill the given box (host client px) with the rainbow cassowary; the
    // silhouette is letterboxed to the box's aspect. borderArgb (0 = none)
    // draws a ~1px outline around the silhouette's exact shape in that color.
    static void  DrawCassowaryRainbow (IDxuiPainter & painter,
                                       float          left,
                                       float          top,
                                       float          width,
                                       float          height,
                                       uint32_t       borderArgb = 0);
};
