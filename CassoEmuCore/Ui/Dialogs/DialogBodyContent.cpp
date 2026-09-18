#include "Pch.h"

#include "DialogBodyContent.h"

#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiButton.h"
#include "Core/UnicodeSymbols.h"


static constexpr int      s_kLineHeightDip = 18;   // == DxuiTheme::BodyLineHeightDip, so a run's block matches real text line flow
static constexpr int      s_kItemGapDip    = 0;    // runs flow like continuous text lines; use an empty run for a one-line gap
static constexpr int      s_kIconGapDip    = 12;
static constexpr int      s_kGlyphGapDip   = 12;
static constexpr size_t   s_kWrapColumns   = 52;
static constexpr int      s_kShellExecOk   = 0;     // ignored ShellExecute result reset value
static constexpr wchar_t  s_kMdl2Family[]  = L"Segoe MDL2 Assets";
static constexpr int      s_kColGapDip     = 10;   // each side of the arrow column
static constexpr int      s_kArrowColDip   = 16;   // the arrow glyph's own column
static constexpr int      s_kStripGapDip   = 8;    // between the pieces of a strip run
static constexpr int      s_kLeadGapDip    = 8;    // between a leading picture and its text
static constexpr int      s_kPicturePadDip = 3;    // above and below a run that carries a picture





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::SetRuns
//
//  Builds one child widget per body run -- a wrapped DxuiLabel for normal
//  text, a DxuiButton(Link) for a hyperlink -- and records an estimated
//  line count per run (used for vertical sizing).
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::SetRuns (const std::vector<DialogTextRun> & runs)
{
    m_items.clear();
    m_items.reserve (runs.size());

    m_leftColDip    = 0;
    m_rightColDip   = 0;
    m_leadingRowDip = 0;

    for (const DialogTextRun & run : runs)
    {
        Item  item;


        item.lines = EstimateLineCount (run.text, s_kWrapColumns);

        if (run.IsColumnRow())
        {
            // Three cells so each can be positioned independently in Layout;
            // one label with padding could not keep a column across rows.
            DxuiLabel  &  left  = Add<DxuiLabel>();
            DxuiLabel  &  arrow = Add<DxuiLabel>();
            DxuiLabel  &  right = Add<DxuiLabel>();

            left.SetText       (run.text);
            left.SetTextRole   (DxuiTextRole::Body);
            left.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

            arrow.SetText      (s_kpszRightArrow);
            arrow.SetTextRole  (DxuiTextRole::Body);
            arrow.SetTextAlign (DxuiTextHAlign::Center, DxuiTextVAlign::Top);

            right.SetText      (run.rightText);
            right.SetTextRole  (DxuiTextRole::Body);
            right.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

            item.widget      = &left;
            item.arrowWidget = &arrow;
            item.rightWidget = &right;
            item.lines       = 1;   // a column row never wraps

            m_leftColDip  = (std::max) (m_leftColDip,  EstimateTextWidthDip (run.text));
            m_rightColDip = (std::max) (m_rightColDip, EstimateTextWidthDip (run.rightText));
        }
        else if (run.isHyperlink)
        {
            DxuiButton  &  link = Add<DxuiButton>();
            std::wstring   url  = run.hyperlinkUrl;

            link.SetLabel   (run.text);
            link.SetVariant (DxuiButton::Variant::Link);
            link.SetOnClick   ([url] ()
                             {
                                 INT_PTR  rc = (INT_PTR) ShellExecuteW (nullptr, L"open", url.c_str(),
                                                                        nullptr, nullptr, SW_SHOWNORMAL);

                                 IGNORE_RETURN_VALUE (rc, s_kShellExecOk);
                             });
            item.widget = &link;
        }
        else if (!run.strip.empty())
        {
            BuildStripRow (run, item);
        }
        else if (!BuildLeadingRow (run, item))
        {
            DxuiLabel  &  label = Add<DxuiLabel>();

            label.SetText      (run.text);
            label.SetTextRole  (DxuiTextRole::Body);
            label.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
            item.widget = &label;
        }

        m_items.push_back (item);
    }

    //  Rows led by a picture take one height between them, so their pictures
    //  are evenly spaced whether their text runs to one line or three.
    for (const Item & item : m_items)
    {
        if (item.leadingDip > 0)
        {
            m_leadingRowDip = (std::max) (m_leadingRowDip, RawItemHeightDip (item));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::SetIcon
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::SetIcon (std::vector<uint32_t> bgraPremul, int srcW, int srcH, int displaySizeDip)
{
    m_iconPixels  = std::move (bgraPremul);
    m_iconSrcW    = srcW;
    m_iconSrcH    = srcH;
    m_iconSizeDip = displaySizeDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::ToPremultipliedBgra
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> DialogBodyContent::ToPremultipliedBgra (const DialogImage & image)
{
    constexpr size_t    kChannels = 4;
    constexpr uint32_t  kOpaque   = 255;



    std::vector<uint32_t>  pixels;
    size_t                 count = 0;
    size_t                 i     = 0;



    if (image.width <= 0 || image.height <= 0)
    {
        return pixels;
    }

    count = (size_t) image.width * (size_t) image.height;

    if (image.rgba.size() != count * kChannels)
    {
        return pixels;
    }

    pixels.resize (count);

    for (i = 0; i < count; i++)
    {
        uint32_t  r = image.rgba[i * kChannels];
        uint32_t  g = image.rgba[i * kChannels + 1];
        uint32_t  b = image.rgba[i * kChannels + 2];
        uint32_t  a = image.rgba[i * kChannels + 3];

        r = (r * a + kOpaque / 2) / kOpaque;
        g = (g * a + kOpaque / 2) / kOpaque;
        b = (b * a + kOpaque / 2) / kOpaque;

        pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }

    return pixels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::SetImage
//
////////////////////////////////////////////////////////////////////////////////

bool DialogBodyContent::SetImage (const DialogImage & image)
{
    std::vector<uint32_t>  pixels = ToPremultipliedBgra (image);



    if (pixels.empty())
    {
        return false;
    }

    SetIcon (std::move (pixels), image.width, image.height, (int) image.displayDp);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::SetGlyphIcon
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::SetGlyphIcon (wchar_t glyph, uint32_t argb, int sizeDip)
{
    m_glyph        = glyph;
    m_glyphArgb    = argb;
    m_glyphSizeDip = sizeDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::GetPreferredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::GetPreferredHeightDip() const
{
    int  iconTop  = 0;
    int  besides  = 0;
    int  runs     = 0;



    if (!m_iconPixels.empty() && m_iconSizeDip > 0)
    {
        if (m_placement == ImagePlacement::TrailingBeside)
        {
            besides = m_iconSizeDip;
        }
        else
        {
            iconTop = m_iconSizeDip + s_kIconGapDip;
        }
    }

    for (const Item & item : m_items)
    {
        runs += GetItemHeightDip (item) + s_kItemGapDip;
    }

    if (m_glyph != 0 && m_glyphSizeDip > runs)
    {
        runs = m_glyphSizeDip;
    }

    return iconTop + (std::max) (runs, besides);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::EstimateTextWidthDip
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::EstimateTextWidthDip (const std::wstring & text)
{
    constexpr float  kFontDip    = 13.0f;
    constexpr float  kEstGlyphEm = 0.58f;



    return static_cast<int> (text.size() * kFontDip * kEstGlyphEm);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::GetPreferredWidthDip
//
//  The width the column rows want: both cells at their widest, the arrow
//  column, and the gaps around it. Zero when there are no column rows, which
//  the caller reads as "no opinion" and keeps its own default width.
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::GetPreferredWidthDip() const
{
    int  width = 0;



    if (m_leftColDip > 0 || m_rightColDip > 0)
    {
        width = m_leftColDip + s_kColGapDip + s_kArrowColDip + s_kColGapDip + m_rightColDip;

        if (m_glyph != 0 && m_glyphSizeDip > 0)
        {
            width += m_glyphSizeDip + s_kGlyphGapDip;
        }
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::Layout
//
//  Stacks the run widgets top-down within the (physical-pixel) content
//  rect, each sized to its estimated line count.
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  linePx   = scaler.ToPx (s_kLineHeightDip);
    int  gapPx    = scaler.ToPx (s_kItemGapDip);
    int  y        = boundsPx.top;
    int  runsLeft = boundsPx.left;



    SetBounds (boundsPx);

    m_iconRectPx  = {};
    m_glyphRectPx = {};

    if (!m_iconPixels.empty() && m_iconSizeDip > 0)
    {
        int  iconPx = scaler.ToPx (m_iconSizeDip);
        int  cx     = (boundsPx.left + boundsPx.right) / 2;

        if (m_placement == ImagePlacement::TrailingBeside)
        {
            m_iconRectPx.left   = boundsPx.right - iconPx;
            m_iconRectPx.top    = y;
            m_iconRectPx.right  = boundsPx.right;
            m_iconRectPx.bottom = y + iconPx;
        }
        else
        {
            m_iconRectPx.left   = cx - iconPx / 2;
            m_iconRectPx.top    = y;
            m_iconRectPx.right  = cx + iconPx / 2;
            m_iconRectPx.bottom = y + iconPx;
            y += iconPx + scaler.ToPx (s_kIconGapDip);
        }
    }

    if (m_glyph != 0 && m_glyphSizeDip > 0)
    {
        int  glyphPx = scaler.ToPx (m_glyphSizeDip);

        m_glyphRectPx.left   = boundsPx.left;
        m_glyphRectPx.top    = y;
        m_glyphRectPx.right  = boundsPx.left + glyphPx;
        m_glyphRectPx.bottom = y + glyphPx;
        runsLeft = boundsPx.left + glyphPx + scaler.ToPx (s_kGlyphGapDip);
    }

    for (Item & item : m_items)
    {
        int   hPx     = item.pictures.empty() ? item.lines * linePx : scaler.ToPx (GetItemHeightDip (item));
        int   rightPx = boundsPx.right;
        RECT  b       = {};


        //  A run that starts level with a trailing picture stops short of it.
        if (m_placement == ImagePlacement::TrailingBeside && y < m_iconRectPx.bottom)
        {
            rightPx = m_iconRectPx.left - scaler.ToPx (s_kIconGapDip);
        }

        b = { runsLeft, y, rightPx, y + hPx };

        if (item.arrowWidget != nullptr)
        {
            // Column positions come from the shared widths, so every arrow
            // lands on the same x whatever its row says.
            int  leftPx  = scaler.ToPx (m_leftColDip);
            int  colGap  = scaler.ToPx (s_kColGapDip);
            int  arrowPx = scaler.ToPx (s_kArrowColDip);
            int  arrowX  = runsLeft + leftPx + colGap;
            int  rightX  = arrowX + arrowPx + colGap;

            RECT  leftBox  = { runsLeft, y, runsLeft + leftPx,  y + hPx };
            RECT  arrowBox = { arrowX,   y, arrowX + arrowPx,   y + hPx };
            RECT  rightBox = { rightX,   y, rightPx,            y + hPx };

            item.widget->Layout      (leftBox,  scaler);
            item.arrowWidget->Layout (arrowBox, scaler);
            item.rightWidget->Layout (rightBox, scaler);
        }
        else if (!item.strip.empty())
        {
            LayoutStripRow (item, y, hPx, runsLeft, rightPx, scaler);
        }
        else if (item.leadingDip > 0 && item.widget != nullptr)
        {
            // Both take the whole row: the picture centered in it, the label
            // centered in it by its own vertical alignment, so the text reads
            // level with the picture whatever it wraps to.
            int   picturePx  = scaler.ToPx (item.leadingDip);
            int   pictureTop = y + (std::max) (0, (hPx - picturePx) / 2);
            int   textLeft   = runsLeft + picturePx + scaler.ToPx (s_kLeadGapDip);
            RECT  textBox    = { textLeft, y, rightPx, y + hPx };

            item.pictures[0].rectPx = { runsLeft, pictureTop, runsLeft + picturePx, pictureTop + picturePx };
            item.widget->Layout (textBox, scaler);
        }
        else if (item.widget != nullptr)
        {
            item.widget->Layout (b, scaler);
        }

        y += hPx + gapPx;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::Paint
//
//  Draws the optional top icon (premultiplied BGRA bitmap), then fans the
//  paint out to the run widgets via the base panel.
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (!m_iconPixels.empty() && m_iconRectPx.right > m_iconRectPx.left)
    {
        HRESULT  hr = text.DrawIconBitmap (m_iconPixels.data(),
                                           m_iconSrcW,
                                           m_iconSrcH,
                                           (float) m_iconRectPx.left,
                                           (float) m_iconRectPx.top,
                                           (float) (m_iconRectPx.right  - m_iconRectPx.left),
                                           (float) (m_iconRectPx.bottom - m_iconRectPx.top));

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_glyph != 0 && m_glyphRectPx.right > m_glyphRectPx.left)
    {
        wchar_t  glyphStr[2] = { m_glyph, L'\0' };
        float    sizePx      = (float) (m_glyphRectPx.bottom - m_glyphRectPx.top);
        HRESULT  hr          = text.DrawString (glyphStr,
                                                (float) m_glyphRectPx.left,
                                                (float) m_glyphRectPx.top,
                                                (float) (m_glyphRectPx.right  - m_glyphRectPx.left),
                                                (float) (m_glyphRectPx.bottom - m_glyphRectPx.top),
                                                m_glyphArgb,
                                                sizePx,
                                                s_kMdl2Family,
                                                DxuiTextHAlign::Center,
                                                DxuiTextVAlign::Center,
                                                DxuiFontWeight::Normal);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (const Item & item : m_items)
    {
        for (const Picture & picture : item.pictures)
        {
            if (picture.rectPx.right > picture.rectPx.left)
            {
                HRESULT  hr = text.DrawIconBitmap (picture.pixels.data(),
                                                   picture.srcW,
                                                   picture.srcH,
                                                   (float) picture.rectPx.left,
                                                   (float) picture.rectPx.top,
                                                   (float) (picture.rectPx.right  - picture.rectPx.left),
                                                   (float) (picture.rectPx.bottom - picture.rectPx.top));

                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }
    }

    DxuiPanel::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::BuildStripRow
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::BuildStripRow (const DialogTextRun & run, Item & item)
{
    for (const DialogInlinePiece & source : run.strip)
    {
        Piece        piece;
        Picture      picture;
        DxuiLabel  * label = nullptr;


        if (MakePicture (source.image, picture))
        {
            piece.picture  = (int) item.pictures.size();
            piece.widthDip = picture.sizeDip;
            item.pictures.push_back (std::move (picture));
        }
        else if (!source.text.empty())
        {
            label = &Add<DxuiLabel>();

            label->SetText      (source.text);
            label->SetTextRole  (DxuiTextRole::Body);
            label->SetTextAlign (DxuiTextHAlign::Center, DxuiTextVAlign::Top);

            piece.label    = label;
            piece.widthDip = (std::max) (EstimateTextWidthDip (source.text), s_kLineHeightDip);
        }
        else
        {
            continue;
        }

        item.strip.push_back (piece);
    }

    item.lines = 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::BuildLeadingRow
//
//  Wrapping is estimated in characters, so the picture's width comes off the
//  line as the characters it would have held.
//
////////////////////////////////////////////////////////////////////////////////

bool DialogBodyContent::BuildLeadingRow (const DialogTextRun & run, Item & item)
{
    Picture      picture;
    DxuiLabel  * label   = nullptr;
    size_t       indent  = 0;
    size_t       columns = s_kWrapColumns;



    if (!run.leadingImage.has_value() || !MakePicture (*run.leadingImage, picture))
    {
        return false;
    }

    indent  = (size_t) ((picture.sizeDip + s_kLeadGapDip) / (std::max) (1, EstimateTextWidthDip (L"x")));
    columns = (indent < s_kWrapColumns) ? s_kWrapColumns - indent : 1;

    label = &Add<DxuiLabel>();

    label->SetText      (run.text);
    label->SetTextRole  (DxuiTextRole::Body);
    label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    item.widget     = label;
    item.lines      = EstimateLineCount (run.text, columns);
    item.leadingDip = picture.sizeDip;
    item.pictures.push_back (std::move (picture));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::MakePicture
//
////////////////////////////////////////////////////////////////////////////////

bool DialogBodyContent::MakePicture (const DialogImage & image, Picture & outPicture)
{
    outPicture.pixels = ToPremultipliedBgra (image);

    if (outPicture.pixels.empty() || image.displayDp <= 0.0f)
    {
        return false;
    }

    outPicture.srcW    = image.width;
    outPicture.srcH    = image.height;
    outPicture.sizeDip = (int) image.displayDp;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::EstimateLineCount
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::EstimateLineCount (const std::wstring & text, size_t wrapColumns)
{
    int     lines   = 1;
    size_t  lineLen = 0;



    for (wchar_t ch : text)
    {
        if (ch == L'\n')
        {
            lines++;
            lineLen = 0;
        }
        else
        {
            lineLen++;

            if (lineLen >= wrapColumns)
            {
                lines++;
                lineLen = 0;
            }
        }
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::GetItemHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::RawItemHeightDip (const Item & item)
{
    int  height = item.lines * s_kLineHeightDip;



    for (const Picture & picture : item.pictures)
    {
        height = (std::max) (height, picture.sizeDip);
    }

    if (!item.pictures.empty())
    {
        height += 2 * s_kPicturePadDip;
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::GetItemHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::GetItemHeightDip (const Item & item) const
{
    if (item.leadingDip > 0 && m_leadingRowDip > 0)
    {
        return m_leadingRowDip;
    }

    return RawItemHeightDip (item);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::LayoutStripRow
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::LayoutStripRow (Item & item, int topPx, int heightPx, int leftPx, int rightPx, const DxuiDpiScaler & scaler)
{
    int  gapPx   = scaler.ToPx (s_kStripGapDip);
    int  linePx  = scaler.ToPx (s_kLineHeightDip);
    int  totalPx = 0;
    int  x       = 0;



    if (item.strip.empty())
    {
        return;
    }

    for (const Piece & piece : item.strip)
    {
        totalPx += scaler.ToPx (piece.widthDip);
    }

    totalPx += gapPx * (int) (item.strip.size() - 1);
    x        = leftPx + (std::max) (0, (rightPx - leftPx - totalPx) / 2);

    for (const Piece & piece : item.strip)
    {
        int  widthPx = scaler.ToPx (piece.widthDip);


        if (piece.picture >= 0)
        {
            int  top = topPx + (heightPx - widthPx) / 2;

            item.pictures[(size_t) piece.picture].rectPx = { x, top, x + widthPx, top + widthPx };
        }
        else if (piece.label != nullptr)
        {
            int   top = topPx + (heightPx - linePx) / 2;
            RECT  box = { x, top, x + widthPx, top + linePx };

            piece.label->Layout (box, scaler);
        }

        x += widthPx + gapPx;
    }
}
