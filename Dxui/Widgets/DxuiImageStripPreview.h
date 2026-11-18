#pragma once

#include "Pch.h"
#include "IDxuiImageStripSource.h"


class DxuiHwndSource;
class DxuiPopupHost;
class IDxuiImageStripPreviewHost;
class IDxuiPainter;
class IDxuiTextRenderer;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview
//
//  The full-size picture of the cell under the pointer, in one popup beside
//  a DxuiImageStrip that follows the pointer along the strip. The popup takes
//  neither focus nor the pointer.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiImageStripPreview
{
public:
    explicit DxuiImageStripPreview (IDxuiImageStripPreviewHost & host) : m_host (host) {}
    ~DxuiImageStripPreview();

    void  SetPopupHost   (DxuiHwndSource * popupHost);
    void  SetThemeColors (const IDxuiTheme & theme);
    void  Sync           (IDxuiImageStripSource * source, int hovered);
    void  FollowPointer  ();
    void  Hide           ();

    bool                                  IsShown      () const { return m_popup != nullptr; }
    SIZE                                  GetSizeDip   () const;
    RECT                                  GetRectPx    () const;
    const IDxuiImageStripSource::Image &  GetShownImage() const { return m_image; }

private:
    static constexpr int  kPadDip = 6;   // between the preview's frame and its picture, past the corners' curve

    void  Show         ();
    void  Move         ();
    void  ReleasePopup ();
    void  Render       (IDxuiPainter & painter, IDxuiTextRenderer & text);

    IDxuiImageStripPreviewHost    & m_host;
    DxuiHwndSource                * m_popupHost  = nullptr;
    DxuiPopupHost                 * m_popup      = nullptr;
    IDxuiImageStripSource::Image    m_image;
    IDxuiImageStripSource::Image    m_thumb;                // the hovered cell's picture when the preview was chosen
    uint32_t                        m_borderArgb = 0;       // the theme's toolbar border color, from the last paint
    uint32_t                        m_backArgb   = 0;       // the theme's tooltip background, from the last paint
    SIZE                            m_sizeDip    = {};      // the last full-size picture's size; a thumbnail scales to it
};
