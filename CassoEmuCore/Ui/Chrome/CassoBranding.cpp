#include "Pch.h"

#include "CassoBranding.h"





// Cassowary head + neck silhouette, baked from
// Resources/Branding/Cassowary.png at 36x54 resolution. Stored as a per-row
// 36-bit bitmask (bit 0 = leftmost column). The bitmask format preserves
// per-row concavities -- in particular the gap between the underside of the
// beak and the top of the neck -- that a single (start, end) span per row
// would erroneously fill. File scope so the painter path and the 3D desk
// scene's geometry stamp share one source of truth.
static const uint64_t s_kSilhouette[CassoBranding::kGridH] = {
    0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL,
    0x000000FE00ULL, 0x000001FF80ULL, 0x000003FFC0ULL, 0x000007FFE0ULL, 0x00000FFFC0ULL,
    0x00000FFFC0ULL, 0x00001FFF80ULL, 0x00001FFF00ULL, 0x00003FFF00ULL, 0x00003FFF00ULL,
    0x00007FFE00ULL, 0x00007FFE00ULL, 0x0000FFFE00ULL, 0x0000FFFC00ULL, 0x0000FFFC00ULL,
    0x0000FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL,
    0x0003FFFE00ULL, 0x0003FFFE00ULL, 0x0003FFFF00ULL, 0x0003FFFF80ULL, 0x0003FFFFC0ULL,
    0x0003FFFFC0ULL, 0x0007FFFFE0ULL, 0x0007FFFFE0ULL, 0x0007FFFFE0ULL, 0x000FFFFFF0ULL,
    0x001FFFFFF0ULL, 0x003FFFFFF0ULL, 0x007FFFFFF0ULL, 0x00FFFFFFF0ULL, 0x01FFFFFFF8ULL,
    0x01FFFFFFF8ULL, 0x03F007FFF8ULL, 0x038000FFF8ULL, 0x0200007FF8ULL, 0x0000007FF8ULL,
    0x0000007FF8ULL, 0x0000007FF8ULL, 0x000000FFF8ULL, 0x000000FFF8ULL, 0x000001FFF8ULL,
    0x000001FFF8ULL, 0x000001FFF8ULL, 0x000001FFF0ULL, 0x000003FFF0ULL
};

static const uint32_t s_kStripeColors[CassoBranding::kStripeCount] = {
    0xFF61BB46,  // green
    0xFFFDB827,  // yellow
    0xFFF5821F,  // orange
    0xFFE03A3E,  // red
    0xFF963D97,  // purple
    0xFF009DDC,  // blue
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoBranding::SilhouetteRow
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CassoBranding::SilhouetteRow (int row)
{
    return (row >= 0 && row < kGridH) ? s_kSilhouette[row] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoBranding::StripeColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t CassoBranding::StripeColor (int stripe)
{
    return (stripe >= 0 && stripe < kStripeCount) ? s_kStripeColors[stripe] : 0xFFFFFFFF;
}
