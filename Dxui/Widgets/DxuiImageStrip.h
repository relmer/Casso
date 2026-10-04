#pragma once

#include "Pch.h"
#include "DxuiToolbar.h"
#include "IDxuiImageStripSource.h"



class DxuiHwndSource;
class DxuiPopupHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip
//
//  A row of pictures as a toolbar entry: cells as thick as the entry, each
//  as long as its picture's aspect ratio makes it, laid end to end from the
//  leading edge with no gap between them, as many as fit whole. Docked
//  against a side, the toolbar stands the entry on end and the cells run top
//  to bottom. An IDxuiImageStripSource supplies the pictures.
//
//  The pointer over a cell outlines it and shows the cell's full-size picture
//  in a popup beside the strip: the picture's own pixels, scaled for the
//  window's DPI, in a popup that takes neither focus nor the pointer. A click
//  goes to the source with the cell's index.
//
//  The host calls Sync once a frame, so a preview the source had not drawn
//  yet appears when it has, and keeps its popup host current as the toolbar
//  docks and floats.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiImageStrip : public IDxuiToolbarCustomEntry
{
public:
    //  Room the entry asks for when the host gives it no length of its own.
    static constexpr int  kDefaultLengthDp = 480;

    ~DxuiImageStrip() override;

    void  SetSource        (IDxuiImageStripSource * source) { m_source = source; }
    void  SetAspect        (float widthOverHeight)          { m_aspect = widthOverHeight; }
    void  SetPopupHost     (DxuiHwndSource * host);

    //  The length the entry asks for, in pixels; 0 asks for kDefaultLengthDp.
    void  SetPreferredLengthPx (int lengthPx)                { m_preferredPx = lengthPx; }

    int   GetCellCount     () const { return m_count; }
    int   GetHoveredCell   () const { return m_hovered; }
    bool  IsPreviewShown   () const { return m_preview != nullptr; }

    void  Sync             ();
    void  HidePreview      ();

    //  The cell's length along the strip for a strip `thicknessPx` thick, the
    //  number of cells that fit whole in `lengthPx`, where cell `index` lies
    //  in `rc`, and which cell a point is over (-1 for none).
    static int   GetCellLength (int thicknessPx, float aspect, bool vertical);
    static int   GetCellCount  (int lengthPx, int cellPx);
    static RECT  GetCellRect   (const RECT & rc, int index, int cellPx, bool vertical);
    static int   HitTestCell   (const RECT & rc, int count, int cellPx, bool vertical, int x, int y);

    int              GetWidthPx    (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const override;
    int              GetMinWidthPx (const DxuiDpiScaler & scaler) const override;
    void             Layout        (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler) override;
    void             Paint         (IDxuiPainter      & painter,
                                    IDxuiTextRenderer & text,
                                    const IDxuiTheme  & theme,
                                    bool                hovered,
                                    bool                pressed,
                                    bool                labeled) override;
    const wchar_t *  GetTooltipAt  (int x, int y, RECT & anchor) const override;
    bool             OnClick       (int x, int y) override;
    bool             OnMouseMove   (int x, int y) override;
    bool             OnLButtonDown (int x, int y) override;
    void             OnMouseLeave  () override;

private:
    static constexpr float  kDefaultAspect  = 560.0f / 384.0f;
    static constexpr float  kHoverEdgeDip   = 2.0f;
    static constexpr int    kMinThicknessPx = 1;

    void  ShowPreview  ();
    void  RenderPreview (IDxuiPainter & painter, IDxuiTextRenderer & text);

    IDxuiImageStripSource           * m_source      = nullptr;
    DxuiHwndSource                  * m_popupHost   = nullptr;
    DxuiPopupHost                   * m_preview     = nullptr;
    IDxuiImageStripSource::Image      m_previewImage;
    int                               m_previewCell = -1;
    float                             m_aspect      = kDefaultAspect;
    int                               m_preferredPx = 0;

    DxuiDpiScaler                     m_scaler;
    RECT                              m_rc          = {};
    bool                              m_vertical    = false;
    int                               m_cellPx      = 0;
    int                               m_count       = 0;
    int                               m_hovered     = -1;
};
