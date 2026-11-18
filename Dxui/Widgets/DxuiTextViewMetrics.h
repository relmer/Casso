#pragma once

#include "Pch.h"
#include "DxuiTextRow.h"


class IDxuiTextMetricsHost;
class IDxuiTextRenderer;
struct DxuiFontHandle;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewMetrics
//
//  Where a text view's characters fall. Proportional text has each row's
//  cells' character places and each column's widest cell measured once a
//  paint has the renderer; in the fixed-width face each is the cell
//  arithmetic.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewMetrics
{
public:
    using Row = DxuiTextRow;

    explicit DxuiTextViewMetrics (const IDxuiTextMetricsHost & host) : m_host (host) {}

    void   Measure        (IDxuiTextRenderer & text, const DxuiFontHandle & font);
    void   Clear          ()                     { m_charX.clear(); }
    bool   IsMeasured     (size_t rowCount) const { return m_charX.size() == rowCount; }

    float  GetCellX       (const Row & row, int cell) const;
    float  GetCharsPx     (int row, int cell, int from, int to) const;
    int    GetNearestChar (int row, int cell, int from, int to, float x) const;
    float  GetPartPx      (int row, int cell, int cellFrom, int index) const;

private:
    //  Each row's cells' character places, the width of the characters
    //  before each, and each column's widest cell, in pixels. Empty until a
    //  paint measures them.
    const IDxuiTextMetricsHost                  & m_host;
    std::vector<std::vector<std::vector<float>>>  m_charX;
    std::vector<float>                            m_columnPx;
};
