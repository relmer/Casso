#pragma once

#include "Pch.h"





//
//  Shared Casso brand mark. The rainbow cassowary is Casso's period-Apple
//  analog of the rainbow Apple logo, stamped on the 3D desk scene's hardware.
//  Built from a baked silhouette so it stays crisp at any size and needs no
//  image asset.
//
class CassoBranding
{
public:
    // The baked silhouette and stripe palette, from which the 3D desk scene
    // stamps the mark as geometry -- one source of truth for the brand.
    static constexpr int  kGridW       = 36;
    static constexpr int  kGridH       = 54;
    static constexpr int  kStripeCount = 6;

    static uint64_t  SilhouetteRow (int row);
    static uint32_t  StripeColor   (int stripe);
};
