#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLegendView
//
//  The block grid's legend: a swatch and a name for each role a block can
//  have, and the mark for a block that differs, on a card over the grid's
//  top left.
//
////////////////////////////////////////////////////////////////////////////////

class BlockLegendView : public IDxuiControl
{
public:
    static constexpr float  kRowDip    = 20.0f;
    static constexpr float  kPadDip    = 10.0f;
    static constexpr float  kSwatchDip = 12.0f;
    static constexpr float  kTextDip   = 12.0f;
    static constexpr float  kWidthDip  = 230.0f;

    SIZE  GetSizePx (const DxuiDpiScaler & scaler) const;

    //  Whether another image is compared, so the mark for a block that
    //  differs has its line.
    void  SetComparing (bool isComparing) { m_isComparing = isComparing; }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override { SetBounds (boundsPx); m_scaler = scaler; }
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    static const vector<SectorRole> &  GetRoles ();

    DxuiDpiScaler  m_scaler;
    bool           m_isComparing = false;
};
