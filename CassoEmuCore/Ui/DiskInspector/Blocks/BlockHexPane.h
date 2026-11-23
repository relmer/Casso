#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHexSource.h"

class BlockFieldsView;
class BlockHexView;
class BlockInspectorModel;
struct BlockField;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane
//
//  The current block's hex view beside its fields list: the bytes with
//  their marks, a byte's tip, a cut-off field row's tip, and the most bytes
//  in a hex row that fit. While the block is edited, an accent frame is
//  drawn around the hex view and the bytes have a light accent tint. The
//  bytes are drawn as the debugger's memory window draws them: the accent
//  behind the caret byte while the hex view has the keys, an accent box
//  around a byte being typed, and edited bytes in the changed-byte red. The
//  views are children of the panel that creates the pane.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHexPane
{
public:
    static constexpr int    kBytesPerRow   = 16;
    static constexpr int    kMinHexBytes   = 8;       // the fewest bytes in a hex row when the fields are fitted
    static constexpr float  kHexScrollDip  = 12.0f;   // room for the hex view's vertical scroll bar
    static constexpr float  kFrameDip      = 2.0f;    // the edit frame around the hex view, reserved whether or not it's drawn
    static constexpr float  kTintShare     = 0.12f;   // the accent's share of the edit tint behind the bytes
    static constexpr float  kMinContrast   = 4.5f;    // the least contrast of an edited byte's ink with its fill

    //  The theme's colors the edit look is made from.
    struct EditColors
    {
        uint32_t  accent     = 0;
        uint32_t  background = 0;
        uint32_t  foreground = 0;
    };

    //  What is drawn for a byte in one column.
    struct ByteState
    {
        bool     isEditing   = false;   // the block is being edited
        bool     isCaret     = false;   // the caret byte, with the hex view focused
        bool     isTyping    = false;   // the byte's high nibble is typed and its low nibble isn't yet
        bool     isHexColumn = false;
        bool     isSelected  = false;
        uint8_t  mark        = 0;
    };

    void  Create       (DxuiPanel & parent);
    void  SetPopupHost (DxuiHwndSource * host) { m_fieldTip.SetPopupHost (host); }

    BlockHexView     & GetHexView () const { return *m_hex; }
    BlockFieldsView  & GetFields  () const { return *m_fields; }

    //  The block's bytes with their marks, and its fields. True when the
    //  block differs from the last one drawn, so the hex view starts over
    //  at the top.
    bool  ShowBytes (BlockInspectorModel & model);

    //  The bytes again, with the field under the pointer marked over the
    //  link marks.
    void  MarkField (BlockInspectorModel & model, const BlockField * field);

    void  Layout (const RECT & hexPx, const RECT & fieldsPx, const DxuiDpiScaler & scaler);

    //  Whether the block is being edited and the hex view has the keys, and
    //  the theme's colors, for the next paint.
    void  SetEditLook (bool isEditing, bool hasFocus, const EditColors & colors);

    //  The accent frame around the hex view while editing.
    void  PaintFrame (IDxuiPainter & painter) const;

    //  How a byte is drawn in both columns: false for the hex view's look.
    static bool  PickByteStyle (const EditColors & colors, const ByteState & state, DxuiHexViewTypes::ByteStyle & outStyle);

    //  ink, moved toward black or white until it reads on ill.
    static uint32_t  GetReadableInk (uint32_t ink, uint32_t fill);

    //  An edited byte's ink before it is fitted to its fill.
    static uint32_t  GetChangedInk  (uint32_t background);

    //  The hex view measures its text as it paints, so its rows are fitted
    //  again once their width is known. True when the rows were fitted.
    bool  RefitRows (const DxuiDpiScaler & scaler);

    //  The hex view's width for kMinHexBytes in a row, with its scroll bar,
    //  or 0 before the view has measured its text.
    int  MeasureMinHexPx (const DxuiDpiScaler & scaler);

    //  The hex view's width within `widthPx`: its columns for the most bytes
    //  in a row that fit, its scroll bar and the edit frame, or `widthPx`
    //  before the view has measured its text.
    int  FitHexWidthPx (int widthPx, const DxuiDpiScaler & scaler);

    //  The tip for a byte under a point: its field, and what its color
    //  marks. False where there is no byte.
    bool  GetTooltip (POINT pointPx, const BlockInspectorModel & model, std::wstring & outText, RECT & outAnchorPx, vector<DxuiTooltipSwatch> & outSwatches) const;

    //  A cut-off field row under the pointer, drawn whole over the row.
    void  UpdateFieldTip (const DxuiMouseEvent & ev, IDxuiTextRenderer * text, const IDxuiTheme * theme);

private:
    static constexpr uint32_t  s_kChangedColor = 0xFFE0A030;
    static constexpr uint32_t  s_kDiffersColor = 0xFFF06060;
    static constexpr uint32_t  s_kFieldColor   = 0xFF60B0F0;
    static constexpr uint32_t  s_kLinkColor    = 0xFF60D090;

    uint32_t  GetMarkColor (uint8_t mark) const;

    void  FitRows (int widthPx, const DxuiDpiScaler & scaler);

    BlockHexView     * m_hex       = nullptr;
    BlockFieldsView  * m_fields    = nullptr;
    BlockHexSource     m_source;
    DxuiInPlaceTip     m_fieldTip;
    int64_t            m_drawn     = -1;
    int                m_contentPx = 0;     // the hex view's width for all its columns when the rows were fitted
    RECT               m_frameRect = {};
    int                m_framePx   = 0;
    bool               m_isEditing = false;
    bool               m_hasFocus  = false;
    EditColors         m_colors;
};
