#pragma once

#include "Pch.h"
#include "IDxuiImageStripSource.h"



class DxuiHwndSource;
class DxuiPopupHost;
class IDxuiPainter;
class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview
//
//  The hovered cell's picture, enlarged, in a popup of the strip's popup
//  host: below a strip lying down, to the right of one standing up, flipped
//  when there is no room. The anchor for a picture's size comes from the
//  strip, in screen pixels, centered on the pointer. The popup lets the
//  pointer through and never activates, so the hover stays on the strip and
//  the focus on the window.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiStripPreview
{
public:
    using Image    = IDxuiImageStripSource::Image;
    using AnchorFn = std::function<RECT (SIZE sizeDip)>;

    ~DxuiStripPreview();

    void  SetPopupHost (DxuiHwndSource * host);
    void  SetAnchor    (AnchorFn anchor)  { m_anchor = std::move (anchor); }
    void  SetVertical  (bool isVertical)  { m_isVertical = isVertical; }

    //  Once a frame, for the hovered cell: its full-size picture, or null
    //  while it is not drawn, and its thumbnail.
    void  Choose       (Image full, Image thumb);

    //  The open popup moves with the pointer, its picture as it was.
    void  Follow       ();
    void  Hide         ();

    bool          IsShown   () const { return m_popup != nullptr; }
    RECT          GetRectPx () const;
    const Image & GetImage  () const { return m_image; }

private:
    void  Open    ();
    void  Move    ();
    void  Render  (IDxuiPainter & painter, IDxuiTextRenderer & text);

    DxuiHwndSource  * m_popupHost  = nullptr;
    DxuiPopupHost   * m_popup      = nullptr;
    AnchorFn          m_anchor;
    Image             m_image;
    Image             m_thumb;               // the hovered cell's picture when the preview was chosen
    SIZE              m_sizeDip    = {};     // the last full-size picture's size; a thumbnail scales to it
    bool              m_isVertical = false;
};
