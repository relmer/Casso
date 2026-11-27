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
//  in a hex row that fit. The hex view draws the edit look. While comparing,
//  the other image's block is in a second hex view under the first, linked
//  to it, and each view's block is outlined with its image's name. The
//  views are children of the panel that creates the pane.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHexPane
{
public:
    static constexpr int    kBytesPerRow   = 16;
    static constexpr int    kMinHexBytes   = 8;       // the fewest bytes in a hex row when the fields are fitted
    static constexpr float  kHexScrollDip  = 12.0f;   // room for the hex view's vertical scroll bar

    void  Create       (DxuiPanel & parent);
    void  SetPopupHost (DxuiHwndSource * host) { m_fieldTip.SetPopupHost (host); }

    //  The band under a byte that differs from the other image's.
    void  SetDifferenceArgb (uint32_t argb) { m_differenceArgb = argb; }

    BlockHexView     & GetHexView  () const { return *m_hex; }
    BlockHexView     & GetHexViewB () const { return *m_hexB; }
    BlockFieldsView  & GetFields   () const { return *m_fields; }

    //  The block's bytes with their marks, and its fields. True when the
    //  block differs from the last one drawn, so the hex view starts over
    //  at the top, or when comparing began or ended, so the views are laid
    //  out again.
    bool  ShowBytes (BlockInspectorModel & model);

    //  The bytes again, with the field under the pointer marked over the
    //  link marks.
    void  MarkField (BlockInspectorModel & model, const BlockField * field);

    void  Layout (const RECT & hexPx, const RECT & fieldsPx, const DxuiDpiScaler & scaler);

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
    static constexpr uint32_t  s_kFieldColor   = 0xFF60B0F0;
    static constexpr uint32_t  s_kLinkColor    = 0xFF60D090;
    static constexpr uint32_t  s_kEdgeColor    = 0xFF707880;   // the outline around each image's block while comparing
    static constexpr float     s_kViewGapDip   = 8.0f;         // between the two views while comparing

    uint32_t  GetMarkColor   (uint8_t mark) const;
    bool      PickStyle      (uint8_t mark, bool isSelected, DxuiHexViewTypes::ByteStyle & outStyle) const;
    void      CopySelection  (DxuiHexView & from, DxuiHexView & to);
    void      ShowOtherBytes (BlockInspectorModel & model, uint32_t block, std::span<const Byte> bytes);

    static std::wstring  GetThisName (const BlockInspectorModel & model);

    void  FitRows (int widthPx, const DxuiDpiScaler & scaler);

    BlockHexView     * m_hex            = nullptr;
    BlockHexView     * m_hexB           = nullptr;
    BlockFieldsView  * m_fields         = nullptr;
    BlockHexSource     m_source;
    BlockHexSource     m_sourceB;
    DxuiHexViewLink    m_link;
    DxuiInPlaceTip     m_fieldTip;
    int64_t            m_drawn          = -1;
    int                m_contentPx      = 0;     // the hex view's width for all its columns when the rows were fitted
    uint32_t           m_differenceArgb = 0xFFC030A0;
    bool               m_isComparing    = false;
    bool               m_isCopyingSel   = false; // one view's selection is being copied to the other
};
