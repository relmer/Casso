#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/CommGraph.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommSignalGraph
//
//  The serial or network log's lanes over a span of cycles, a lane to a
//  row under its name: for the serial log each line as a trace between its
//  high and low levels, for the network log each socket's states as bands
//  colored by state, with a tick up for each send and down for each
//  receive. The span's first cycle and its length are along the bottom. A
//  tip at the pointer lists every shown lane's state there.
//
//  Bounds are pixels, like every Dxui control.
//
////////////////////////////////////////////////////////////////////////////////

class CommSignalGraph : public IDxuiControl
{
public:
    static constexpr int  kLaneDip  = 18;
    static constexpr int  kLabelDip = 64;
    static constexpr int  kAxisDip  = 16;

    void  SetLanes (std::vector<CommLane> lanes, std::vector<bool> shown, uint64_t first, uint64_t last, bool isNetwork);

    //  The height for the lanes shown and the axis.
    int   GetHeightDip () const;

    std::optional<uint64_t>  GetCycleAt (POINT point) const;
    bool                     TryGetTipAt (POINT point, std::wstring & text) const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return m_isNetwork ? L"Network graph" : L"Serial graph"; }

    static uint32_t     GetStateColor (Byte status);

private:
    float  GetX (uint64_t cycle) const;
    void   PaintLevels (IDxuiPainter & painter, const CommLane & lane, float top, float bottom, uint32_t color) const;
    void   PaintStates (IDxuiPainter & painter, const CommLane & lane, float top, float bottom, uint32_t tick) const;

    std::vector<CommLane>  m_lanes;
    std::vector<bool>      m_shown;
    uint64_t               m_first     = 0;
    uint64_t               m_last      = 1;
    bool                   m_isNetwork = false;
    DxuiDpiScaler          m_scaler;
};
