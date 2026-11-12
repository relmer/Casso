#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"

class BlockInspectorModel;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView
//
//  Block mode's map: the partitions as bands across the top, then a cell for
//  each block or run of blocks, colored by its role. The current block is
//  outlined, and the highlighted file's blocks are marked. Pointing at a
//  cell reports its blocks, role and files; a click on a cell or a band
//  steps to its first block.
//
////////////////////////////////////////////////////////////////////////////////

class BlockMapView : public IDxuiControl
{
public:
    static constexpr float  kBandDip     = 18.0f;
    static constexpr float  kBandGapDip  = 6.0f;
    static constexpr float  kFontDip     = 11.0f;
    static constexpr float  kOutlineDip  = 2.0f;

    using SelectFn = std::function<void (uint32_t block)>;
    using HoverFn  = std::function<void (const std::wstring & text)>;

    void  SetModel    (BlockInspectorModel * model) { m_model = model; }
    void  SetOnSelect (SelectFn fn) { m_onSelect = std::move (fn); }
    void  SetOnHover  (HoverFn fn)  { m_onHover  = std::move (fn); }

    //  Reads each cell's role and highlight from the model again.
    void  Refresh ();

    //  What pointing at a cell reports.
    std::wstring  DescribeCell (uint32_t cell) const;

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    std::wstring        GetAccessibleName () const override { return L"Block map"; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }

private:
    static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

    RECT  GetGridRect () const;
    void  PaintBands  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void  PaintCells  (IDxuiPainter & painter, const IDxuiTheme & theme, const RECT & grid) const;

    BlockInspectorModel  * m_model       = nullptr;
    DxuiDpiScaler          m_scaler;
    BlockMapGrid           m_grid;
    vector<SectorRole>     m_roles;
    vector<bool>           m_highlighted;
    SelectFn               m_onSelect;
    HoverFn                m_onHover;
    int64_t                m_hoverCell   = -1;
};
