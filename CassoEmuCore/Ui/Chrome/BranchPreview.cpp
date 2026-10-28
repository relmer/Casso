#include "Pch.h"

#include "Ui/Chrome/BranchPreview.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview::SetPicture
//
////////////////////////////////////////////////////////////////////////////////

bool BranchPreview::SetPicture (Picture picture)
{
    bool  isChanged = picture != m_picture;



    m_picture = std::move (picture);

    SetVisible (m_picture != nullptr && m_picture->width > 0);

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview::Layout
//
//  The emulated screen's rectangle.
//
////////////////////////////////////////////////////////////////////////////////

void BranchPreview::Layout (
    const RECT           & boundsDip,
    const DxuiDpiScaler  & scaler)
{
    UNREFERENCED_PARAMETER (scaler);

    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BranchPreview::Paint (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme)
{
    RECT     bounds = GetBounds();
    HRESULT  hr     = S_OK;



    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (theme);

    if (m_picture == nullptr || m_picture->width <= 0)
    {
        return;
    }

    hr = text.DrawFramebuffer (m_picture->bgraPremul.data(), m_picture->width, m_picture->height,
                               (float) bounds.left, (float) bounds.top,
                               (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top));
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview::FitBeside
//
////////////////////////////////////////////////////////////////////////////////

RECT BranchPreview::FitBeside (
    const RECT  & area,
    const RECT  & flyout,
    float         widthOverHeight,
    int           gapPx)
{
    RECT  left  = { area.left + gapPx,     area.top + gapPx, flyout.left - gapPx, area.bottom - gapPx };
    RECT  right = { flyout.right + gapPx,  area.top + gapPx, area.right - gapPx,  area.bottom - gapPx };
    RECT  room  = (right.right - right.left >= left.right - left.left) ? right : left;
    RECT  whole = { area.left + gapPx,     area.top + gapPx, area.right - gapPx,  area.bottom - gapPx };



    if (room.right - room.left < (LONG) ((float) (area.right - area.left) * kMinSideShare))
    {
        room = whole;
    }

    return FitInside (room, widthOverHeight);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview::FitInside
//
//  The largest rectangle of the aspect in the room, centered across it and
//  level with its bottom.
//
////////////////////////////////////////////////////////////////////////////////

RECT BranchPreview::FitInside (
    const RECT  & room,
    float         widthOverHeight)
{
    LONG  roomW  = std::max<LONG> (room.right - room.left, 0);
    LONG  roomH  = std::max<LONG> (room.bottom - room.top, 0);
    LONG  width  = roomW;
    LONG  height = (widthOverHeight > 0.0f) ? (LONG) std::lround ((float) width / widthOverHeight) : roomH;
    RECT  fit    = {};



    if (height > roomH)
    {
        height = roomH;
        width  = (LONG) std::lround ((float) height * widthOverHeight);
    }

    fit.left   = room.left + (roomW - width) / 2;
    fit.right  = fit.left + width;
    fit.bottom = room.bottom;
    fit.top    = fit.bottom - height;

    return fit;
}