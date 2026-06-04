#pragma once

#include "Pch.h"
#include "Core/IDxuiLayout.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiGridLayout
//
//  Fixed rows x cols grid with uniform gap. Per-child cell placement
//  is registered via SetCell(child, row, col, rowSpan, colSpan). Cell
//  widths and heights are computed as
//
//      cellWidth  = (boundsWidth  - (cols-1) * gap) / cols
//      cellHeight = (boundsHeight - (rows-1) * gap) / rows
//
//  Children spanning multiple cells claim the union rect.
//
////////////////////////////////////////////////////////////////////////////////



class DxuiGridLayout : public IDxuiLayout
{
public:
    DxuiGridLayout (int rows, int cols, float gapDip);

    void  SetCell  (IDxuiControl * child, int row, int col, int rowSpan = 1, int colSpan = 1);

    void  Arrange  (const RECT                          & boundsDip,
                    const DxuiDpiScaler                 & scaler,
                    std::span<IDxuiControl * const>       children) override;

private:
    struct Cell
    {
        int  row     = 0;
        int  col     = 0;
        int  rowSpan = 1;
        int  colSpan = 1;
    };

    int                                       m_rows;
    int                                       m_cols;
    float                                     m_gapDip;
    std::unordered_map<IDxuiControl *, Cell>  m_cells;
};
