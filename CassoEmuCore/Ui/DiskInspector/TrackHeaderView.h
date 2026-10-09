#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackHeaderView
//
//  The selected track's title and class, its counts, and its measurements,
//  one line each (FR-032), with the note for a track built from sector data
//  or read from a nibble image (FR-003).
//
////////////////////////////////////////////////////////////////////////////////

class TrackHeaderView : public InspectorView
{
public:
    static constexpr int  kLineDip  = 20;
    static constexpr int  kLines    = 4;

    explicit TrackHeaderView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    static std::wstring  GetFormatNote (DiskFormat format);
};
