#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView
//
//  The Sector data tab: the selected sector's header, then its 256 bytes as
//  16 rows of 16 with hex offsets and a text column, zero bytes and bytes of
//  $80 or more in their own colors, and bad data marked (FR-040). Read-only;
//  editing comes with the sector editor.
//
////////////////////////////////////////////////////////////////////////////////

class SectorByteView : public InspectorView
{
public:
    static constexpr int  kRowDip      = 18;
    static constexpr int  kHeaderDip   = 22;
    static constexpr int  kBytesPerRow = 16;

    explicit SectorByteView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    //  The text column's character for a byte: high bit masked, "." for
    //  anything not printable.
    static wchar_t  GetTextChar (Byte value);

private:
    float  PaintHeader (IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track, float y);
    void   PaintBytes  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track, float y);
};
