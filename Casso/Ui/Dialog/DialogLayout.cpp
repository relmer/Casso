#include "Pch.h"

#include "DialogLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr float  s_kZeroPx = 0.0f;



    struct WrappedRun
    {
        size_t  runIndex;
        size_t  start;
        size_t  count;
        float   xPx;
        float   yPx;
        float   widthPx;
    };



    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindWrapBoundary
    //
    //  Returns the largest character count in [1..remaining] whose measured
    //  width fits in `maxWidthPx`. Prefers a whitespace break when possible.
    //
    ////////////////////////////////////////////////////////////////////////////

    size_t FindWrapBoundary (
        std::wstring_view                                  text,
        size_t                                             start,
        float                                              maxWidthPx,
        const std::function<float (std::wstring_view)>   & measure)
    {
        size_t  remaining     = text.size() - start;
        size_t  fit           = 0;
        size_t  lastSpace     = 0;
        size_t  i             = 0;
        float   widthPx       = 0.0f;



        if (remaining == 0)
        {
            return 0;
        }

        // Coarse linear probe: extend by 1 char until width exceeds the
        // bound. Cheap for the short body strings dialogs carry.
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
            return 1;
        }

        if (fit < remaining && lastSpace > 0)
        {
            return lastSpace;
        }

        return fit;
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  WrapBody
    //
    //  Greedy line-wrap across all body runs into `maxBodyWidthPx`.
    //  Each emitted `WrappedRun` is a single line's worth of one source
    //  run; a long run spans multiple WrappedRuns. The caller turns each
    //  WrappedRun into a RECT.
    //
    ////////////////////////////////////////////////////////////////////////////

    void WrapBody (
        const std::vector<DialogTextRun>                 & runs,
        float                                              maxBodyWidthPx,
        float                                              lineHeightPx,
        const std::function<float (std::wstring_view)>   & measure,
        std::vector<WrappedRun>                          & outWrapped,
        float                                            & outTotalHeightPx)
    {
        size_t  runIndex     = 0;
        size_t  pos          = 0;
        float   cursorXPx    = 0.0f;
        float   cursorYPx    = 0.0f;
        float   remainingPx  = maxBodyWidthPx;



        outWrapped.clear();
        outTotalHeightPx = 0.0f;

        if (runs.empty())
        {
            return;
        }

        for (runIndex = 0; runIndex < runs.size(); runIndex++)
        {
            const std::wstring & text = runs[runIndex].text;
            pos = 0;
            while (pos < text.size())
            {
                std::wstring_view  view (text);
                size_t             tentative = FindWrapBoundary (view, pos, remainingPx, measure);



                if (tentative == 0)
                {
                    // No room on the current line for even one char — newline.
                    cursorXPx   = 0.0f;
                    cursorYPx  += lineHeightPx;
                    remainingPx = maxBodyWidthPx;
                    continue;
                }

                float  pieceWidthPx = measure (view.substr (pos, tentative));



                WrappedRun  emit { runIndex, pos, tentative, cursorXPx, cursorYPx, pieceWidthPx };
                outWrapped.push_back (emit);

                pos        += tentative;
                cursorXPx  += pieceWidthPx;
                remainingPx = maxBodyWidthPx - cursorXPx;

                if (pos < text.size())
                {
                    cursorXPx   = 0.0f;
                    cursorYPx  += lineHeightPx;
                    remainingPx = maxBodyWidthPx;
                }
            }
        }

        outTotalHeightPx = cursorYPx + (outWrapped.empty() ? 0.0f : lineHeightPx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutDialog
//
//  Pure layout math producing the rects every DialogPrimitive consumer
//  needs (icon slot, per-run body rects, hyperlink hit-rects, button
//  row, optional custom-body rect) plus the overall window content
//  size. Win32-free; all measurement happens through the metrics'
//  callback hooks. Tests pin behavior with deterministic stubs.
//
////////////////////////////////////////////////////////////////////////////////

DialogLayoutResult LayoutDialog (
    const DialogDefinition     & def,
    const DialogLayoutMetrics  & metrics)
{
    DialogLayoutResult       result;
    std::vector<WrappedRun>  wrapped;
    float                    bodyTotalHeightPx = 0.0f;
    float                    bodyOriginXPx     = 0.0f;
    float                    bodyOriginYPx     = 0.0f;
    float                    contentTopPx      = 0.0f;
    float                    contentBottomPx   = 0.0f;
    float                    contentRightPx    = 0.0f;
    bool                     hasIcon           = (def.icon != DialogIcon::None);
    bool                     hasCustomBody     = (def.onPaintCustomBody != nullptr);
    size_t                   wi                = 0;
    size_t                   bi                = 0;
    float                    buttonRowYPx      = 0.0f;
    float                    buttonRowRightPx  = 0.0f;
    float                    iconYPx           = 0.0f;
    float                    iconBlockHeightPx = hasIcon ? metrics.iconSizePx : 0.0f;
    std::vector<float>       buttonWidthsPx;



    bodyOriginXPx = metrics.outerPaddingPx;
    contentTopPx  = metrics.outerPaddingPx;

    if (hasIcon)
    {
        bodyOriginXPx += metrics.iconSizePx + metrics.iconBodyGapPx;
    }

    bodyOriginYPx = contentTopPx;

    WrapBody (def.body,
              metrics.maxBodyWidthPx,
              metrics.bodyLineHeightPx,
              metrics.measureBodyTextRun ? metrics.measureBodyTextRun
                                         : [] (std::wstring_view v) { return (float) v.size() * s_kZeroPx; },
              wrapped,
              bodyTotalHeightPx);

    // Per-run rects (1:1 with def.body). When a run wraps into multiple
    // WrappedRun pieces, we take the union of the pieces' rects so the
    // hyperlink hit-test is the full bounding box of the link's visual
    // run. That matches user expectation — clicking anywhere on the
    // underlined text triggers the link.
    result.bodyRunRectsPx.assign (def.body.size(), RECT {});
    result.hyperlinkHitRectsPx.clear();

    for (wi = 0; wi < wrapped.size(); wi++)
    {
        const WrappedRun &  w     = wrapped[wi];
        RECT &              rect  = result.bodyRunRectsPx[w.runIndex];

        float  leftPx   = bodyOriginXPx + w.xPx;
        float  topPx    = bodyOriginYPx + w.yPx;
        float  rightPx  = leftPx + w.widthPx;
        float  bottomPx = topPx + metrics.bodyLineHeightPx;

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

    for (bi = 0; bi < def.body.size(); bi++)
    {
        if (def.body[bi].isHyperlink)
        {
            result.hyperlinkHitRectsPx.push_back (result.bodyRunRectsPx[bi]);
        }
    }

    contentBottomPx = bodyOriginYPx + std::max (bodyTotalHeightPx, iconBlockHeightPx);
    contentRightPx  = bodyOriginXPx + metrics.maxBodyWidthPx;

    if (hasIcon)
    {
        iconYPx = contentTopPx;
        result.iconRectPx.left   = (LONG) metrics.outerPaddingPx;
        result.iconRectPx.top    = (LONG) iconYPx;
        result.iconRectPx.right  = (LONG) (metrics.outerPaddingPx + metrics.iconSizePx);
        result.iconRectPx.bottom = (LONG) (iconYPx + metrics.iconSizePx);
    }

    if (hasCustomBody)
    {
        float  cbTopPx     = contentBottomPx + metrics.bodyButtonsGapPx;
        float  cbLeftPx    = metrics.outerPaddingPx;
        float  cbWidthPx   = std::max ((float) def.customBodyMinSizePx.cx,
                                       contentRightPx - cbLeftPx);
        float  cbHeightPx  = (float) def.customBodyMinSizePx.cy;

        result.customBodyRectPx.left   = (LONG) cbLeftPx;
        result.customBodyRectPx.top    = (LONG) cbTopPx;
        result.customBodyRectPx.right  = (LONG) (cbLeftPx + cbWidthPx);
        result.customBodyRectPx.bottom = (LONG) (cbTopPx + cbHeightPx);

        contentBottomPx = (float) result.customBodyRectPx.bottom;
        contentRightPx  = std::max (contentRightPx, (float) result.customBodyRectPx.right);
    }

    // Button row: right-aligned within content; spaced by buttonSpacingPx.
    buttonRowYPx     = contentBottomPx + metrics.bodyButtonsGapPx;
    buttonRowRightPx = contentRightPx;

    buttonWidthsPx.reserve (def.buttons.size());
    for (bi = 0; bi < def.buttons.size(); bi++)
    {
        float  labelW = metrics.measureButtonLabel
                            ? metrics.measureButtonLabel (def.buttons[bi].label)
                            : (float) def.buttons[bi].label.size() * s_kZeroPx;
        float  w      = labelW + 2.0f * metrics.buttonPaddingPx;
        if (w < metrics.minButtonWidthPx) { w = metrics.minButtonWidthPx; }
        buttonWidthsPx.push_back (w);
    }

    // Place buttons right-to-left so the rightmost button hugs the
    // content right edge.
    result.buttonRectsPx.assign (def.buttons.size(), RECT {});
    {
        float  cursorRightPx = buttonRowRightPx;
        for (bi = def.buttons.size(); bi > 0; bi--)
        {
            size_t  idx = bi - 1;
            float   w   = buttonWidthsPx[idx];

            RECT &  r = result.buttonRectsPx[idx];
            r.right  = (LONG) cursorRightPx;
            r.left   = (LONG) (cursorRightPx - w);
            r.top    = (LONG) buttonRowYPx;
            r.bottom = (LONG) (buttonRowYPx + metrics.buttonHeightPx);

            cursorRightPx -= (w + metrics.buttonSpacingPx);
        }
    }

    // Total size = right + padding, button-row-bottom + padding (or
    // content-bottom + padding when there are no buttons).
    {
        float  bottomPx = def.buttons.empty()
                            ? contentBottomPx
                            : buttonRowYPx + metrics.buttonHeightPx;

        result.totalSizePx.cx = (LONG) (contentRightPx + metrics.outerPaddingPx);
        result.totalSizePx.cy = (LONG) (bottomPx + metrics.outerPaddingPx);
    }

    return result;
}
