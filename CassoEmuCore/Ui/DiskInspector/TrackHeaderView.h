#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackHeaderView
//
//  The selected track in two lines (FR-032): its title, its class in a pill
//  like the disk's format chip and a second pill for a track past the last
//  one the head can be stepped to, then its counts; then its measurements
//  and the note for a track built from sector data or read from a nibble
//  image (FR-003). Text that does not fit is cut from the lines, and the
//  tooltip has all of it.
//
////////////////////////////////////////////////////////////////////////////////

class TrackHeaderView : public InspectorView
{
public:
    static constexpr int  kLineDip  = 20;
    static constexpr int  kLines    = 2;

    explicit TrackHeaderView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;

    static std::wstring  GetFormatNote (DiskFormat format);

private:
    std::wstring  SplitToFit (IDxuiTextRenderer & text, std::wstring & inOutLine, float widthPx) const;
    float         PaintPill  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label, bool isBad, float x, float y) const;

    std::wstring  m_fullText;
};
