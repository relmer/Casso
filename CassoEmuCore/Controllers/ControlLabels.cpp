#include "Pch.h"

#include "Controllers/ControlLabels.h"

#include "Controllers/XInputSampleDecoder.h"





// XInputSampleDecoder's button order: A, B, X, Y, LB, RB, Back, Start, then
// the two stick clicks.
static constexpr const wchar_t *  s_kXInputButtonNames[] =
{
    L"A", L"B", L"X", L"Y", L"Left bumper (LB)", L"Right bumper (RB)", L"Back", L"Start",
    L"Left stick click (LS)", L"Right stick click (RS)",
};

// DIJOYSTATE2's axis order: X, Y, Z, the three rotations, then two sliders.
static constexpr const wchar_t *  s_kDirectInputAxisNames[] =
{
    L"X axis", L"Y axis", L"Z axis", L"X rotation", L"Y rotation", L"Z rotation", L"Slider 1", L"Slider 2",
};





////////////////////////////////////////////////////////////////////////////////
//
//  For
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControlLabels::For (ControllerKind kind, const ControlId & control)
{
    if (kind == ControllerKind::XInput)
    {
        return ForXInput (control);
    }

    return ForDirectInput (control);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForXInput
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControlLabels::ForXInput (const ControlId & control)
{
    switch (control.kind)
    {
        case ControlKind::Axis:
            if (control.index == XInputSampleDecoder::kLeftStickX)  { return L"Left stick X";  }
            if (control.index == XInputSampleDecoder::kLeftStickY)  { return L"Left stick Y";  }
            if (control.index == XInputSampleDecoder::kRightStickX) { return L"Right stick X"; }
            if (control.index == XInputSampleDecoder::kRightStickY) { return L"Right stick Y"; }
            return Numbered (L"Axis", control.index);

        case ControlKind::Trigger:
            if (control.index == 0) { return L"Left trigger (LT)";  }
            if (control.index == 1) { return L"Right trigger (RT)"; }
            return Numbered (L"Trigger", control.index);

        case ControlKind::Button:
            if (control.index >= 0 && control.index < (int) std::size (s_kXInputButtonNames))
            {
                return s_kXInputButtonNames[control.index];
            }

            return Numbered (L"Button", control.index);

        default:
            break;
    }

    return L"D-pad " + DpadDirection (control.kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForDirectInput
//
//  A device with one hat calls it the D-pad, which is what it is on nearly
//  every gamepad; a second hat and beyond are numbered.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControlLabels::ForDirectInput (const ControlId & control)
{
    switch (control.kind)
    {
        case ControlKind::Axis:
            if (control.index >= 0 && control.index < (int) std::size (s_kDirectInputAxisNames))
            {
                return s_kDirectInputAxisNames[control.index];
            }

            return Numbered (L"Axis", control.index);

        case ControlKind::Trigger:
            return Numbered (L"Trigger", control.index);

        case ControlKind::Button:
            return Numbered (L"Button", control.index);

        default:
            break;
    }

    if (control.index == 0)
    {
        return L"D-pad " + DpadDirection (control.kind);
    }

    return Numbered (L"Hat", control.index) + L" " + DpadDirection (control.kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Numbered
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControlLabels::Numbered (const wchar_t * pszKind, int index)
{
    return std::format (L"{} {}", pszKind, index + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DpadDirection
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControlLabels::DpadDirection (ControlKind kind)
{
    switch (kind)
    {
        case ControlKind::DpadUp:    return L"up";
        case ControlKind::DpadDown:  return L"down";
        case ControlKind::DpadLeft:  return L"left";
        case ControlKind::DpadRight: return L"right";
        default:                     return L"";
    }
}
