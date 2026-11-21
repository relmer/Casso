#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/StripPaintScene.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter
//
//  The track strip's nibbles, cells and timing line in the bar, and over
//  the bar the write seam, each sector's bracket and label, and the label
//  of each stretch of one kind.
//
////////////////////////////////////////////////////////////////////////////////

class StripTrackPainter
{
public:
    explicit StripTrackPainter (const StripPaintScene & scene) : m_scene (scene) {}

    float  PaintNibbles       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const;
    void   PaintSeam          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const;
    void   PaintLabels        (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const;
    void   PaintStretchLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, bool isInLabelRow) const;

private:
    void                 PaintCells           (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const;
    void                 PaintTimingLine      (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, float top, float bottom) const;
    float                GetLabelRowClearance (IDxuiTextRenderer & text, const TrackAnalysis & track, const StripGeometry & g, const NibbleSection & section,
                                               float x0, float x1) const;
    float                GetSectorLabel       (IDxuiTextRenderer & text, const AnalyzedSector & sector, float widthPx, std::wstring & outLabel) const;
    static std::wstring  GetNibbleKindLabel   (NibbleKind kind);

    const StripPaintScene  & m_scene;
};
