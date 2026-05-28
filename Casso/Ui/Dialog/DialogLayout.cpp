#include "Pch.h"

#include "DialogLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::FindWrapBoundary
//
//  Returns the largest character count in [1..remaining] whose measured
//  width fits in `maxWidthPx`. Prefers a whitespace break when possible.
//  Single-exit.
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogLayout::FindWrapBoundary (
    std::wstring_view                                  text,
    size_t                                             start,
    float                                              maxWidthPx,
    const std::function<float (std::wstring_view)>   & measure)
{
    size_t  remaining = text.size() - start;
    size_t  fit       = 0;
    size_t  lastSpace = 0;
    size_t  i         = 0;
    size_t  result    = 0;
    float   widthPx   = 0.0f;



    if (remaining > 0)
    {
        for (i = 1; i <= remaining; i++)
        {
            widthPx = measure (text.substr (start, i));
            if (widthPx > maxWidthPx)
            {
                break;
            }
            fit = i;
            if (text[start + i - 1] == L' ')
            {
                lastSpace = i;
            }
        }

        if (fit == 0)
        {
            // Single character overflows the line — force a 1-char break.
            result = 1;
        }
        else if (fit < remaining && lastSpace > 0)
        {
            result = lastSpace;
        }
        else
        {
            result = fit;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::WrapBody
//
//  Greedy line-wrap across all body runs into `maxBodyWidthPx`. Each
//  emitted `WrappedRun` is one line's worth of one source run; a long
//  run spans multiple WrappedRuns.
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::WrapBody (
    const std::vector<DialogTextRun>                 & runs,
    float                                              maxBodyWidthPx,
    float                                              lineHeightPx,
    const std::function<float (std::wstring_view)>   & measure,
    std::vector<WrappedRun>                          & outWrapped,
    float                                            & outTotalHeightPx)
{
    size_t  runIndex     = 0;
    size_t  pos          = 0;
    size_t  tentative    = 0;
    float   cursorXPx    = 0.0f;
    float   cursorYPx    = 0.0f;
    float   remainingPx  = maxBodyWidthPx;
    float   pieceWidthPx = 0.0f;



    outWrapped.clear();
    outTotalHeightPx = 0.0f;

    for (runIndex = 0; runIndex < runs.size(); runIndex++)
    {
        std::wstring_view  view (runs[runIndex].text);
        pos = 0;
        while (pos < view.size())
        {
            tentative = FindWrapBoundary (view, pos, remainingPx, measure);
            if (tentative == 0)
            {
                cursorXPx   = 0.0f;
                cursorYPx  += lineHeightPx;
                remainingPx = maxBodyWidthPx;
            }
            else
            {
                pieceWidthPx = measure (view.substr (pos, tentative));
                outWrapped.push_back ({ runIndex, pos, tentative, cursorXPx, cursorYPx, pieceWidthPx });
                pos        += tentative;
                cursorXPx  += pieceWidthPx;
                remainingPx = maxBodyWidthPx - cursorXPx;
                if (pos < view.size())
                {
                    cursorXPx   = 0.0f;
                    cursorYPx  += lineHeightPx;
                    remainingPx = maxBodyWidthPx;
                }
            }
        }
    }

    outTotalHeightPx = cursorYPx + (outWrapped.empty() ? 0.0f : lineHeightPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::BeginLayout
//
//  Sets up the body/icon origins from the metrics. Must run before any
//  build-rects helper.
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::BeginLayout (LayoutState & s)
{
    s.bodyOriginXPx = s.metrics->outerPaddingPx;
    s.bodyOriginYPx = s.metrics->outerPaddingPx;
    s.hasIcon       = (s.def->icon != DialogIcon::None);
    s.hasCustomBody = (s.def->onPaintCustomBody != nullptr);

    if (s.hasIcon)
    {
        s.bodyOriginXPx += s.metrics->iconSizePx + s.metrics->iconBodyGapPx;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::PerformBodyWrap
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::PerformBodyWrap (LayoutState & s)
{
    auto  fallback = [] (std::wstring_view v) { return (float) v.size() * 0.0f; };

    WrapBody (s.def->body,
              s.metrics->maxBodyWidthPx,
              s.metrics->bodyLineHeightPx,
              s.metrics->measureBodyTextRun ? s.metrics->measureBodyTextRun : fallback,
              s.wrapped,
              s.bodyTotalHeightPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::BuildBodyRunRects
//
//  Turns each WrappedRun into (or unions with) the corresponding source
//  run's rect. Multi-line runs end up with their bounding-box rect so
//  hyperlink hit-testing covers the full underlined region.
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::BuildBodyRunRects (LayoutState & s)
{
    size_t  wi       = 0;
    float   leftPx   = 0.0f;
    float   topPx    = 0.0f;
    float   rightPx  = 0.0f;
    float   bottomPx = 0.0f;



    s.result->bodyRunRectsPx.assign (s.def->body.size(), RECT {});

    for (wi = 0; wi < s.wrapped.size(); wi++)
    {
        const WrappedRun &  w    = s.wrapped[wi];
        RECT &              rect = s.result->bodyRunRectsPx[w.runIndex];

        leftPx   = s.bodyOriginXPx + w.xPx;
        topPx    = s.bodyOriginYPx + w.yPx;
        rightPx  = leftPx + w.widthPx;
        bottomPx = topPx + s.metrics->bodyLineHeightPx;

        if (rect.right == 0 && rect.bottom == 0)
        {
            rect.left   = (LONG) leftPx;
            rect.top    = (LONG) topPx;
            rect.right  = (LONG) rightPx;
            rect.bottom = (LONG) bottomPx;
        }
        else
        {
            rect.left   = std::min (rect.left,   (LONG) leftPx);
            rect.top    = std::min (rect.top,    (LONG) topPx);
            rect.right  = std::max (rect.right,  (LONG) rightPx);
            rect.bottom = std::max (rect.bottom, (LONG) bottomPx);
        }
    }

    s.contentBottomPx = s.bodyOriginYPx + std::max (s.bodyTotalHeightPx,
                                                    s.hasIcon ? s.metrics->iconSizePx : 0.0f);
    s.contentRightPx  = s.bodyOriginXPx + s.metrics->maxBodyWidthPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::BuildHyperlinkHitRects
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::BuildHyperlinkHitRects (LayoutState & s)
{
    size_t  bi = 0;



    s.result->hyperlinkHitRectsPx.clear();
    for (bi = 0; bi < s.def->body.size(); bi++)
    {
        if (s.def->body[bi].isHyperlink)
        {
            s.result->hyperlinkHitRectsPx.push_back (s.result->bodyRunRectsPx[bi]);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::BuildIconRect
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::BuildIconRect (LayoutState & s)
{
    if (s.hasIcon)
    {
        s.result->iconRectPx.left   = (LONG) s.metrics->outerPaddingPx;
        s.result->iconRectPx.top    = (LONG) s.metrics->outerPaddingPx;
        s.result->iconRectPx.right  = (LONG) (s.metrics->outerPaddingPx + s.metrics->iconSizePx);
        s.result->iconRectPx.bottom = (LONG) (s.metrics->outerPaddingPx + s.metrics->iconSizePx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::BuildCustomBodyRect
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::BuildCustomBodyRect (LayoutState & s)
{
    float  cbTopPx    = 0.0f;
    float  cbLeftPx   = 0.0f;
    float  cbWidthPx  = 0.0f;
    float  cbHeightPx = 0.0f;



    if (s.hasCustomBody)
    {
        cbTopPx    = s.contentBottomPx + s.metrics->bodyButtonsGapPx;
        cbLeftPx   = s.metrics->outerPaddingPx;
        cbWidthPx  = std::max ((float) s.def->customBodyMinSizePx.cx,
                               s.contentRightPx - cbLeftPx);
        cbHeightPx = (float) s.def->customBodyMinSizePx.cy;

        s.result->customBodyRectPx.left   = (LONG) cbLeftPx;
        s.result->customBodyRectPx.top    = (LONG) cbTopPx;
        s.result->customBodyRectPx.right  = (LONG) (cbLeftPx + cbWidthPx);
        s.result->customBodyRectPx.bottom = (LONG) (cbTopPx + cbHeightPx);

        s.contentBottomPx = (float) s.result->customBodyRectPx.bottom;
        s.contentRightPx  = std::max (s.contentRightPx, (float) s.result->customBodyRectPx.right);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::BuildButtonRects
//
//  Right-aligned within the content. Width = label + 2*padding, clamped
//  by minButtonWidthPx. Placed right-to-left so the rightmost button
//  hugs the content right edge.
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::BuildButtonRects (LayoutState & s)
{
    std::vector<float>  widthsPx;
    size_t              bi             = 0;
    size_t              idx            = 0;
    float               labelW         = 0.0f;
    float               w              = 0.0f;
    float               cursorRightPx  = s.contentRightPx;
    float               rowTopPx       = s.contentBottomPx + s.metrics->bodyButtonsGapPx;



    widthsPx.reserve (s.def->buttons.size());
    for (bi = 0; bi < s.def->buttons.size(); bi++)
    {
        labelW = s.metrics->measureButtonLabel
                     ? s.metrics->measureButtonLabel (s.def->buttons[bi].label)
                     : 0.0f;
        w = labelW + 2.0f * s.metrics->buttonPaddingPx;
        if (w < s.metrics->minButtonWidthPx)
        {
            w = s.metrics->minButtonWidthPx;
        }
        widthsPx.push_back (w);
    }

    s.result->buttonRectsPx.assign (s.def->buttons.size(), RECT {});
    for (bi = s.def->buttons.size(); bi > 0; bi--)
    {
        idx = bi - 1;
        w   = widthsPx[idx];

        RECT &  r = s.result->buttonRectsPx[idx];
        r.right  = (LONG) cursorRightPx;
        r.left   = (LONG) (cursorRightPx - w);
        r.top    = (LONG) rowTopPx;
        r.bottom = (LONG) (rowTopPx + s.metrics->buttonHeightPx);

        cursorRightPx -= (w + s.metrics->buttonSpacingPx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::ComputeTotalSize
//
////////////////////////////////////////////////////////////////////////////////

void DialogLayout::ComputeTotalSize (LayoutState & s)
{
    float  rowTopPx = s.contentBottomPx + s.metrics->bodyButtonsGapPx;
    float  bottomPx = s.def->buttons.empty()
                          ? s.contentBottomPx
                          : rowTopPx + s.metrics->buttonHeightPx;

    s.result->totalSizePx.cx = (LONG) (s.contentRightPx + s.metrics->outerPaddingPx);
    s.result->totalSizePx.cy = (LONG) (bottomPx + s.metrics->outerPaddingPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout::Compute
//
//  Pure layout math producing the rects every DialogPrimitive consumer
//  needs (icon slot, per-run body rects, hyperlink hit-rects, button
//  row, optional custom-body rect) plus the overall window content
//  size. Win32-free; all measurement happens through the metrics'
//  callback hooks. Tests pin behavior with deterministic stubs.
//
////////////////////////////////////////////////////////////////////////////////

DialogLayoutResult DialogLayout::Compute (
    const DialogDefinition     & def,
    const DialogLayoutMetrics  & metrics)
{
    DialogLayoutResult  result;
    LayoutState         s;

    s.def     = & def;
    s.metrics = & metrics;
    s.result  = & result;

    BeginLayout            (s);
    PerformBodyWrap        (s);
    BuildBodyRunRects      (s);
    BuildHyperlinkHitRects (s);
    BuildIconRect          (s);
    BuildCustomBodyRect    (s);
    BuildButtonRects       (s);
    ComputeTotalSize       (s);

    return result;
}
