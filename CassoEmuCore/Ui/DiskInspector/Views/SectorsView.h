#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"

struct HeaderItem;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView
//
//  The Sector data tab: the selected sector's header on fixed lines, the
//  edit bar's row, then its 256 bytes in an editable hex view, with zero
//  bytes muted and bad data marked (FR-040); while comparing, B's paired
//  sector in a second hex view beside or below it, the two scrolling
//  together and the bytes that differ marked in both. The right-click menu
//  sets the bytes across a row and where B's view goes.
//  The hex views draw the edit look and take typing while the sector is
//  edited (FR-097). The view model has the byte selection, and the main hex
//  view follows it.
//
////////////////////////////////////////////////////////////////////////////////

class SectorsView : public InspectorView
{
public:
    static constexpr int  kRowDip         = 18;
    static constexpr int  kHeaderDip      = 22;
    static constexpr int  kHeaderLines    = 2;
    static constexpr int  kBytesPerRow    = 16;

    //  Where B's hex view goes while comparing: beside A's or below it,
    //  whichever has more bytes visible at once, or as chosen.
    enum class Arrangement { Auto, SideBySide, Stacked };

    explicit SectorsView (InspectorViewContext & context);

    //  The width a sector's bytes take at full size.
    static int  GetFullWidthPx (const DxuiDpiScaler & scaler);

    //  The width a hex view takes with ytesPerRow bytes across.
    static int  GetViewWidthPx (const DxuiDpiScaler & scaler, int bytesPerRow);

    //  Bytes across a row of the hex views, as laid out.
    int  GetBytesPerRow () const { return m_hexA.GetBytesPerRow(); }

    //  The hex view's menu, with where B's view goes, for a right-click on
    //  either view. False for a point on neither.
    bool  TryShowHexMenu (POINT pointPx, DxuiHwndSource & host);

