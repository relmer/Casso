#include "Pch.h"

#include "Ui/Chrome/BranchPreview.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview::SetPicture
//
////////////////////////////////////////////////////////////////////////////////

bool BranchPreview::SetPicture (Picture picture)
{
    bool  isChanged = picture != m_picture;



    m_picture = std::move (picture);

    SetVisible (m_picture != nullptr && m_picture->width > 0);

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview::Layout
//
//  The emulated screen's rectangle.
//
////////////////////////////////////////////////////////////////////////////////

void BranchPreview::Layout (
    const RECT           & boundsDip,
    const DxuiDpiScaler  & scaler)
{
    UNREFERENCED_PARAMETER (scaler);

    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BranchPreview::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BranchPreview::Paint (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme)
{
    RECT     bounds = GetBounds();
    HRESULT  hr     = S_OK;



    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (theme);

    if (m_picture == nullptr || m_picture->width <= 0)
    {
        return;
    }

    hr = text.DrawFramebuffer (m_picture->bgraPremul.data(), m_picture->width, m_picture->height,
                               (float) bounds.left, (float) bounds.top,
                               (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top));
    IGNORE_RETURN_VALUE (hr, S_OK);
}
