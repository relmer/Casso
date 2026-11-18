#pragma once

#include "Pch.h"
#include "DxuiTextRow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextMetricsHost
//
//  The text view a DxuiTextViewMetrics measures: its rows, its face's
//  advance and column gaps, and where each cell starts in a row's text.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextMetricsHost
{
public:
    using Row = DxuiTextRow;

    virtual                                   ~IDxuiTextMetricsHost() = default;

    virtual const std::vector<Row>          & GetMetricsRows        () const                     = 0;
    virtual bool                              IsMetricsProportional () const                     = 0;
    virtual float                             GetMetricsAdvance     () const                     = 0;
    virtual int                               GetMetricsColumnGap   (int column) const           = 0;
    virtual int                               GetCellStart          (const Row & row, int cell) const = 0;
    virtual int                               GetCellBase           (const Row & row, int cell) const = 0;
    virtual std::vector<uint8_t>              GetRowStyles          (const Row & row) const      = 0;
};
