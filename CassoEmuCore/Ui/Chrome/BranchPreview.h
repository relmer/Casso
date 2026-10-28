#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview
//
//  The last frame of the branch of history whose fork row is under the
//  pointer, drawn over the emulated screen until the pointer moves off the
//  row. Nothing is drawn with no picture. While the rewind bar's flyout is
//  open, the frame is placed beside the flyout and above the bar, so neither
//  covers it; it covers the whole screen otherwise.
//
////////////////////////////////////////////////////////////////////////////////

class BranchPreview : public IDxuiControl
{
public:
    using Picture = std::shared_ptr<const DxuiIconImage>;

    //  The picture, or null for none; true when it changed.
    bool  SetPicture (Picture picture);

    const Picture &  GetPicture () const { return m_picture; }

    //  The rectangle for a picture widthOverHeight wide beside a flyout, in
    //  the screen above the bar (area): on the side of the flyout with more
    //  room, as large as fits kGapDp from the flyout and the area's edges, and
    //  level with the bottom of the area. With too little room on either
    //  side, the largest the area fits.
    static RECT  FitBeside (const RECT & area, const RECT & flyout, float widthOverHeight, int gapPx);

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter      & painter,
                  IDxuiTextRenderer & text,
                  const IDxuiTheme  & theme) override;

    static constexpr int    kGapDp        = 8;
    static constexpr float  kMinSideShare = 0.25f;   // of the area's width, for a side to take the picture

private:
    static RECT  FitInside (const RECT & room, float widthOverHeight);

    Picture  m_picture;
};
