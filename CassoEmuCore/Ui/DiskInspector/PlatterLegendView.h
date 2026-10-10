#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView
//
//  The legend for the platter's mode (FR-030): in Structure mode a swatch
//  per kind, the damaged hatch, the pending pattern and the beyond-reach
//  dimming; in Timing mode fast, nominal and slow with the numeric scale,
//  and a note that only flux tracks record timing.
//
////////////////////////////////////////////////////////////////////////////////

struct LegendEntry
{
    uint32_t      argb = 0;
    std::wstring  label;
};


class PlatterLegendView : public InspectorView
{
public:
    //  Twelve Structure mode entries in four columns take three rows, and
    //  the note takes a fourth.
    static constexpr int  kRowDip    = 18;
    static constexpr int  kColumns   = 4;
    static constexpr int  kRows      = 4;

    explicit PlatterLegendView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    static vector<LegendEntry>  BuildEntries (const DiskInspectorPalette & palette, bool isTimingMode, double range);
    static std::wstring         GetNote      (bool isTimingMode);
};
