#include "Pch.h"

#include "DxuiTextViewPictures.h"
#include "DxuiTextPicture.h"
#include "DxuiTextRowLayout.h"
#include "IDxuiTextPictureHost.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::GetFrame
//
//  Where a row's picture is drawn at its normal size: the text's left, the
//  top of the row's first line, and the scale from the picture's pixels to
//  the view's. False for a row with no picture or none of whose lines are
//  laid out.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewPictures::GetFrame (int row, DxuiPictureMap::Frame & outFrame) const
{
    const DxuiTextPicture  * picture = m_host.GetRowPicture (row);
    int                      first   = -1;



    for (int i = 0; i < m_host.GetPictureLineCount() && first < 0; i++)
    {
        first = (m_host.GetPictureLineRow (i) == row) ? i : -1;
    }

    if (first < 0 || picture == nullptr)
    {
        return false;
    }

    outFrame.left  = m_host.GetPictureTextLeft();
    outFrame.top   = (float) m_host.GetPictureTextTop() + (float) ((first - m_host.GetPictureTopLine()) * m_host.GetPictureLineHeight());
    outFrame.scale = DxuiTextRowLayout::GetPictureScale (*picture, m_host.GetPictureTextWidth(), m_host.GetPictureScaler().ToPxf (1.0f));

    return outFrame.scale > 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::GetKey
//
//  What identifies a row's picture to the map: the row, and the picture's
//  size and ping point, so a different picture in the row starts unzoomed.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPictureMap::Key DxuiTextViewPictures::GetKey (int row) const
{
    const DxuiTextPicture  * picture = m_host.GetRowPicture (row);



    return (picture == nullptr || !picture->zoom) ? DxuiPictureMap::Key()
                                                  : DxuiPictureMap::Key { row, picture->width, picture->height, picture->pingX, picture->pingY };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::HitTest
//
//  The picture under a point, as its row and the pixel of the picture,
//  through any zoom; false off a picture.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewPictures::HitTest (POINT point, int & outRow, int & outX, int & outY) const
{
    int                      textTop = m_host.GetPictureTextTop();
    int                      height  = m_host.GetPictureLineHeight();
    int                      line    = (height > 0 && point.y >= textTop) ? m_host.GetPictureTopLine() + (point.y - textTop) / height : -1;
    int                      row     = 0;
    const DxuiTextPicture  * picture = nullptr;
    DxuiPictureMap::Frame    frame;
    DxuiPictureMap::Key      key;



    if (line < 0 || line >= m_host.GetPictureLineCount())
    {
        return false;
    }

    row     = m_host.GetPictureLineRow (line);
    picture = m_host.GetRowPicture (row);

    if (picture == nullptr || !GetFrame (row, frame))
    {
        return false;
    }

    key    = GetKey (row);
    outRow = row;
    outX   = (int) (((float) point.x - frame.left) / (frame.scale * m_map.GetZoom (key)) + m_map.GetOriginX (key));
    outY   = (int) (((float) point.y - frame.top)  / (frame.scale * m_map.GetZoom (key)) + m_map.GetOriginY (key));

    return (float) point.x >= frame.left && (float) point.y >= frame.top
        && (float) point.x < frame.left + (float) picture->width * frame.scale && (float) point.y < frame.top + (float) picture->height * frame.scale
        && outX >= 0 && outX < picture->width && outY >= 0 && outY < picture->height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::SetOutline
//
//  A rectangle of a row's picture, in its pixels, outlined in the accent
//  color, as the cell under the pointer is; row -1 for none. Returns
//  whether the outline moved.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewPictures::SetOutline (int row, const RECT & pictureRect)
{
    bool  moved = row != m_outlineRow || (row >= 0 && !EqualRect (&pictureRect, &m_outlineRect));



    m_outlineRow  = row;
    m_outlineRect = pictureRect;

    return moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::Paint
//
//  A zoomed picture is drawn larger, shifted by its origin, and clipped to
//  the place it has at its normal size. The outline is drawn over the
//  picture, around the cell; a cell smaller than the minimum is outlined a
//  cell's width out from its edge, so the outline doesn't cover it. The
//  ping's rings are drawn last.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPictures::Paint (IDxuiTextRenderer & text, const IDxuiTheme & theme, int row) const
{
    const DxuiTextPicture  * picture = m_host.GetRowPicture (row);
    DxuiPictureMap::Frame    frame;
    DxuiPictureMap::Key      key     = GetKey (row);
    float                    zoom    = m_map.GetZoom    (key);
    float                    originX = m_map.GetOriginX (key);
    float                    originY = m_map.GetOriginY (key);
    float                    stroke  = m_host.GetPictureScaler().ToPxf (2.0f);
    float                    shown   = 0.0f;
    HRESULT                  hr      = S_OK;



    if (picture == nullptr || !GetFrame (row, frame))
    {
        return;
    }

    shown = frame.scale * zoom;

    hr = text.PushClipRect (frame.left, frame.top, (float) picture->width * frame.scale, (float) picture->height * frame.scale);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (picture->width > 0 && picture->height > 0 && picture->bgra.size() >= (size_t) picture->width * (size_t) picture->height)
    {
        hr = text.DrawFramebuffer (picture->bgra.data(), picture->width, picture->height,
                                   frame.left - originX * shown, frame.top - originY * shown,
                                   (float) picture->width * shown, (float) picture->height * shown);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (row == m_outlineRow)
    {
        float  minimum = m_host.GetPictureScaler().ToPxf ((float) kOutlineMinCellDip);
        float  w       = (float) (m_outlineRect.right - m_outlineRect.left) * shown;
        float  h       = (float) (m_outlineRect.bottom - m_outlineRect.top) * shown;
        float  margin  = (w < minimum) ? w : 0.0f;
        float  x       = frame.left + ((float) m_outlineRect.left - originX) * shown - margin - stroke;
        float  y       = frame.top  + ((float) m_outlineRect.top  - originY) * shown - margin - stroke;

        w += 2.0f * (margin + stroke);
        h += 2.0f * (margin + stroke);

        for (const auto & edge : { std::array<float, 4> { x, y, w, stroke },          std::array<float, 4> { x, y + h - stroke, w, stroke },
                                   std::array<float, 4> { x, y, stroke, h },          std::array<float, 4> { x + w - stroke, y, stroke, h } })
        {
            hr = text.FillRect (edge[0], edge[1], edge[2], edge[3], theme.Accent());
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    m_map.PaintPing (text, theme, m_host.GetPictureScaler(), picture->pingX, picture->pingY, frame.left - originX * shown, frame.top - originY * shown, shown);

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::IsPinging
//
//  Whether a picture with a ping is in the rows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewPictures::IsPinging() const
{
    for (int row = 0; row < m_host.GetPictureRowCount(); row++)
    {
        const DxuiTextPicture  * picture = m_host.GetRowPicture (row);

        if (picture != nullptr && picture->pingX >= 0 && picture->pingY >= 0)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::TickPing
//
//  The ping's animation: true when a frame changed, so the view needs
//  painting.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewPictures::TickPing (int64_t nowMs)
{
    return m_map.TickPing (nowMs, IsPinging());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::Zoom
//
//  The wheel over a zoomable picture; false off one, so the view scrolls
//  there.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewPictures::Zoom (POINT point, float notches)
{
    int                    row   = 0;
    int                    x     = 0;
    int                    y     = 0;
    DxuiPictureMap::Frame  frame;



    if (!HitTest (point, row, x, y) || !m_host.GetRowPicture (row)->zoom || !GetFrame (row, frame))
    {
        return false;
    }

    m_map.Zoom (GetKey (row), frame, point, notches);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::TryStartPan
//
//  A left press on a zoomable picture starts panning it; false off one, so
//  the view selects text there.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewPictures::TryStartPan (POINT point)
{
    int  row = 0;
    int  x   = 0;
    int  y   = 0;



    if (!HitTest (point, row, x, y) || !m_host.GetRowPicture (row)->zoom)
    {
        return false;
    }

    m_map.StartPan (GetKey (row), point);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::Pan
//
//  The panned picture follows the pointer, in its frame on the view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPictures::Pan (POINT point)
{
    DxuiPictureMap::Frame  frame;



    for (int row = 0; row < m_host.GetPictureRowCount(); row++)
    {
        if (m_map.IsFor (GetKey (row)) && GetFrame (row, frame))
        {
            m_map.Pan (frame, point);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPictures::GetLineCount
//
//  How many of the view's lines a row's picture takes at a text width: as
//  many as it is tall at the scale it fits, at least one.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextViewPictures::GetLineCount (int row, float widthPx, int lineHeightPx) const
{
    const DxuiTextPicture  * picture = m_host.GetRowPicture (row);



    return (picture == nullptr) ? 0 : DxuiTextRowLayout::GetPictureLines (*picture, widthPx, m_host.GetPictureScaler().ToPxf (1.0f), lineHeightPx);
}