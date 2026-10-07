#include "Pch.h"

#include "Ui/Dialogs/ReleaseNotesLayout.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout::MakeStyle
//
//  A heading is bold at its level's size; code uses the code size; any other
//  run takes the body size with its own bold and link flags.
//
////////////////////////////////////////////////////////////////////////////////

NotesRunStyle ReleaseNotesLayout::MakeStyle (
    const FormattedLine       & line,
    const FormattedRun        & run,
    const NotesLayoutMetrics  & metrics)
{
    NotesRunStyle  style;
    size_t         level = 0;



    style.sizePx = metrics.bodySizePx;
    style.bold   = run.bold;
    style.code   = run.code;
    style.isLink = !run.linkUrl.empty();

    if (line.kind == FormattedLineKind::Heading)
    {
        level        = (size_t) std::clamp (line.headingLevel, 1, (int) NotesLayoutMetrics::kHeadingLevels);
        style.sizePx = metrics.headingSizePx[level - 1];
        style.bold   = true;
    }
    else if (run.code)
    {
        style.sizePx = metrics.codeSizePx;
    }

    return style;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout::PlaceWord
//
//  Puts one word at the cursor, wrapping first when it would run past the
//  right edge. A word wider than the whole column is placed anyway, at the
//  start of its own line; there is nowhere better for it.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesLayout::PlaceWord (
    const std::wstring          & word,
    const NotesRunStyle         & style,
    const std::string           & linkUrl,
    const MeasureFn             & measure,
    Cursor                      & cursor,
    std::vector<PlacedNotesRun> & outRuns)
{
    PlacedNotesRun  placed;
    float           width      = measure (word, style);
    float           spaceWidth = 0.0f;



    if (cursor.pendingSpace && cursor.x > cursor.left)
    {
        spaceWidth = measure (L" ", style);
    }

    if (cursor.x > cursor.left && cursor.x + spaceWidth + width > cursor.right)
    {
        cursor.x   = cursor.left;
        cursor.y  += cursor.lineHeight;
        spaceWidth = 0.0f;
    }

    cursor.x += spaceWidth;

    placed.text    = word;
    placed.x       = cursor.x;
    placed.y       = cursor.y;
    placed.width   = width;
    placed.height  = cursor.lineHeight;
    placed.style   = style;
    placed.linkUrl = linkUrl;
    outRuns.push_back (std::move (placed));

    cursor.x           += width;
    cursor.pendingSpace = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout::FlowRun
//
//  Splits a run at whitespace and places each word. Whitespace between
//  words, or at either end of a run, becomes one space before the next
//  word; a run that starts with no whitespace joins the word before it, so
//  "`code`," keeps its comma.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesLayout::FlowRun (
    const FormattedRun          & run,
    const NotesRunStyle         & style,
    const MeasureFn             & measure,
    Cursor                      & cursor,
    std::vector<PlacedNotesRun> & outRuns)
{
    std::wstring  text = TextEncoding::Utf8ToWide (run.text);
    std::wstring  word;



    for (wchar_t ch : text)
    {
        if (ch == L' ' || ch == L'\t')
        {
            if (!word.empty())
            {
                PlaceWord (word, style, run.linkUrl, measure, cursor, outRuns);
                word.clear();
            }

            cursor.pendingSpace = true;
            continue;
        }

        word += ch;
    }

    if (!word.empty())
    {
        PlaceWord (word, style, run.linkUrl, measure, cursor, outRuns);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout::Flow
//
//  Lays every line out top to bottom and returns the total height. A blank
//  line is a short gap rather than a full line; a heading after other text
//  gets a gap above it; a bullet is indented by its nesting depth with its
//  mark in the gutter to its left.
//
////////////////////////////////////////////////////////////////////////////////

float ReleaseNotesLayout::Flow (
    const std::vector<FormattedLine>  & lines,
    float                               widthPx,
    const NotesLayoutMetrics          & metrics,
    const MeasureFn                   & measure,
    std::vector<PlacedNotesRun>       & outRuns)
{
    std::vector<PlacedNotesImage>  images;



    return Flow (lines, widthPx, metrics, measure, nullptr, outRuns, images);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout::PlaceImage
//
//  An image fills the body width, or its percent of it, but never grows past
//  its own size at the current DPI; the aspect is kept. Until it loads, and
//  if it fails, a box of the placeholder height holds the alt text. The
//  caption sits centered beneath in the smaller, muted caption style.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesLayout::PlaceImage (
    const FormattedImage              & image,
    const NotesImageState             & state,
    float                               widthPx,
    const NotesLayoutMetrics          & metrics,
    const MeasureFn                   & measure,
    float                             & y,
    std::vector<PlacedNotesRun>       & outRuns,
    std::vector<PlacedNotesImage>     & outImages)
{
    PlacedNotesImage  placed;
    PlacedNotesRun    caption;
    float             target  = (image.widthPercent > 0) ? widthPx * (float) image.widthPercent / 100.0f : widthPx;
    float             native  = 0.0f;



    if (y > 0.0f)
    {
        y += metrics.imageGapPx;
    }

    placed.src      = image.src;
    placed.alt      = TextEncoding::Utf8ToWide (image.alt);
    placed.isLoaded = state.isLoaded && state.widthPx > 0 && state.heightPx > 0;
    placed.isFailed = !placed.isLoaded && state.isFailed;
    placed.width    = target;
    placed.height   = metrics.placeholderPx;

    if (placed.isLoaded)
    {
        native        = (float) state.widthPx * metrics.imageScale;
        placed.width  = std::min (target, native);
        placed.height = placed.width * (float) state.heightPx / (float) state.widthPx;
    }

    placed.x = (widthPx - placed.width) / 2.0f;
    placed.y = y;
    y       += placed.height;
    outImages.push_back (std::move (placed));

    if (!image.caption.empty())
    {
        caption.text         = TextEncoding::Utf8ToWide (image.caption);
        caption.style.sizePx = metrics.captionSizePx;
        caption.style.muted  = true;
        caption.width        = std::min (measure (caption.text, caption.style), widthPx);
        caption.height       = metrics.lineHeightPx * metrics.captionSizePx / metrics.bodySizePx;
        caption.x            = (widthPx - caption.width) / 2.0f;
        caption.y            = y;
        y                   += caption.height;
        outRuns.push_back (std::move (caption));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout::Flow
//
//  As above, with images: each Image line is placed by PlaceImage, using
//  what imageState reports for its source (a null function reports every
//  image as not loaded yet).
//
////////////////////////////////////////////////////////////////////////////////

float ReleaseNotesLayout::Flow (
    const std::vector<FormattedLine>  & lines,
    float                               widthPx,
    const NotesLayoutMetrics          & metrics,
    const MeasureFn                   & measure,
    const ImageStateFn                & imageState,
    std::vector<PlacedNotesRun>       & outRuns,
    std::vector<PlacedNotesImage>     & outImages)
{
    Cursor          cursor;
    NotesRunStyle   bulletStyle;
    PlacedNotesRun  bullet;
    float           y      = 0.0f;
    float           indent = 0.0f;



    outRuns.clear();
    outImages.clear();

    bulletStyle.sizePx = metrics.bodySizePx;

    for (const FormattedLine & line : lines)
    {
        if (line.kind == FormattedLineKind::Blank)
        {
            y += metrics.blankGapPx;
            continue;
        }

        if (line.kind == FormattedLineKind::Image)
        {
            PlaceImage (line.image, imageState ? imageState (line.image.src) : NotesImageState {}, widthPx, metrics, measure, y, outRuns, outImages);
            continue;
        }

        indent = 0.0f;

        if (line.kind == FormattedLineKind::Bullet)
        {
            indent = metrics.indentPx * (float) (line.indentLevel + 1);
        }

        if (line.kind == FormattedLineKind::Heading && y > 0.0f)
        {
            y += metrics.headingGapPx;
        }

        cursor              = {};
        cursor.left         = indent;
        cursor.right        = std::max (widthPx, indent + 1.0f);
        cursor.x            = indent;
        cursor.y            = y;
        cursor.lineHeight   = metrics.lineHeightPx;

        if (line.kind == FormattedLineKind::Heading && !line.runs.empty())
        {
            cursor.lineHeight = metrics.lineHeightPx * MakeStyle (line, line.runs.front(), metrics).sizePx / metrics.bodySizePx;
        }

        if (line.kind == FormattedLineKind::Bullet)
        {
            bullet        = {};
            bullet.text   = std::wstring (1, s_kchBullet);
            bullet.x      = indent - metrics.bulletGapPx;
            bullet.y      = y;
            bullet.width  = metrics.bulletGapPx;
            bullet.height = cursor.lineHeight;
            bullet.style  = bulletStyle;
            outRuns.push_back (bullet);
        }

        for (const FormattedRun & run : line.runs)
        {
            FlowRun (run, MakeStyle (line, run, metrics), measure, cursor, outRuns);
        }

        y = cursor.y + cursor.lineHeight;
    }

    return y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout::FindLinkAt
//
//  The link word under a point, or null.
//
////////////////////////////////////////////////////////////////////////////////

const PlacedNotesRun * ReleaseNotesLayout::FindLinkAt (const std::vector<PlacedNotesRun> & runs, float x, float y)
{
    const PlacedNotesRun  * found = nullptr;



    for (const PlacedNotesRun & run : runs)
    {
        if (!run.linkUrl.empty() &&
            x >= run.x && x < run.x + run.width &&
            y >= run.y && y < run.y + run.height)
        {
            found = &run;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout::ScaleScrollPos
//
//  Where to scroll after the notes reflowed to a new height: the same
//  fraction of the way down, so a resize keeps the reader near what they
//  were reading. With no old height there is nothing to keep, and the top
//  is right.
//
////////////////////////////////////////////////////////////////////////////////

int ReleaseNotesLayout::ScaleScrollPos (int oldPosPx, int oldHeightPx, int newHeightPx)
{
    int  pos = 0;



    if (oldHeightPx > 0 && newHeightPx > 0 && oldPosPx > 0)
    {
        pos = (int) std::lround ((double) oldPosPx * (double) newHeightPx / (double) oldHeightPx);
    }

    return pos;
}