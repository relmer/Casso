#include "Pch.h"

#include "DxuiTextRowLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRowLayout::GetWrapIndent
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextRowLayout::GetWrapIndent (const DxuiTextRow & row, int avail)
{
    return (row.wrapIndent > 0 && row.wrapIndent * 2 <= avail) ? row.wrapIndent : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRowLayout::GetWeight
//
//  Italic is a bit above the weights, so one value has a weight and the
//  italic style.
//
////////////////////////////////////////////////////////////////////////////////

DxuiFontWeight DxuiTextRowLayout::GetWeight (bool bold, bool italic)
{
    int  weight = bold ? (int) DxuiFontWeight::Bold : (int) DxuiFontWeight::Normal;



    weight |= italic ? ((int) DxuiFontWeight::Italic & ~(int) DxuiFontWeight::Normal) : 0;

    return (DxuiFontWeight) weight;
}
