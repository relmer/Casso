#pragma once

#include "Pch.h"

#include "Update/ReleaseNotesFormatter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NotesRunStyle
//
//  How one placed word is drawn: its size in pixels, and whether it is
//  bold, code or a link.
//
////////////////////////////////////////////////////////////////////////////////

struct NotesRunStyle
{
    float  sizePx = 0.0f;
    bool   bold   = false;
    bool   code   = false;
    bool   isLink = false;
    bool   muted  = false;

    bool operator== (const NotesRunStyle &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlacedNotesRun
//
//  One word (or a bullet mark) at its position in the notes body, in
//  pixels relative to the body's top-left corner.
//
////////////////////////////////////////////////////////////////////////////////

struct PlacedNotesRun
{
    std::wstring   text;
    float          x      = 0.0f;
    float          y      = 0.0f;
    float          width  = 0.0f;
    float          height = 0.0f;
    NotesRunStyle  style;
    std::string    linkUrl;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NotesLayoutMetrics
//
//  The sizes the layout works in, already scaled to pixels. Heading sizes
//  are indexed by level - 1.
//
////////////////////////////////////////////////////////////////////////////////

struct NotesLayoutMetrics
{
    static constexpr size_t  kHeadingLevels = 4;

    float                              bodySizePx    = 13.0f;
    float                              codeSizePx    = 12.0f;
    std::array<float, kHeadingLevels>  headingSizePx = { 18.0f, 16.0f, 14.0f, 13.0f };
    float                              lineHeightPx  = 18.0f;
    float                              indentPx      = 18.0f;
    float                              bulletGapPx   = 12.0f;
    float                              blankGapPx    = 8.0f;
    float                              headingGapPx  = 6.0f;
    float                              captionSizePx = 11.0f;
    float                              imageGapPx    = 8.0f;
    float                              placeholderPx = 80.0f;
    float                              imageScale    = 1.0f;   // image pixels to layout pixels (DPI / 96)
};





////////////////////////////////////////////////////////////////////////////////
//
//  NotesImageState
//
//  What the layout knows of one image: nothing yet (a placeholder), its
//  decoded size, or that it could not be shown (its alt text stays).
//
////////////////////////////////////////////////////////////////////////////////

struct NotesImageState
{
    bool  isLoaded = false;
    bool  isFailed = false;
    int   widthPx  = 0;
    int   heightPx = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlacedNotesImage
//
//  An image, or its placeholder box, at its position in the body.
//
////////////////////////////////////////////////////////////////////////////////

struct PlacedNotesImage
{
    std::string   src;
    std::wstring  alt;
    float         x        = 0.0f;
    float         y        = 0.0f;
    float         width    = 0.0f;
    float         height   = 0.0f;
    bool          isLoaded = false;
    bool          isFailed = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayout
//
//  Flows formatted release-note lines into a column of a given width, word
//  by word, so bold, code and link text can sit inside a wrapped
//  paragraph. Measurement is a callback, which keeps the flow a pure
//  function: the dialog measures with DirectWrite, a test with a fixed
//  width per character.
//
////////////////////////////////////////////////////////////////////////////////

class ReleaseNotesLayout
{
public:
    using MeasureFn    = std::function<float (const std::wstring & text, const NotesRunStyle & style)>;
    using ImageStateFn = std::function<NotesImageState (const std::string & src)>;

    static float  Flow       (const std::vector<FormattedLine>  & lines,
                              float                               widthPx,
                              const NotesLayoutMetrics          & metrics,
                              const MeasureFn                   & measure,
                              std::vector<PlacedNotesRun>       & outRuns);
    static float  Flow       (const std::vector<FormattedLine>  & lines,
                              float                               widthPx,
                              const NotesLayoutMetrics          & metrics,
                              const MeasureFn                   & measure,
                              const ImageStateFn                & imageState,
                              std::vector<PlacedNotesRun>       & outRuns,
                              std::vector<PlacedNotesImage>     & outImages);
    static void   PlaceImage (const FormattedImage              & image,
                              const NotesImageState             & state,
                              float                               widthPx,
                              const NotesLayoutMetrics          & metrics,
                              const MeasureFn                   & measure,
                              float                             & y,
                              std::vector<PlacedNotesRun>       & outRuns,
                              std::vector<PlacedNotesImage>     & outImages);

    static const PlacedNotesRun *  FindLinkAt (const std::vector<PlacedNotesRun> & runs, float x, float y);

private:
    struct Cursor
    {
        float  left         = 0.0f;
        float  right        = 0.0f;
        float  x            = 0.0f;
        float  y            = 0.0f;
        float  lineHeight   = 0.0f;
        bool   pendingSpace = false;
    };

    static NotesRunStyle  MakeStyle   (const FormattedLine       & line,
                                       const FormattedRun        & run,
                                       const NotesLayoutMetrics  & metrics);
    static void           PlaceWord   (const std::wstring          & word,
                                       const NotesRunStyle         & style,
                                       const std::string           & linkUrl,
                                       const MeasureFn             & measure,
                                       Cursor                      & cursor,
                                       std::vector<PlacedNotesRun> & outRuns);
    static void           FlowRun     (const FormattedRun          & run,
                                       const NotesRunStyle         & style,
                                       const MeasureFn             & measure,
                                       Cursor                      & cursor,
                                       std::vector<PlacedNotesRun> & outRuns);
};
