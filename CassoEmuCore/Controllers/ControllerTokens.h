#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerTokens
//
//  The text forms of controller keys and control identifiers, as preferences
//  store them.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerTokens
{
public:

    static std::string  ModelToToken     (const ControllerModelKey & model);
    static HRESULT      ModelFromToken   (std::string_view token, ControllerModelKey & outModel);
    static std::string  UnitToToken      (const ControllerUnitKey & unit);
    static HRESULT      UnitFromToken    (std::string_view token, ControllerUnitKey & outUnit);
    static std::string  ControlToToken   (const ControlId & control);
    static HRESULT      ControlFromToken (std::string_view token, ControlId & outControl);

private:

    static constexpr const char *  kpszXInputKind      = "xinput";
    static constexpr const char *  kpszDirectInputKind = "dinput";
    static constexpr const char *  kpszGenericModel    = "generic";
    static constexpr const char *  kpszSerialPrefix    = "serial:";
    static constexpr const char *  kpszGuidPrefix      = "guid:";

    static bool  TryParseHexWord      (std::string_view text, Word & outValue);
    static bool  TryParseIndex        (std::string_view text, int & outValue);
    static int   GetControlIndexLimit (ControlKind kind);
};
