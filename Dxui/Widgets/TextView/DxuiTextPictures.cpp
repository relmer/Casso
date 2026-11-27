#include "Pch.h"

#include "Widgets/TextView/DxuiTextPictures.h"
#include "Widgets/TextView/DxuiTextPicture.h"
#include "Widgets/TextView/DxuiTextLayout.h"
#include "Widgets/TextView/DxuiTextViewInterfaces.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::GetFrame
//
//  Where a row's picture is drawn at its normal size: the text's left, the
//  top of the row's first line, and the scale from the picture's pixels to
//  the view's. False for a row with no picture or none of whose lines are
//  laid out.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextPictures::GetFrame (int row, DxuiPictureMap::Frame & outFrame) const
{
    const DxuiTextPicture  * picture = GetPicture (row);
    int                      first   = -1;



    for (int i = 0; i < m_layout.GetLineCount() && first < 0; i++)
    {
        first = (m_layout.GetLineRow (i) == row) ? i : -1;
    }

    if (first < 0 || picture == nullptr)
    {
        return false;
    }

    outFrame.left  = GetTextLeft();
    outFrame.top   = (float) (int) m_geometry.GetTextOrigin().y + (float) ((first - m_viewport.GetTopLine()) * m_layout.GetCellHeightPx());
    outFrame.scale = GetPictureScale (*picture, GetTextWidth(), m_geometry.GetScaler().ToPxf (1.0f));

    return outFrame.scale > 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::GetKey
//
//  What identifies a row's picture to the map: the row, and the picture's
//  size and ping point, so a different picture in the row starts unzoomed.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPictureMap::Key DxuiTextPictures::GetKey (int row) const
{
    const DxuiTextPicture  * picture = GetPicture (row);



    return (picture == nullptr || !picture->zoom) ? DxuiPictureMap::Key()
                                                  : DxuiPictureMap::Key { row, picture->width, picture->height, picture->pingX, picture->pingY };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::HitTest
//
//  The picture under a point, as its row and the pixel of the picture,
//  through any zoom; false off a picture.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextPictures::HitTest (POINT point, int & outRow, int & outX, int & outY) const
{
    int                      textTop = (int) m_geometry.GetTextOrigin().y;
    int                      height  = m_layout.GetCellHeightPx();
    int                      line    = (height > 0 && point.y >= textTop) ? m_viewport.GetTopLine() + (point.y - textTop) / height : -1;
    int                      row     = 0;
    const DxuiTextPicture  * picture = nullptr;
    DxuiPictureMap::Frame    frame;
    DxuiPictureMap::Key      key;



    if (line < 0 || line >= m_layout.GetLineCount())
    {
        return false;
    }

    row     = m_layout.GetLineRow (line);
    picture = GetPicture (row);

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
//  DxuiTextPictures::IsOverMap
//
//  Whether a point is on a picture that Ctrl with the wheel zooms and a
//  drag pans.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextPictures::IsOverMap (POINT point) const
{
    int  row = 0;
    int  x   = 0;
    int  y   = 0;



    return HitTest (point, row, x, y) && GetPicture (row)->zoom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::SetOutline
//
//  A rectangle of a row's picture, in its pixels, outlined in the accent
//  color, as the cell under the pointer is; row -1 for none. Returns
//  whether the outline moved.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextPictures::SetOutline (int row, const RECT & pictureRect)
{
    bool  moved = row != m_outlineRow || (row >= 0 && !EqualRect (&pictureRect, &m_outlineRect));



    m_outlineRow  = row;
    m_outlineRect = pictureRect;

    return moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::Paint
//
//  A zoomed picture is drawn larger, shifted by its origin, and clipped to
//  the place it has at its normal size. The outline is drawn over the
//  picture, around the cell; a cell smaller than the minimum is outlined a
//  cell's width out from its edge, so the outline doesn't cover it. The
//  ping's rings are drawn last.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextPictures::Paint (IDxuiTextRenderer & text, const IDxuiTheme & theme, int row) const
{
    const DxuiTextPicture  * picture    = GetPicture (row);
    DxuiPictureMap::Frame    frame;
    DxuiPictureMap::Key      key        = GetKey (row);
    float                    zoom       = m_map.GetZoom    (key);
    float                    originX    = m_map.GetOriginX (key);
    float                    originY    = m_map.GetOriginY (key);
    float                    stroke     = m_geometry.GetScaler().ToPxf (2.0f);
    float                    drawnScale = 0.0f;
    HRESULT                  hr         = S_OK;



    if (picture == nullptr || !GetFrame (row, frame))
    {
        return;
    }

    drawnScale = frame.scale * zoom;

    hr = text.PushClipRect (frame.left, frame.top, (float) picture->width * frame.scale, (float) picture->height * frame.scale);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (picture->width > 0 && picture->height > 0 && picture->bgra.size() >= (size_t) picture->width * (size_t) picture->height)
    {
        hr = text.DrawFramebuffer (picture->bgra.data(), picture->width, picture->height,
                                   frame.left - originX * drawnScale, frame.top - originY * drawnScale,
                                   (float) picture->width * drawnScale, (float) picture->height * drawnScale);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (row == m_outlineRow)
    {
        float  minimum = m_geometry.GetScaler().ToPxf ((float) kOutlineMinCellDip);
        float  w       = (float) (m_outlineRect.right - m_outlineRect.left) * drawnScale;
        float  h       = (float) (m_outlineRect.bottom - m_outlineRect.top) * drawnScale;
        float  margin  = (w < minimum) ? w : 0.0f;
        float  x       = frame.left + ((float) m_outlineRect.left - originX) * drawnScale - margin - stroke;
        float  y       = frame.top  + ((float) m_outlineRect.top  - originY) * drawnScale - margin - stroke;

        w += 2.0f * (margin + stroke);
        h += 2.0f * (margin + stroke);

        for (const auto & edge : { std::array<float, 4> { x, y, w, stroke },          std::array<float, 4> { x, y + h - stroke, w, stroke },
                                   std::array<float, 4> { x, y, stroke, h },          std::array<float, 4> { x + w - stroke, y, stroke, h } })
        {
            hr = text.FillRect (edge[0], edge[1], edge[2], edge[3], theme.Accent());
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    m_map.PaintPing (text, theme, m_geometry.GetScaler(), picture->pingX, picture->pingY, frame.left - originX * drawnScale, frame.top - originY * drawnScale, drawnScale);

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::IsPinging
//
//  Whether a picture with a ping is in the rows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextPictures::IsPinging() const
{
    for (int row = 0; row < (int) m_layout.GetRows().size(); row++)
    {
        const DxuiTextPicture  * picture = GetPicture (row);

        if (picture != nullptr && picture->pingX >= 0 && picture->pingY >= 0)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::TickPing
//
//  The ping's animation: true when a frame changed, so the view is to be
//  painted again.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextPictures::TickPing (int64_t nowMs)
{
    return m_map.TickPing (nowMs, IsPinging());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::Zoom
//
//  Ctrl with the wheel over a zoomable picture; false off one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextPictures::Zoom (POINT point, float notches)
{
    int                    row   = 0;
    int                    x     = 0;
    int                    y     = 0;
    DxuiPictureMap::Frame  frame;



    if (!HitTest (point, row, x, y) || !GetPicture (row)->zoom || !GetFrame (row, frame))
    {
        return false;
    }

    m_map.Zoom (GetKey (row), frame, point, notches);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::TryStartPan
//
//  A left press on a zoomable picture starts panning it; false off one, so
//  the view selects text there.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextPictures::TryStartPan (POINT point)
{
    int  row = 0;
    int  x   = 0;
    int  y   = 0;



    if (!HitTest (point, row, x, y) || !GetPicture (row)->zoom)
    {
        return false;
    }

    m_map.StartPan (GetKey (row), point);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::Pan
//
//  The panned picture follows the pointer, in its frame on the view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextPictures::Pan (POINT point)
{
    DxuiPictureMap::Frame  frame;



    for (int row = 0; row < (int) m_layout.GetRows().size(); row++)
    {
        if (m_map.IsFor (GetKey (row)) && GetFrame (row, frame))
        {
            m_map.Pan (frame, point);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::GetPicture
//
//  A row's picture, or null for a row of text.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiTextPicture * DxuiTextPictures::GetPicture (int row) const
{
    const std::vector<DxuiTextLayout::Row> &  rows = m_layout.GetRows();



    return (row >= 0 && row < (int) rows.size()) ? rows[(size_t) row].picture.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::GetPictureLines
//
//  How many lines of `lineHeightPx` a picture takes at a text width: as
//  many as it is tall at the scale it fits, at least one.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextPictures::GetPictureLines (const DxuiTextPicture & picture, float widthPx, float pxPerDip, int lineHeightPx)
{
    float  height = (float) picture.height * GetPictureScale (picture, widthPx, pxPerDip);



    return (std::max) (1, (int) std::ceil (height / (float) (std::max) (1, lineHeightPx)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::GetTextLeft
//
//  Where a line's first character is drawn, which is off the view's left edge
//  once a row that doesn't wrap has scrolled.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextPictures::GetTextLeft() const
{
    return (float) m_geometry.GetTextOrigin().x - (float) m_viewport.GetLeftColumn() * m_layout.GetCellAdvance();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::PaintPicture
//
//  The picture with its top left at `left` and `top`.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextPictures::PaintPicture (IDxuiTextRenderer & text, const DxuiTextPicture & picture, float left, float top, float widthPx, float pxPerDip)
{
    HRESULT  hr    = S_OK;
    float    scale = GetPictureScale (picture, widthPx, pxPerDip);



    if (picture.width > 0 && picture.height > 0 && picture.bgra.size() >= (size_t) picture.width * (size_t) picture.height)
    {
        hr = text.DrawFramebuffer (picture.bgra.data(), picture.width, picture.height, left, top, (float) picture.width * scale, (float) picture.height * scale);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::GetTextWidth
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextPictures::GetTextWidth() const
{
    return (float) m_geometry.GetTextColumns (m_viewport.IsScrollbarVisible()) * m_layout.GetCellAdvance();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPictures::GetPictureScale
//
//  Pixels on screen for each of the picture's: a DIP's, `pxPerDip`, or
//  fewer to fit `widthPx`.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextPictures::GetPictureScale (const DxuiTextPicture & picture, float widthPx, float pxPerDip)
{
    float  scale = pxPerDip;



    if (picture.width > 0 && (float) picture.width * scale > widthPx)
    {
        scale = (std::max) (widthPx, 1.0f) / (float) picture.width;
    }

    return scale;
}
