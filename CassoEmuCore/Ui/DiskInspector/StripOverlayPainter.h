#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/StripPaintScene.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter
//
//  What the track strip draws over its nibbles: the Files band and the
//  selected file's dimming and marquee, the dividers, the differences, the
//  find hits, the selection and its shimmer, and the drive's head.
//
////////////////////////////////////////////////////////////////////////////////

class StripOverlayPainter
{
public:
    explicit StripOverlayPainter (const StripPaintScene & scene) : m_scene (scene) {}

    void  PaintFiles     (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const;
    void  DimOutsideFile (IDxuiPainter & painter, const vector<StripSegment> & parts) const;
    void  PaintMarquee   (IDxuiPainter & painter, const vector<StripSegment> & parts) const;
    void  PaintDividers  (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g, float top, float height) const;
    void  PaintDiffs     (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g) const;
    void  PaintHits      (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g) const;
    void  PaintSelection (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g) const;
    void  PaintShimmer   (IDxuiPainter & painter, const StripGeometry & g) const;
    void  PaintHead      (IDxuiPainter & painter, const StripGeometry & g) const;

    //  True when the head is over the record of the selected quarter track,
    //  on side A, and what the head is doing.
    static bool     IsHeadOnTrack (const InspectorViewContext & context, int side);
    static LPCWSTR  GetHeadLabel  (HeadActivity activity);

private:
    const StripPaintScene  & m_scene;
};
