#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LegendEntry / LegendSection
//
//  One color and what it means, and a titled group of them with an optional
//  note under the group.
//
////////////////////////////////////////////////////////////////////////////////

struct LegendEntry
{
    uint32_t      argb = 0;
    std::wstring  label;
};


struct LegendSection
{
    std::wstring         title;
    vector<LegendEntry>  entries;
    std::wstring         note;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView
//
//  The legend the "Legend" button opens in a popup (FR-030): the platter's
//  colors for its mode (in Structure mode a swatch per kind, the damaged
//  hatch, the pending pattern and the beyond-reach dimming; in Timing mode
//  fast, nominal and slow with the numeric scale, and a note that only flux
//  tracks record timing), the file map's roles while the Files overlay is
//  on, and the colors of the Sector data bytes.
//
////////////////////////////////////////////////////////////////////////////////

class PlatterLegendView : public InspectorView
{
public:
    static constexpr int  kRowDip     = 20;
    static constexpr int  kColumns    = 3;
    static constexpr int  kWidthDip   = 680;
    static constexpr int  kPadDip     = 12;

    explicit PlatterLegendView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    //  The legend's height for the sections it has now, padding included.
    int   GetHeightDip (const IDxuiTheme & theme) const;

    static vector<LegendEntry>    BuildEntries  (const DiskInspectorPalette & palette, bool isTimingMode, double range);
    static std::wstring           GetNote       (bool isTimingMode);
    static vector<LegendSection>  BuildSections (const InspectorViewContext & context, const IDxuiTheme & theme);
};
