#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControlLabels
//
//  What a control is called on screen. An Xbox-class controller's controls
//  carry the names printed on it; a DirectInput device's carry the names
//  DirectInput gives its axes, and numbers for everything else, counted from
//  one the way a device's own manual counts them.
//
//  NEVER EMPTY. A control past the ones this class knows by name still gets a
//  numbered label, because a blank row in a mapping list reads as a rendering
//  fault rather than as a control.
//
////////////////////////////////////////////////////////////////////////////////

class ControlLabels
{
public:

    static std::wstring  For      (ControllerKind kind, const ControlId & control);

    // The Segoe MDL2 Assets glyph an Xbox controller's control is drawn with,
    // or empty for a control, or a controller, that has none.
    static std::wstring  GlyphFor (ControllerKind kind, const ControlId & control);

private:

    static std::wstring  ForXInput      (const ControlId & control);
    static std::wstring  ForDirectInput (const ControlId & control);
    static std::wstring  Numbered       (const wchar_t * pszKind, int index);
    static std::wstring  DpadDirection  (ControlKind kind);
};
