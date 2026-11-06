#include "Pch.h"

#include "DxuiStripMarks.h"
#include "DxuiBranchArt.h"
#include "DxuiImageStrip.h"
#include "Theme/DxuiTheme.h"
#include "Window/DxuiHwndSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripMarks::Paint
//
//  Each of the source's marks: a thin line across the pictures at its
//  offset, and its glyph or branch in a badge on the pictures' bottom edge,
//  with the count beside it when it is above 1. Marks closer together than a
//  badge's width share one badge (Join). Drawn as pictures are, since
//  pictures draw over filled rectangles. The badges are saved for the tooltip's hit
//  test.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripMarks::Paint (
    IDxuiImageStripSource  & source,
    const DxuiImageStrip   & strip,
    const DxuiDpiScaler    & scaler,
    IDxuiTextRenderer      & text,
    const IDxuiTheme       & theme)
{
    constexpr uint32_t  kOpaque    = 0xFF000000u;
    constexpr uint32_t  kColorMask = 0x00FFFFFFu;
    constexpr uint32_t  kBadgeArgb = 0xE0000000u;
    constexpr float     kCountWide = 2.0f;
    HRESULT             hr         = S_OK;
    RECT                rc         = strip.GetPicturesRect();
    float               thick      = std::max (1.0f, (float) std::lround (scaler.ToPxf (kLineDip)));
    float               badge      = scaler.ToPxf (kBadgeDip);
    float               size       = scaler.ToPxf (kGlyphDip);
    float               top        = (float) rc.bottom - badge;
    float               along      = 0.0f;
    float               width      = 0.0f;
    uint32_t            ink        = theme.Foreground() | kOpaque;
    uint32_t            back       = (theme.Background() & kColorMask) | kBadgeArgb;
    std::wstring        label;
    float               labelLeft  = 0.0f;
    float               labelWidth = 0.0f;
    Badge               placed;



    m_badges.clear();

    source.GetMarks (m_marks);

    Join (strip, badge);

    for (const DxuiStripMark & mark : m_marks)
    {
        along = strip.GetLineAlong (mark.offset);

        label = (mark.art == DxuiStripMarkArt::Glyph) ? std::wstring (1, mark.glyph) : std::wstring();

        if (mark.count > 1)
        {
            label += std::to_wstring (mark.count);
        }

        width = (mark.count > 1) ? badge * kCountWide : badge;

        hr = text.DrawIconBitmap (&ink, 1, 1, along - thick / 2.0f, (float) rc.top, thick, (float) (rc.bottom - rc.top));
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawIconBitmap (&back, 1, 1, along - width / 2.0f, top, width, badge);
        IGNORE_RETURN_VALUE (hr, S_OK);

        labelLeft  = along - width / 2.0f;
        labelWidth = width;

        //  A branch takes the badge's first square, and the count the rest.
        if (mark.art == DxuiStripMarkArt::Branch)
        {
            PaintBranchArt (text, labelLeft + (badge - size) / 2.0f, top + (badge - size) / 2.0f, size, ink);

            labelLeft  += badge;
            labelWidth -= badge;
        }

        if (!label.empty())
        {
            hr = text.DrawString (label.c_str(), labelLeft, top, labelWidth, badge, ink, size, kGlyphFace,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        placed.rect    = RECT { (LONG) std::lround (along - width / 2.0f), (LONG) std::lround (top), (LONG) std::lround (along + width / 2.0f), rc.bottom };
        placed.along   = along;
        placed.tip     = mark.tip;
        placed.id      = mark.id;
        placed.hasRows = mark.hasRows;

        m_badges.push_back (placed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripMarks::Join
//
//  A mark closer to the one before than a badge's width joins that mark's
//  badge, whose tip lists both. A mark with rows takes the badge's art, id
//  and rows from a mark without, since only it opens a flyout: a fork made
//  by a reset is joined by the reset's mark, one keyframe after it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripMarks::Join (
    const DxuiImageStrip  & strip,
    float                   badgePx)
{
    std::vector<DxuiStripMark>  joined;
    float                       lastAlong = 0.0f;
    float                       along     = 0.0f;



    for (const DxuiStripMark & mark : m_marks)
    {
        along = strip.GetLineAlong (mark.offset);

        if (!joined.empty() && along - lastAlong < badgePx)
        {
            DxuiStripMark  & first = joined.back();
            std::wstring     tip   = first.tip + L"\n" + mark.tip;
            float            at    = first.offset;



            if (mark.hasRows && !first.hasRows)
            {
                first        = mark;
                first.offset = at;
            }

            first.tip = std::move (tip);
            continue;
        }

        joined.push_back (mark);
        lastAlong = along;
    }

    m_marks.swap (joined);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripMarks::PaintBranchArt
//
//  A branch mark's art, square at sizePx, drawn again only when its size or
//  ink changed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripMarks::PaintBranchArt (
    IDxuiTextRenderer  & text,
    float                x,
    float                y,
    float                sizePx,
    uint32_t             ink)
{
    HRESULT  hr     = S_OK;
    int      sizeAt = std::max ((int) std::lround (sizePx), 1);



    if (sizeAt != m_branchArtSize || ink != m_branchArtInk)
    {
        DxuiBranchArt::Draw (sizeAt, ink, m_branchArt);

        m_branchArtSize = sizeAt;
        m_branchArtInk  = ink;
    }

    hr = text.DrawIconBitmap (m_branchArt.data(), sizeAt, sizeAt, x, y, sizePx, sizePx);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripMarks::Track
//
//  The pointer over a mark with rows, or not, for the flyout, with the
//  mark's badge on screen as where it opens.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripMarks::Track (
    IDxuiImageStripSource  * source,
    DxuiHwndSource         * popupHost,
    int                      x,
    int                      y)
{
    size_t  badge    = 0;
    bool    isOver   = TryGetBadgeAt (x, y, badge) && m_badges[badge].hasRows;
    RECT    anchor   = isOver ? m_badges[badge].rect : RECT {};
    POINT   topLeft  = { anchor.left,  anchor.top };
    POINT   botRight = { anchor.right, anchor.bottom };
    HWND    owner    = (popupHost != nullptr) ? popupHost->GetHwnd() : nullptr;



    if (isOver && owner != nullptr)
    {
        ClientToScreen (owner, &topLeft);
        ClientToScreen (owner, &botRight);
    }

    m_flyout.Track (source, isOver, isOver ? m_badges[badge].id : 0, RECT { topLeft.x, topLeft.y, botRight.x, botRight.y }, (int64_t) GetTickCount64());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripMarks::Untrack
//
//  The pointer left the strip: the flyout tracks it as off every mark.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripMarks::Untrack (IDxuiImageStripSource * source)
{
    m_flyout.Track (source, false, 0, RECT {}, (int64_t) GetTickCount64());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripMarks::TryGetBadgeAt
//
//  The badge the last paint placed under a point, if any.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiStripMarks::TryGetBadgeAt (
    int       x,
    int       y,
    size_t  & outIndex) const
{
    POINT   pt = { x, y };
    size_t  i  = 0;



    for (i = 0; i < m_badges.size(); i++)
    {
        if (PtInRect (&m_badges[i].rect, pt))
        {
            outIndex = i;
            return true;
        }
    }

    return false;
}
