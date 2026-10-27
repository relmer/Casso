#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview
//
//  The last frame of the branch of history whose fork row is under the
//  pointer, drawn over the emulated screen until the pointer moves off the
//  row. Nothing is drawn with no picture.
//
////////////////////////////////////////////////////////////////////////////////

class BranchPreview : public IDxuiControl
{
public:
    using Picture = std::shared_ptr<const DxuiIconImage>;

    //  The picture, or null for none; true when it changed.
    bool  SetPicture (Picture picture);

    const Picture &  GetPicture () const { return m_picture; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter      & painter,
                  IDxuiTextRenderer & text,
                  const IDxuiTheme  & theme) override;

private:
    Picture  m_picture;
};
