#include "Pch.h"

#include "Core/DxuiMagnifiedRow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRow::GetMagnification
//
//  A raised cosine: full size right under the pointer, falling off smoothly
//  with no corner at either end, so an item grows and shrinks without a jump
//  as the pointer passes. Either side alike, and 1 past the range on either
//  side, where the cosine would come back up to full size at twice the range.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiMagnifiedRow::GetMagnification (float distance, float reach, float maxScale)
{
    constexpr float  kPi = 3.14159265f;
    float            t   = (reach > 0.0f) ? std::fabs (distance) / reach : 1.0f;



    return (t >= 1.0f) ? 1.0f : 1.0f + (maxScale - 1.0f) * 0.5f * (1.0f + std::cos (kPi * t));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRow::Lay
//
//  Each item is scaled by its distance from the pointer, then the row is laid
//  out again from the left with the grown widths and moved to its anchor. At
//  rest, or at a strength of 0, this is the row at rest.
//
//  A row anchored at its center does not move along the screen only when its
//  width is the same wherever the pointer is: raised cosines with a range of
//  two pitches, at one pitch apart, add up to the same total wherever the
//  pointer is. A row anchored at the pointer is right for any spacing, but
//  spreads past its ends; a row that fits is in the span of the row at rest
//  for any spacing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMagnifiedRow::Lay (std::span<const DxuiMagnifiedItem> items, float pointer, float strength, const Options & options, std::span<DxuiMagnifiedSpan> outSpans)
{
    size_t  count    = std::min (items.size(), outSpans.size());
    size_t  last     = (count > 0) ? count - 1 : 0;
    size_t  anchor   = 0;
    float   x        = 0.0f;
    float   widths   = 0.0f;
    float   gaps     = 0.0f;
    float   shift    = 0.0f;
    float   fraction = 0.0f;
    float   fit      = 1.0f;



    for (size_t i = 0; i < count; i++)
    {
        float  full = GetMagnification (pointer - (items[i].left + items[i].width * 0.5f), options.reach, items[i].maxScale);

        outSpans[i].scale = 1.0f + (full - 1.0f) * strength;
        outSpans[i].width = items[i].width * outSpans[i].scale;
        widths           += outSpans[i].width;
        anchor            = (pointer >= items[i].left) ? i : anchor;
    }

    for (size_t i = 0; i < count; i++)
    {
        gaps += GetGap (items, outSpans, i, options.gaps);
    }

    if (options.anchor == Anchor::Fit && widths > 0.0f)
    {
        fit = std::max (0.0f, (items[last].left + items[last].width - items[0].left - gaps) / widths);
        x   = items[0].left;
    }

    for (size_t i = 0; i < count; i++)
    {
        outSpans[i].width *= fit;
        outSpans[i].left   = x;
        x                 += outSpans[i].width + GetGap (items, outSpans, i, options.gaps);
    }

    if (count > 0 && options.anchor == Anchor::Center)
    {
        shift = (items[0].left + items[last].left + items[last].width) * 0.5f - x * 0.5f;
    }

    if (count > 0 && options.anchor == Anchor::Pointer)
    {
        fraction = (items[anchor].width > 0.0f) ? (pointer - items[anchor].left) / items[anchor].width : 0.0f;
        shift    = pointer - (outSpans[anchor].left + fraction * outSpans[anchor].width);
    }

    for (size_t i = 0; i < count; i++)
    {
        outSpans[i].left += shift;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRow::GetGap
//
//  The gap after item i as laid out: as at rest, or grown with the items
//  beside it. None after the last item.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiMagnifiedRow::GetGap (std::span<const DxuiMagnifiedItem> items, std::span<const DxuiMagnifiedSpan> spans, size_t i, Gaps gaps)
{
    bool   isLast = i + 1 >= std::min (items.size(), spans.size());
    float  gap    = isLast ? 0.0f : items[i + 1].left - (items[i].left + items[i].width);



    return (gaps == Gaps::Grow && !isLast) ? gap * (spans[i].scale + spans[i + 1].scale) * 0.5f : gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRow::FindItemAt
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMagnifiedRow::FindItemAt (std::span<const DxuiMagnifiedItem> items, float x)
{
    int  found = -1;



    for (size_t i = 0; i < items.size(); i++)
    {
        found = (x >= items[i].left) ? (int) i : found;
    }

    if (found >= 0 && (size_t) found + 1 == items.size() && x >= items[(size_t) found].left + items[(size_t) found].width)
    {
        found = -1;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRow::SetNear
//
//  The strength starts toward 1 when the pointer arrives and toward 0 when
//  the pointer moves off, from wherever it is at the time.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMagnifiedRow::SetNear (bool isNear, int64_t nowMs)
{
    if (isNear != m_isNear)
    {
        m_from   = GetStrength (nowMs);
        m_fromMs = nowMs;
        m_isNear = isNear;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRow::IsEasing
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMagnifiedRow::IsEasing (int64_t nowMs) const
{
    float  strength = GetStrength (nowMs);



    return m_isNear ? strength < 1.0f : strength > 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRow::GetStrength
//
////////////////////////////////////////////////////////////////////////////////

float DxuiMagnifiedRow::GetStrength (int64_t nowMs) const
{
    float  step = (m_fadeMs > 0) ? (float) (nowMs - m_fromMs) / (float) m_fadeMs : 1.0f;



    return std::clamp (m_from + (m_isNear ? step : -step), 0.0f, 1.0f);
}
