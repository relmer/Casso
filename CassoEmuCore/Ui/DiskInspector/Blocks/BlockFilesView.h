#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesView
//
//  Block mode's Files tab: each file and directory of the current block's
//  volume, a row each under column headings, with its path, its kind, its
//  count of blocks and its first block. A click on a row selects it, and
//  the panel highlights its blocks on the grid. The wheel scrolls the list.
//
////////////////////////////////////////////////////////////////////////////////

class BlockFilesView : public IDxuiControl
{
public:
    static constexpr float  kRowDip   = 22.0f;
    static constexpr float  kTextDip  = 12.0f;
    static constexpr float  kPadDip   = 8.0f;
    static constexpr float  kKindDip  = 90.0f;
    static constexpr float  kCountDip = 80.0f;
    static constexpr float  kFirstDip = 90.0f;

    struct Row
    {
        std::wstring  path;
        bool          isDirectory = false;
        uint32_t      blocks      = 0;
        uint32_t      first       = 0;
    };

    using SelectFn = std::function<void (const Row & row)>;

    void  SetRows     (vector<Row> rows);
    void  SetSelected (const std::wstring & path) { m_selected = path; }
    void  SetOnSelect (SelectFn fn)               { m_onSelect = std::move (fn); }

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override { SetBounds (boundsPx); m_scaler = scaler; }
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    std::wstring        GetAccessibleName () const override { return L"Files"; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }

private:
    int   GetVisibleRows () const;
    int   FindRowAt      (POINT pointPx) const;
    void  ClampScroll    ();

    vector<Row>    m_rows;
    std::wstring   m_selected;
    SelectFn       m_onSelect;
    DxuiDpiScaler  m_scaler;
    int            m_scroll = 0;
    int            m_hover  = -1;
};
