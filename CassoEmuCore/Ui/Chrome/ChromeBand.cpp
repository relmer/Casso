#include "Pch.h"

#include "Ui/Chrome/ChromeBand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBand::Layout
//
//  The band's bounds are its thickness; there is nothing inside to lay out.
//
////////////////////////////////////////////////////////////////////////////////

void ChromeBand::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UNREFERENCED_PARAMETER (scaler);
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBand::Paint
//
//  Never painted: the band only reserves room for chrome drawn elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

void ChromeBand::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);
}
