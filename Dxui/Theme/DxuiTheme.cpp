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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTheme::GetContrastingText
//
//  By the fill's luminance, Rec. 709 weights.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTheme::GetContrastingText (uint32_t fill)
{
    float  r = (float) ((fill >> 16) & 0xFF);
    float  g = (float) ((fill >>  8) & 0xFF);
    float  b = (float) ( fill        & 0xFF);



    return (0.2126f * r + 0.7152f * g + 0.0722f * b) > 140.0f ? 0xFF000000u : 0xFFFFFFFFu;
}