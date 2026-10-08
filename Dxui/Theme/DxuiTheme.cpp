#include "Pch.h"
#include "DxuiTheme.h"
#include "Render/DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTheme::GetUiFace
//
//  Windows 11's own apps set their text in Segoe UI Variable, whose Text
//  optical size is the one for body text; Windows 10 has only Segoe UI. Which
//  one this machine has does not change while it runs, so it is asked once.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiTheme::GetUiFace()
{
    static const bool  s_variable = DxuiTextRenderer::IsFontFamilyInstalled (kVariableFace);



    return s_variable ? kVariableFace : kBodyFace;
}