    void  Layout     (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;

    //  Editing (FR-097): typing goes to the selected sector's pending edit at
    //  the caret, in the column last clicked.
    void  SetEditing         (bool isEditing);
    bool  IsEditing          () const { return m_isEditing; }
    bool  OnEditKey          (const DxuiKeyEvent & ev);
    bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const override;

    //  Tab arrives at the hex column and Shift+Tab at the text column.
    void  OnFocusEntered     (bool forward) override;

    //  Pixels at the right of the header's first line reserved for the
    //  window's "Edit" and "Copy sector" buttons.
    void  SetHeaderReserve (int px) { m_headerReservePx = static_cast<float> (px); }

    //  Called as the view is laid out, with the space on the header's first
    //  line left of its buttons and the row under the header. The edit bar
    //  goes on the line when it fits; the call sets outLeft to the bar's left
    //  edge there and returns the height it takes of the row, which the bytes
    //  start below.
    using EditRowFn = std::function<int (const RECT & inlinePx, const RECT & rowPx, int & outLeft)>;
    void  SetEditRow (EditRowFn fn) { m_editRow = std::move (fn); }

    using CanCopyFn = std::function<bool (int toSide)>;
    using CopyFn    = std::function<void (int toSide)>;

    //  The menu's "Copy sector from" the other image into side `toSide`:
    //  whether it can run, and running it.
    void  SetCopySector (CanCopyFn canRun, CopyFn run) { m_canCopySector = std::move (canRun); m_copySector = std::move (run); }

    //  B's bytes for A's sector `index` while comparing, and whether they are
    //  a data field no address field leads; empty for none.
    std::span<const Byte>  GetPairedBytes (const TrackAnalysis & track, int index, bool & outIsLoose) const;

    //  The side whose view was last clicked, 0 for A and 1 for B, which
    //  editing and "Copy sector from" act on; and B's sector in the
    //  selected sector's slot, or -1.
    int   GetActiveSide        () const { return m_isB ? m_activeSide : 0; }
    int   GetSelectedSectorB   () const;

    //  Called with the byte double-clicked.
    using OpenFn = std::function<void (int offset)>;
    void  SetOnOpen  (OpenFn fn)    { m_onOpen = std::move (fn); }

    //  The selected sector's first row at the top of the views, at the next
    //  paint.
    void  ScrollToSelected ();

    //  The byte the caret is on, moved into view.
    void  GoToByte   (int offset);

private:
    void   PaintHeader             (IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track);
    void   PaintHeaderItem         (IDxuiTextRenderer & text, const IDxuiTheme & theme, const HeaderItem & item, float x, float y, float rowH) const;
    int    GetEdit                 () const;
    int    GetEditB                () const;
    void   ShowBytes               (const TrackAnalysis & track);
    void   ScrollToSelectedIfMoved ();
    void   BuildSlots              (const TrackAnalysis & track);
    void   AddSectorsOnlyInB       (const TrackAnalysis & track);

    std::span<const Byte>  GetLooseBBytes  (const TrackAnalysis & track, size_t slot, int & outSectorB) const;
    std::span<const Byte>  GetSectorBBytes (int sectorB) const;

    void   PlaceBytes              (bool isB);
    void   PlaceEditRow            ();
    float  GetButtonsLeftPx        () const;
    void   AddMenuItems            (std::vector<DxuiPopupMenuItem> & items, bool isViewB);
    void   CopySelection           (DxuiHexView & from, DxuiHexView & to);
    int    PickBytesPerRow         (float widthPx) const;
    int    CountShown              (bool isSide, float widthPx, float roomPx, float gapPx) const;
    void   PaintTitleRow           (IDxuiPainter & painter, IDxuiTextRenderer & text);
    void   PaintToggle             (IDxuiPainter & painter);
    void   FollowSelection         ();
    void   TakeSelection           ();
    int    HitTest                 (POINT pointPx) const;
    bool   PickStyle               (const BlockHexSource & source, uint64_t offset, uint8_t mark, bool isSelected, DxuiHexViewTypes::ByteStyle & outStyle) const;

    static int  HitTest (const DxuiHexView & view, POINT pointPx);

    DxuiHexView     m_hexA;
    DxuiHexView     m_hexB;
    DxuiHexViewLink m_link;
    BlockHexSource  m_sourceA;
    BlockHexSource  m_sourceB;
    DxuiTheme       m_panelTheme;
    EditRowFn       m_editRow;
    OpenFn          m_onOpen;
    CanCopyFn       m_canCopySector;
    CopyFn          m_copySector;
    int64_t         m_pressMs         = 0;
    int             m_pressAt         = -1;
    int             m_pressSide       = 0;
    Arrangement     m_arrangement     = Arrangement::Auto;
    RECT            m_toggleRect      = {};    // the button that moves B's view
    float           m_titleTopPx      = 0.0f;  // the row over the views while comparing
    bool            m_isSide          = false; // B's view is beside A's
    struct Slot
    {
        int  sector  = -1;   // the sector of A, or -1 for a data field no address field leads
        int  field   = -1;   // its data field, or -1 for a sector only B has
        int  sectorB = -1;   // with no sector of A, the sector of B in the slot
    };

    vector<Slot>    m_slots;                   // A's data fields, then sectors only B has, a slot of the views' bytes each
    int             m_selectedSlot    = -1;    // the selected sector's slot
    int             m_shownSlot       = -1;    // the slot last scrolled to the top
    int             m_shownPerRow     = 0;     // and the bytes across a row then
    size_t          m_slotLen         = DiskFieldFormat::kSectorBytes;  // a sector's slot: its bytes, then padding to the end of the row
    DxuiHexView   * m_dragView        = nullptr;
    int             m_activeSide      = 0;     // the view last clicked, 0 for A and 1 for B
    float           m_headerReservePx = 0.0f;
    float           m_headerRightPx   = 0.0f;  // where the header's first line is cut off, short of the edit bar and the buttons
    int             m_bytesTopPx      = 0;     // where the bytes start, below the header and the edit row
    float           m_rowPx           = 0.0f;  // a row of the bytes at full size, once the hex view has measured it
    bool            m_isEditing       = false;
    bool            m_isFollowing     = false; // the main hex view is being set from the view model
    bool            m_isCopyingSel    = false; // one view's selection is being copied to the other
    bool            m_isB             = false;
};