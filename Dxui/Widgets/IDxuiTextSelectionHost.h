#pragma once

#include "Pch.h"
#include "DxuiTextRow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextSelectionHost
//
//  The text view a DxuiTextViewSelection selects in: its rows, and each
//  row's text with its cells joined by tabs.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextSelectionHost
{
public:
    using Row = DxuiTextRow;

    virtual                           ~IDxuiTextSelectionHost() = default;

    virtual const std::vector<Row>  & GetSelectionRows () const                  = 0;
    virtual std::wstring              GetRowText       (const Row & row) const   = 0;
    virtual int                       GetRowLength     (const Row & row) const   = 0;
};
