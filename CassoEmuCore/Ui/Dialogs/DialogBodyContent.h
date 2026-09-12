#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"

#include "DialogDefinition.h"


class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent
//
//  Dxui content panel that renders a DialogDefinition's body runs as a
//  vertical stack: normal runs become wrapped DxuiLabels, hyperlink runs
//  become DxuiButton(Link) controls whose click opens the URL. Lays itself
//  out in physical pixels (the hosted dialog passes a px content rect), so
//  the per-run heights scale with DPI. Used by the Dxui dialog path that
//  replaces the legacy DialogPrimitive for simple + hyperlink dialogs.
//
////////////////////////////////////////////////////////////////////////////////

class DialogBodyContent : public DxuiPanel
{
public:
    //  Where the body's picture goes: centered over the runs, or in the top
    //  right corner with the runs that start beside it kept clear of it.
    enum class ImagePlacement
    {
        CenteredAbove,
        TrailingBeside
    };

    void  SetImagePlacement (ImagePlacement placement) { m_placement = placement; }

    // Build one child widget per body run. Normal runs render with the
    // theme's Body text role (resolved at paint); hyperlink runs become
    // DxuiButton(Link) controls whose click opens the URL.
    void  SetRuns  (const std::vector<DialogTextRun> & runs);

    // Optional centered top icon (premultiplied BGRA), drawn above the
    // run stack at `displaySizeDip` square. Pass empty pixels for none.
    void  SetIcon  (std::vector<uint32_t> bgraPremul, int srcW, int srcH, int displaySizeDip);

    // A DialogImage in the same spot, converted from straight-alpha RGBA.
    // False when the image's size and its bytes disagree.
    bool  SetImage (const DialogImage & image);

    // Straight-alpha RGBA bytes to premultiplied BGRA pixels, the form the
    // icon bitmap draw takes. Empty when the byte count is not width x
    // height x 4.
    static std::vector<uint32_t>  ToPremultipliedBgra (const DialogImage & image);

    // Optional semantic icon glyph (Segoe MDL2 Assets codepoint) drawn in
    // a left column at `sizeDip` square, with the run stack inset to its
    // right. `argb` is the glyph color. Pass glyph 0 for none. Mutually
    // exclusive in practice with SetIcon (app-bitmap dialogs don't use a
    // semantic glyph).
    void  SetGlyphIcon (wchar_t glyph, uint32_t argb, int sizeDip);

    // Estimated stacked height in DIP, used by the caller to size the
    // hosting dialog.
    int   GetPreferredHeightDip () const;

    // Width in DIP the column rows need, or 0 when the body has none. Only
    // column rows report a width: prose wraps to whatever it is given, so a
    // body without them keeps the caller's default dialog width.
    int   GetPreferredWidthDip  () const;

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;


private:
    struct Picture
    {
        std::vector<uint32_t>  pixels;
        int                    srcW    = 0;
        int                    srcH    = 0;
        int                    sizeDip = 0;
        RECT                   rectPx  = {};
    };

    // One piece of a strip run: a picture, as an index into the run's
    // pictures, or a label, and the width it takes on the line.
    struct Piece
    {
        int             picture  = -1;
        IDxuiControl *  label    = nullptr;
        int             widthDip = 0;
    };

    struct Item
    {
        IDxuiControl *  widget = nullptr;
        int             lines  = 1;

        // A three-column row owns two more widgets: the arrow between the
        // cells and the right cell. Null on an ordinary prose run.
        IDxuiControl *  arrowWidget = nullptr;
        IDxuiControl *  rightWidget = nullptr;

        // A strip run's pieces, left to right, or a leading picture's square
        // (0 for none). Either way the pictures themselves are kept here for
        // Paint.
        std::vector<Piece>    strip;
        std::vector<Picture>  pictures;
        int                   leadingDip = 0;
    };

    // Builds a strip run's pieces, one label per text piece.
    void  BuildStripRow   (const DialogTextRun & run, Item & item);

    // Builds a run led by a picture. False, building nothing, when the run
    // has no leading picture with pixels.
    bool  BuildLeadingRow (const DialogTextRun & run, Item & item);

    // Converts a DialogImage to a picture. False when it has no pixels or no
    // size to be shown at.
    static bool  MakePicture       (const DialogImage & image, Picture & outPicture);

    // Lines a string takes wrapped at `wrapColumns` characters, estimated the
    // same way for every run.
    static int   EstimateLineCount (const std::wstring & text, size_t wrapColumns);

    // The height a run takes on its own: its lines, or its picture, with room
    // around it when it carries one.
    static int   RawItemHeightDip  (const Item & item);

    // The height a run takes in this body. Every run led by a picture takes
    // the tallest of their heights, so their pictures are evenly spaced
    // however long one row's text runs.
    int          GetItemHeightDip  (const Item & item) const;

    // Centers a strip run's pieces across the line, each picture and label
    // centered on the line's height.
    static void  LayoutStripRow    (Item & item, int topPx, int heightPx, int leftPx, int rightPx, const DxuiDpiScaler & scaler);

    // Width one string needs, estimated from an average glyph width because
    // layout runs without a text renderer -- the same trade DxuiButtonRow
    // makes for button labels. Alignment does not depend on the estimate
    // being right: every row is measured the same way and every arrow is
    // placed at the same column, so they line up whatever the estimate says.
    static int  EstimateTextWidthDip (const std::wstring & text);


    std::vector<Item>      m_items;
    ImagePlacement         m_placement     = ImagePlacement::CenteredAbove;
    int                    m_leadingRowDip = 0;   // shared by every run led by a picture
    std::vector<uint32_t>  m_iconPixels;
    int                    m_iconSrcW      = 0;
    int                    m_iconSrcH      = 0;
    int                    m_iconSizeDip   = 0;
    RECT                   m_iconRectPx    = {};
    // Shared column geometry for every column row in this body, so their
    // arrows form one vertical line. Zero when the body has no column rows.
    int                    m_leftColDip   = 0;
    int                    m_rightColDip  = 0;

    wchar_t                m_glyph        = 0;
    uint32_t               m_glyphArgb    = 0;
    int                    m_glyphSizeDip = 0;
    RECT                   m_glyphRectPx  = {};
};
