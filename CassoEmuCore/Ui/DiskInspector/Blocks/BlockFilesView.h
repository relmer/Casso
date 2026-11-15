#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesView
//
//  Block mode's Files tab: each file of the current block's volume, a row
//  each with its path and its count of blocks. A click on a row selects the
//  file's first block and highlights the file's blocks. The wheel scrolls
//  the list.
//
////////////////////////////////////////////////////////////////////////////////

class BlockFilesView : public IDxuiControl
{
public:
    static constexpr float  kRowDip   = 22.0f;
    static constexpr float  kTextDip  = 12.0f;
    static constexpr float  kPadDip   = 8.0f;
    static constexpr float  kCountDip = 90.0f;   // the count of blocks at a row's right end

    struct Row
    {
        std::wstring  path;
        uint32_t      blocks = 0;
        uint32_t      first  = 0;
    };

    using SelectFn = std::function<void (const Row & row)>;

    void  SetRows     (vector<Row> rows);
    void  SetOnSelect (SelectFn fn) { m_onSelect = std::move (fn); }

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override { SetBounds (boundsPx); m_scaler = scaler; }
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    std::wstring        GetAccessibleName () const override { return L"Files"; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }

private:
    int   GetVisibleRows () const;
    void  ClampScroll    ();

    vector<Row>    m_rows;
    SelectFn       m_onSelect;
    DxuiDpiScaler  m_scaler;
    int            m_scroll = 0;
    int            m_hover  = -1;
};
