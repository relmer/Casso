#pragma once

#include "Pch.h"

class IDxuiPainter;




//
//  Shared Casso brand mark. The rainbow cassowary is Casso's period-Apple
//  analog of the rainbow Apple logo, stamped on the skeuomorphic hardware
//  chrome -- the Disk ][ faceplate (DriveWidget) and the 3D desk scene's
//  monitor chin. Drawn procedurally through IDxuiPainter from a baked
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

    // The baked silhouette and stripe palette, exposed so the 3D desk scene
    // can stamp the SAME mark as geometry -- one source of truth for the
    // brand across the 2D chrome and the scene.
    static constexpr int  kGridW       = 36;
    static constexpr int  kGridH       = 54;
    static constexpr int  kStripeCount = 6;

    static uint64_t  SilhouetteRow (int row);
    static uint32_t  StripeColor   (int stripe);
};
