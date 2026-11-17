#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRow
//
//  A row of items that grow as the pointer nears them, as the macOS dock's
//  icons do. Each item grows by a raised cosine of its distance from the
//  pointer: to its largest scale right under it, and back to its size at
//  rest at a set range. The row is then laid out again with the grown items,
//  so they spread apart instead of overlapping. Positions are along the row,
//  in whatever unit the caller uses.
//
//  The pointer's place is measured on the row at rest, so the pointer is over
//  the same item while the row grows under it.
//
//  An instance is the magnification's strength: 0 at rest and 1 at full
//  size, easing in over the fade time once the pointer is near the row and
//  out once the pointer moves off it.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiMagnifiedItem
{
    float  left     = 0.0f;     // at rest
    float  width    = 0.0f;
    float  maxScale = 1.0f;     // right under the pointer
};


struct DxuiMagnifiedSpan
{
    float  left  = 0.0f;
    float  width = 0.0f;
    float  scale = 1.0f;        // the item's growth; in a row that fits, the width grows less
};


class DxuiMagnifiedRow
{
public:
    //  Where the grown row goes: centered where the row at rest is centered;
    //  moved so the point under the pointer is under it again; or in the
    //  span of the row at rest, every width reduced alike to fit.
    enum class Anchor { Center, Pointer, Fit };

    //  The gaps between items: as at rest, or grown with the items beside
    //  them.
    enum class Gaps { Fixed, Grow };

    struct Options
    {
        float   reach  = 0.0f;
        Anchor  anchor = Anchor::Center;
        Gaps    gaps   = Gaps::Fixed;
    };

    static constexpr int64_t  kFadeMs = 140;

    explicit DxuiMagnifiedRow (int64_t fadeMs = kFadeMs) : m_fadeMs (fadeMs) {}

    //  An item's scale with its center `distance` from the pointer: maxScale
    //  right under it, falling to 1 once the distance is the range or more.
    static float  GetMagnification (float distance, float reach, float maxScale);

    //  The items, left to right, laid out for the pointer at `pointer` on the
    //  row at rest, at `strength`. outSpans has a span for each item.
    static void   Lay              (std::span<const DxuiMagnifiedItem> items, float pointer, float strength, const Options & options, std::span<DxuiMagnifiedSpan> outSpans);

    //  The item at rest under x, with the gap after an item counted as part
    //  of that item; -1 before the first item or past the last.
    static int    FindItemAt       (std::span<const DxuiMagnifiedItem> items, float x);

    void   SetNear     (bool isNear, int64_t nowMs);
    bool   IsNear      () const { return m_isNear; }
    float  GetStrength (int64_t nowMs) const;

    //  Whether the row is magnified or easing in or out, so its host draws
    //  each frame until it settles.
    bool   IsActive    (int64_t nowMs) const { return m_isNear || GetStrength (nowMs) > 0.0f; }

    //  Whether the strength is changing.
    bool   IsEasing    (int64_t nowMs) const;

private:
    int64_t  m_fadeMs   = kFadeMs;
    bool     m_isNear   = false;
    float    m_from     = 0.0f;     // the strength when the pointer last arrived or left
    int64_t  m_fromMs   = 0;        // and when
};
