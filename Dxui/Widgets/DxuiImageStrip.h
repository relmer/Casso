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
//  The pointer over a cell frames it, shows the source's labels for it above
//  and below the pictures of a strip lying down, and shows the cell's
//  full-size picture in a popup beside the strip, past the labels and
//  centered on the pointer as it moves: the picture's own pixels, scaled for
//  the window's DPI, in a popup that takes neither focus nor the pointer. A
//  click goes to the source with the cell's index. The labels at the strip's
//  two ends are buttons, with the toolbar's hover and pressed chrome, a tip
//  each and a click of their own.
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

    //  Room kept above and below the pictures of a strip lying down, for the
    //  labels of the source's playhead line and its leading end.
    void  SetLabelRoomDp   (float roomDp)                   { m_labelRoomDp = roomDp; }

    //  Measures the labels at the strip's two ends, which sit beside the
    //  pictures of a strip lying down, and above and below those of a strip
    //  standing up.
    void  SetTextRenderer  (IDxuiTextRenderer * text)       { m_text = text; }

    //  Whether a label at either end has grown past the room the last layout
    //  kept for it, so the strip needs laying out again.
    bool  HasOutgrownLabelRoom ();

    RECT  GetPicturesRect  () const { return m_rc; }

    bool  IsDraggingPlayhead () const { return m_isDragging; }

    //  How far either side of the playhead line a press takes hold of it.
    static constexpr float  kPlayheadGripDip = 9.0f;

    //  Whether a point along the strip is close enough to the line to grab it.
    static bool  IsOnPlayhead  (int along, float lineAlong, float gripPx);

    int   GetCellCount     () const { return m_count; }
    int   GetHoveredCell   () const { return m_hovered; }
    bool  IsPreviewShown   () const { return m_preview != nullptr; }

    //  Where the preview's picture sits on screen, empty with none up, and
    //  the picture it shows.
    RECT                                  GetPreviewRectPx () const;
    const IDxuiImageStripSource::Image &  GetShownPreview  () const { return m_previewImage; }

    void  Sync             ();
    void  HidePreview      ();

    //  The cell's length along the strip that keeps the picture's aspect for a
    //  strip `thicknessPx` thick, the whole number of such cells nearest to
    //  filling `lengthPx`, where cell `index` of `count` lies in `rc` (the
    //  last reaching the far edge), and which cell a point is over (-1 for none).
    static int   GetCellLength (int thicknessPx, float aspect, bool vertical);
    static int   GetCellCount  (int lengthPx, int cellPx);
    static RECT  GetCellRect   (const RECT & rc, int index, int count, int cellPx, bool vertical);
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
    void             OnLButtonUp   (int x, int y) override;

private:
    static constexpr float  kDefaultAspect  = 560.0f / 384.0f;
    static constexpr float  kMarkerDip      = 4.0f;
    static constexpr int    kMinThicknessPx = 1;
    static constexpr float  kPlayheadDip    = 3.0f;
    static constexpr float  kLineReachDip   = 4.0f;   // how far the line runs past the pictures
    static constexpr float  kLabelFontDip   = 11.0f;
    static constexpr float  kLabelPadDip    = 3.0f;
    static constexpr float  kSidePadDip     = 6.0f;
    static constexpr float  kLiveDotDip     = 6.0f;
    static constexpr float  kHoverFrameDip  = 1.5f;
    static constexpr float  kHoverEdgeDip   = 1.0f;

    //  The end labels, each a button.
    enum class Part { None, Leading, Trailing };

    void  SetHovered       (int index);
    Part  HitTestPart      (int x, int y) const;
    void  PaintPartChrome  (IDxuiPainter & painter, const IDxuiTheme & theme, const RECT & rc, Part part);
    void  ShowPreview      ();
    void  FollowPointer    ();
    void  PaintHoverFrame  (IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & cell);
    void  PaintRing        (IDxuiTextRenderer & text, const RECT & rc, float thick, uint32_t argb);
    void  PaintLabelPair   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & top, const std::wstring & bottom, float centerX);
    void  GetLabelSpan     (IDxuiTextRenderer & text, const std::wstring & label, float centerX, float & outLeft, float & outWidth) const;
    void  HideOverlap      (IDxuiTextRenderer & text, std::wstring & label, float centerX, const std::wstring & other, float otherX) const;
    int   GetSideLabelPx   (const std::wstring & label, bool isTrailing) const;
    void  PaintSideLabel   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label, const RECT & rc, bool isTrailing);
    void  MovePreview      ();
    RECT  GetPreviewAnchor () const;
    void  RenderPreview    (IDxuiPainter & painter, IDxuiTextRenderer & text);
    void  PaintPlayhead    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float offset, const std::wstring & top, const std::wstring & bottom);
    void  PaintLabel       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label, float centerX, float top, float height);
    float GetLineAlong     (float offset) const;
    float GetOffsetAt      (int x, int y) const;

    //  A strip standing up: its end labels above and below the pictures,
    //  and its playhead line's labels beside the line, across the strip.
    int    GetStandingLabelPx          (const std::wstring & label) const;
    float  GetFittedFontPx             (IDxuiTextRenderer & text, const std::wstring & label, float maxWidthPx, float & outWidthPx) const;
    void   PlaceStandingEndLabels      ();
    void   PaintStandingPlayheadLabels (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float along, float thick, const std::wstring & top, const std::wstring & bottom);
    void   PaintStandingLabel          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label, float top, float height);

    IDxuiImageStripSource           * m_source      = nullptr;
    IDxuiTextRenderer               * m_text        = nullptr;
    DxuiHwndSource                  * m_popupHost   = nullptr;
    DxuiPopupHost                   * m_preview     = nullptr;
    IDxuiImageStripSource::Image      m_previewImage;
    IDxuiImageStripSource::Image      m_previewThumb;      // the hovered cell's picture when the preview was chosen
    SIZE                              m_previewDip  = {};   // the last full-size picture's size; a thumbnail scales to it
    POINT                             m_pointer     = {};   // where the pointer last moved over the strip
    float                             m_aspect      = kDefaultAspect;
    int                               m_preferredPx = 0;

    DxuiDpiScaler  m_scaler;
    RECT           m_rc           = {};   // the pictures
    RECT           m_outer        = {};   // the whole entry, labels included
    RECT           m_lead         = {};   // the label before the pictures
    RECT           m_trail        = {};   // the label after them
    float          m_labelRoomDp  = 0.0f;
    float          m_dragOffset   = 0.0f;
    bool           m_isDragging   = false;
    bool           m_isClickEaten = false;
    Part           m_hoverPart    = Part::None;
    Part           m_pressPart    = Part::None;
    Part           m_armedPart    = Part::None;   // the label pressed for the click that follows the release
    bool           m_vertical     = false;
    int            m_cellPx       = 0;
    int            m_idealPx      = 0;
    int            m_count        = 0;
    int            m_hovered      = -1;

    mutable std::wstring  m_tip;          // an end label's tip, kept while it shows
};
