#pragma once

#include "Pch.h"
#include "IDxuiImageStripSource.h"



class DxuiHwndSource;
class DxuiPopupHost;
class IDxuiPainter;
class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout
//
//  The rows a strip's mark with rows opens, one for each the source lists
//  for it: a picture and two lines of text, in a popup of the strip's popup
//  host above the mark. It opens once the pointer has rested on the mark
//  for kOpenDelayMs, and closes when the pointer moves off the mark without
//  going into it, or off it. The row under the pointer goes to the source
//  as hovered, and 0 once none is; a click on a row goes to the source and
//  closes the flyout. Rows whose pictures were not drawn yet are read
//  again each Sync, so each appears once drawn.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiStripFlyout
{
public:
    static constexpr int64_t  kOpenDelayMs  = 400;
    static constexpr float    kPictureDip   = 64.0f;   // a row's picture, tall
    static constexpr float    kPadDip       = 6.0f;
    static constexpr float    kTextDip      = 200.0f;  // the text beside it, wide
    static constexpr float    kFontDip      = 12.0f;

    ~DxuiStripFlyout();

    void  SetPopupHost (DxuiHwndSource * host);
    void  SetAspect    (float widthOverHeight) { m_aspect = widthOverHeight; }

    //  The pointer is over the badge of the mark markId, at anchorScreen, or
    //  over no mark with rows (isOverMark false).
    void  Track        (IDxuiImageStripSource * source, bool isOverMark, uint64_t markId, const RECT & anchorScreen, int64_t nowMs);

    //  Once a frame: a flyout due opens, and an open one takes the pictures
    //  drawn since.
    void  Sync         (int64_t nowMs);
    void  Close        ();

    //  The pointer at a point in the flyout's pixels, and a click there.
    void  HoverAt      (POINT localPx);
    void  ClickAt      (POINT localPx);

    bool  IsOpen       () const { return m_isOpen; }
    bool  WantsTick    () const { return m_isOverMark || m_isOpen; }
    bool  IsPointerOn  () const;

    //  Where the open flyout is on the screen, in pixels; false while closed.
    bool  TryGetPlacedRect (RECT & outScreenPx) const;
    int   GetHoveredRow() const { return m_hoveredRow; }

    const std::vector<DxuiStripForkRow> &  GetRows () const { return m_rows; }

    //  The row at a height in the flyout, rows rowPx tall, or -1 for none.
    static int  HitTestRow (int localY, int rowPx, int count);

private:
    void  Open         ();
    void  Render       (IDxuiPainter & painter, IDxuiTextRenderer & text);
    int   GetRowPx     () const;
    SIZE  GetSizeDip   () const;

    IDxuiImageStripSource          * m_source     = nullptr;
    DxuiHwndSource                 * m_popupHost  = nullptr;
    DxuiPopupHost                  * m_popup      = nullptr;
    std::vector<DxuiStripForkRow>    m_rows;
    RECT                             m_anchor     = {};
    uint64_t                         m_markId     = 0;
    int64_t                          m_restSince  = 0;
    int                              m_hoveredRow = -1;
    float                            m_aspect     = 560.0f / 384.0f;
    bool                             m_isOverMark = false;
    bool                             m_isOpen     = false;
};
