#pragma once

#include "Pch.h"
#include "Widgets/HexView/DxuiHexViewTypes.h"


class IDxuiHexSource;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiHexEditHost
//
//  The hex view a DxuiHexSourceEdit types values into: its source, how its
//  values are shown, the column and caret, and the caret's move after a
//  write.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiHexEditHost
{
public:
    virtual                                ~IDxuiHexEditHost() = default;

    virtual const IDxuiHexSource         * GetEditSource     () const          = 0;
    virtual bool                           CanEditValues     () const          = 0;
    virtual DxuiHexViewTypes::Column       GetEditColumn     () const          = 0;
    virtual DxuiHexViewTypes::ValueFormat  GetEditFormat     () const          = 0;
    virtual size_t                         GetEditGroupBytes () const          = 0;
    virtual DxuiHexViewTypes::TextEncoding GetEditEncoding   () const          = 0;
    virtual uint64_t                       GetEditCaret      () const          = 0;
    virtual uint64_t                       SnapEditToValue   (uint64_t offset) const = 0;
    virtual void                           MoveEditCaretTo   (uint64_t offset) = 0;
};
