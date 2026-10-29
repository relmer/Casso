#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip
//
//  A tooltip's text and a swatch beside each line about a color under the
//  pointer (FR-148): the platter's, the strip's and the Nibbles tab's.
//  The kinds of cell in the spot are listed most cells first, then in Timing
//  mode the spot's timing, then the Files overlay's role for the sector,
//  then whether the spot has a find hit.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorTip
{
public:
    void  AddLine  (const std::wstring & line);
    void  AddColor (uint32_t argb, const std::wstring & line);

    const std::wstring               & GetText     () const { return m_text;     }
    const vector<DxuiTooltipSwatch>  & GetSwatches () const { return m_swatches; }

    //  The platter at a quarter track, over `turnSpan` of a turn centered on
    //  `turn`: the stretch one pixel covers there.
    static InspectorTip  BuildPlatterTip (const InspectorViewContext & context, int quarterTrack, double turn, double turnSpan, bool isShowingNibbles);

    //  A nibble of the current track, with the colors of `cellCount` cells
    //  from `firstCell`: the cells one pixel of the strip covers, or the
    //  nibble's own cells in the Nibbles tab.
    static InspectorTip  BuildNibbleTip  (const InspectorViewContext & context, const TrackAnalysis & track, int nibble, uint32_t firstCell, uint32_t cellCount,
                                          bool isTimingMode);

private:
    void  AddKinds    (const InspectorViewContext & context, const TrackAnalysis & track, uint32_t firstCell, uint32_t cellCount, bool isColored);
    void  AddTiming   (const InspectorViewContext & context, const TrackAnalysis & track, uint32_t firstCell, uint32_t cellCount, bool isColored);
    void  AddFileRole (const InspectorViewContext & context, int wholeTrack, const TrackAnalysis & track, int nibble);
    void  AddFindHit  (const InspectorViewContext & context, const TrackAnalysis & track, uint32_t firstCell, uint32_t cellCount);
    void  AddAccess   (const InspectorViewContext & context, int slot, const TrackAnalysis & track, int sector);

    std::wstring               m_text;
    vector<DxuiTooltipSwatch>  m_swatches;
    int                        m_lines = 0;
};
