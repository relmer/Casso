#pragma once

#include "Pch.h"

class PrintRaster;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel
//
//  The pure decision math the live preview runs each frame, factored out of the
//  panel's RefreshLive so it is unit-testable (these are precisely the cases that
//  regressed repeatedly while they lived in the exe: the reveal-mask column span,
//  the "live band sits below the snapshot" audio gate, the freeze-heal dirty row,
//  and the audio sample window with its line-wrap detection). Every method is a
//  pure function of scalars (plus a raster for the ink probe); the panel keeps the
//  D2D render, the viewport, the pan/zoom, and the 3D scene and calls these for
//  each decision.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterPreviewModel
{
public:
    struct RevealSpan
    {
        int  loDots = 0;
        int  hiDots = 0;
    };

    struct InkSample
    {
        int  loCol = 0;
        int  hiCol = 0;
    };

    // The reveal-mask column span for the current sweep: a left-to-right pass
    // reveals [0, carriageCol]; a right-to-left pass reveals [carriageCol,
    // kDotsPerRow]. Ink at or below the frontier shows only within this span.
    static RevealSpan  RevealColumnSpan (bool sweepLtr, int carriageCol);

    // Whether the visible strip shrank below the viewport's live row -- a tear-off
    // (eject / discard), so the viewport should rewind to the fresh sheet.
    static bool        StripTornOff (int rowsUsed, int viewportLiveRow);

    // Whether the live pin band at `platenRow` sits outside the snapshotted span
    // [spanFirstRow, spanLastRow]. When it does, a span-sampled ink gate reads
    // blank paper (the eased viewport pan lags a fast print), so the caller keeps
    // the worker-raster ink gate instead -- the missing CATALOG buzz fix.
    static bool        IsLiveBandOutsideSpan (int platenRow, int spanFirstRow, int spanLastRow);

    // The absolute row from which the presented layer must be re-rendered. From
    // the last-rendered platen (not just a fixed window at the current platen) so
    // rows painted while the UI was frozen -- a modal disk picker blocks
    // compositing while the worker keeps printing -- are refreshed, not left with
    // stale pixels. -1 on the first render (mark everything dirty).
    static int         GetDirtyFromRow (bool hasRendered, int platenRow, int renderedPlaten);

    // Change detection so an idle panel does zero render work: whether the visible
    // span or the reveal (frontier row + carriage column) moved since the last
    // render.
    static bool        HasSpanMoved (int firstRow, int lastRow, int renderedFirstRow, int renderedLastRow);
    static bool        RevealMoved  (int revealRow, int revealCol, int renderedRevealRow, int renderedRevealCol);

    // The column window to sample for the audio buzz gate. A line wrap (the row
    // changed, no previous column, or a margin-to-margin jump) samples the whole
    // row -- any ink means a printing pass. Otherwise the window is the span the
    // head swept since the last frame, with a small bridge behind the leading edge
    // so a word keeps buzzing across the blank gaps between its glyphs. Clamped to
    // [0, kDotsPerRow - 1].
    static InkSample   GetAudioSampleWindow (bool sweepLtr, int prevCol, int curCol,
                                             int revealRow, int renderedRevealRow);

    // Whether the pin band at absolute `revealRow` carries any ink in columns
    // [loCol, hiCol], sampled from a span raster whose row 0 is absolute
    // `spanFirstRow`. Drives the audio buzz gate -- inked rows buzz, blank feed /
    // form-feed rows stay silent.
    static bool        HasBandInk (const PrintRaster & spanRaster, int spanFirstRow,
                                   int revealRow, int loCol, int hiCol);
};
