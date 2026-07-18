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
//  Fills the given box (host client px); the silhouette is letterboxed to the
//  box's aspect by the caller choosing width/height.
//
//  borderArgb (default 0 = none): when opaque, a ~1px outline is drawn around
//  the silhouette's exact shape (not a bounding box) in that color, so the mark
//  reads against a same-tone background.
//
void DrawCassowaryRainbow (IDxuiPainter & painter,
                           float          left,
                           float          top,
                           float          width,
                           float          height,
                           uint32_t       borderArgb = 0);
