#include "Pch.h"

#include "Controllers/ControllerTokens.h"





struct ControlKindName
{
    ControlKind    kind;
    const char   * pszName;
};

static constexpr ControlKindName  s_kControlKindNames[] =
{
    { ControlKind::Axis,      "axis"       },
    { ControlKind::Trigger,   "trigger"    },
    { ControlKind::Button,    "button"     },
    { ControlKind::DpadUp,    "dpad-up"    },
    { ControlKind::DpadDown,  "dpad-down"  },
    { ControlKind::DpadLeft,  "dpad-left"  },
    { ControlKind::DpadRight, "dpad-right" },
};





////////////////////////////////////////////////////////////////////////////////
//
//  ModelToToken
//
//  "xinput:045e:0b13", "dinput:044f:b10a", or "xinput:generic" for an Xbox
//  model whose hardware IDs could not be read.
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllerTokens::ModelToToken (const ControllerModelKey & model)
{
    bool          isXInput  = model.kind == ControllerKind::XInput;
    const char  * pszKind   = isXInput ? kpszXInputKind : kpszDirectInputKind;
    bool          isGeneric = isXInput && model.vendorId == 0 && model.productId == 0;
    std::string   token;



    if (isGeneric)
    {
        token = std::format ("{}:{}", pszKind, kpszGenericModel);
    }
    else
    {
        token = std::format ("{}:{:04x}:{:04x}", pszKind, model.vendorId, model.productId);
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ModelFromToken
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ControllerTokens::ModelFromToken (std::string_view token, ControllerModelKey & outModel)
{
    HRESULT             hr          = S_OK;
    ControllerModelKey  model;
    size_t              kindEnd     = token.find (':');
    std::string_view    kindText;
    std::string_view    idText;
    size_t              vendorEnd   = std::string_view::npos;
    bool                isXInput    = false;
    bool                isDirect    = false;
    bool                isGeneric   = false;
    bool                hasVendor   = false;
    bool                hasProduct  = false;



    CBREx (kindEnd != std::string_view::npos, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    kindText = token.substr (0, kindEnd);
    idText   = token.substr (kindEnd + 1);
    isXInput = kindText == kpszXInputKind;
    isDirect = kindText == kpszDirectInputKind;

    CBREx (isXInput || isDirect, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    model.kind = isXInput ? ControllerKind::XInput : ControllerKind::DirectInput;
    isGeneric  = isXInput && idText == kpszGenericModel;

    if (!isGeneric)
    {
        vendorEnd = idText.find (':');

        CBREx (vendorEnd != std::string_view::npos, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        hasVendor  = TryParseHexWord (idText.substr (0, vendorEnd), model.vendorId);
        hasProduct = TryParseHexWord (idText.substr (vendorEnd + 1), model.productId);

        CBREx (hasVendor && hasProduct, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    outModel = model;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnitToToken
//
//  The model token, then "/serial:<id>" or "/guid:<id>" for a DirectInput
//  unit with an identity. XInput units have none.
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllerTokens::UnitToToken (const ControllerUnitKey & unit)
{
    std::string  token = ModelToToken (unit.model);



    if (unit.source == ControllerUnitSource::Serial)
    {
        token += std::format ("/{}{}", kpszSerialPrefix, unit.unitId);
    }
    else if (unit.source == ControllerUnitSource::InstanceGuid)
    {
        token += std::format ("/{}{}", kpszGuidPrefix, unit.unitId);
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnitFromToken
//
//  The model part ends at the first slash: model tokens never contain one,
//  while a serial number may.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ControllerTokens::UnitFromToken (std::string_view token, ControllerUnitKey & outUnit)
{
    HRESULT             hr          = S_OK;
    ControllerUnitKey   unit;
    size_t              slash       = token.find ('/');
    std::string_view    identity;
    std::string_view    serialPrefix (kpszSerialPrefix);
    std::string_view    guidPrefix   (kpszGuidPrefix);
    bool                isSerial    = false;
    bool                isGuid      = false;
    bool                hasIdentity = false;



    hr = ModelFromToken (token.substr (0, slash), unit.model);
    CHR (hr);

    if (slash != std::string_view::npos)
    {
        CBREx (unit.model.kind == ControllerKind::DirectInput, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        identity    = token.substr (slash + 1);
        isSerial    = identity.starts_with (serialPrefix);
        isGuid      = identity.starts_with (guidPrefix);
        hasIdentity = (isSerial && identity.size() > serialPrefix.size()) ||
                      (isGuid   && identity.size() > guidPrefix.size());

        CBREx (hasIdentity, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        unit.source = isSerial ? ControllerUnitSource::Serial : ControllerUnitSource::InstanceGuid;
        unit.unitId = std::string (identity.substr (isSerial ? serialPrefix.size() : guidPrefix.size()));
    }

    outUnit = unit;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControlToToken
//
//  "axis:1", "button:3", "dpad-left:0" and so on.
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllerTokens::ControlToToken (const ControlId & control)
{
    const char  * pszName = nullptr;



    for (const ControlKindName & entry : s_kControlKindNames)
    {
        if (entry.kind == control.kind)
        {
            pszName = entry.pszName;
            break;
        }
    }

    return std::format ("{}:{}", pszName != nullptr ? pszName : "", control.index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControlFromToken
//
//  Rejects an unknown kind and an index outside what a sample can hold for
//  that kind, so a stored binding can never address a control that does not
//  exist.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ControllerTokens::ControlFromToken (std::string_view token, ControlId & outControl)
{
    HRESULT           hr        = S_OK;
    ControlId         control;
    size_t            colon     = token.find (':');
    std::string_view  name;
    bool              isKnown   = false;
    bool              hasIndex  = false;
    int               limit     = 0;



    CBREx (colon != std::string_view::npos, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    name = token.substr (0, colon);

    for (const ControlKindName & entry : s_kControlKindNames)
    {
        if (name == entry.pszName)
        {
            control.kind = entry.kind;
            isKnown      = true;
            break;
        }
    }

    CBREx (isKnown, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hasIndex = TryParseIndex (token.substr (colon + 1), control.index);
    limit    = GetControlIndexLimit (control.kind);

    CBREx (hasIndex && control.index < limit, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outControl = control;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParseHexWord
//
//  Exactly four hex digits, either case.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerTokens::TryParseHexWord (std::string_view text, Word & outValue)
{
    constexpr size_t  kHexDigits  = 4;
    constexpr int     kHexBase    = 16;
    constexpr int     kDigitAlpha = 10;
    int               value       = 0;
    bool              isValid     = text.size() == kHexDigits;



    for (size_t i = 0; isValid && i < text.size(); i++)
    {
        char  ch    = text[i];
        int   digit = -1;

        if (ch >= '0' && ch <= '9')
        {
            digit = ch - '0';
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            digit = ch - 'a' + kDigitAlpha;
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            digit = ch - 'A' + kDigitAlpha;
        }

        isValid = digit >= 0;
        value   = value * kHexBase + digit;
    }

    if (isValid)
    {
        outValue = static_cast<Word> (value);
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParseIndex
//
//  One to three decimal digits, no sign.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerTokens::TryParseIndex (std::string_view text, int & outValue)
{
    constexpr size_t  kMaxDigits   = 3;
    constexpr int     kDecimalBase = 10;
    int               value        = 0;
    bool              isValid      = !text.empty() && text.size() <= kMaxDigits;



    for (size_t i = 0; isValid && i < text.size(); i++)
    {
        isValid = text[i] >= '0' && text[i] <= '9';
        value   = value * kDecimalBase + (text[i] - '0');
    }

    if (isValid)
    {
        outValue = value;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetControlIndexLimit
//
////////////////////////////////////////////////////////////////////////////////

int ControllerTokens::GetControlIndexLimit (ControlKind kind)
{
    int  limit = 0;



    switch (kind)
    {
        case ControlKind::Axis:
            limit = ControllerSample::kAxisCount;
            break;

        case ControlKind::Trigger:
            limit = ControllerSample::kTriggerCount;
            break;

        case ControlKind::Button:
            limit = ControllerSample::kButtonCount;
            break;

        case ControlKind::DpadUp:
        case ControlKind::DpadDown:
        case ControlKind::DpadLeft:
        case ControlKind::DpadRight:
            limit = ControllerSample::kHatCount;
            break;

        default:
            break;
    }

    return limit;
}
