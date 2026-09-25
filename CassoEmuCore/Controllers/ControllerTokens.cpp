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
//  "xinput" for every Xbox-class controller, "dinput:044f:b10a" for the rest.
//  XInput controllers share one key because XInput reports them all through
//  one fixed layout, and because the same controller reports different
//  product IDs on USB and on Bluetooth.
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllerTokens::ModelToToken (const ControllerModelKey & model)
{
    bool         isXInput = model.kind == ControllerKind::XInput;
    std::string  token;



    if (isXInput)
    {
        token = kpszXInputKind;
    }
    else
    {
        token = std::format ("{}:{:04x}:{:04x}", kpszDirectInputKind, model.vendorId, model.productId);
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
    HRESULT             hr         = S_OK;
    ControllerModelKey  model;
    size_t              kindEnd    = token.find (':');
    std::string_view    kindText;
    std::string_view    idText;
    size_t              vendorEnd  = std::string_view::npos;
    bool                isXInput   = token == kpszXInputKind;
    bool                isDirect   = false;
    bool                hasVendor  = false;
    bool                hasProduct = false;



    if (isXInput)
    {
        model.kind = ControllerKind::XInput;
        outModel   = model;
        return hr;
    }

    CBREx (kindEnd != std::string_view::npos, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    kindText = token.substr (0, kindEnd);
    idText   = token.substr (kindEnd + 1);
    isDirect = kindText == kpszDirectInputKind;

    // An XInput token carries no IDs at all (FR-018a), so "xinput:045e:0b13"
    // is not a key this build ever wrote and is refused rather than read as
    // some other controller.
    CBREx (isDirect, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    model.kind = ControllerKind::DirectInput;
    vendorEnd  = idText.find (':');

    CBREx (vendorEnd != std::string_view::npos, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hasVendor  = TryParseHexWord (idText.substr (0, vendorEnd), model.vendorId);
    hasProduct = TryParseHexWord (idText.substr (vendorEnd + 1), model.productId);

    CBREx (hasVendor && hasProduct, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outModel = model;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnitToToken
//
//  The model token, then "/serial:<id>" or "/guid:<id>" for a DirectInput
//  unit with an identity, or "/slot:<n>" for an XInput unit. The model token
//  itself never carries the slot, so profiles and deadzone stay shared by
//  every Xbox-class controller.
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
    else if (unit.source == ControllerUnitSource::XInputSlot)
    {
        token += std::format ("/{}{}", kpszSlotPrefix, unit.unitId);
    }
    else if (unit.source == ControllerUnitSource::XInputProduct)
    {
        token += std::format ("/{}{}", kpszProductPrefix, unit.unitId);
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnitFromToken
//
//  The model part ends at the first slash: model tokens never contain one,
//  while a serial number may. A bare "xinput" token, which is what every
//  preferences file written before XInput units carried a slot holds, parses
//  as an XInput unit with no slot.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ControllerTokens::UnitFromToken (std::string_view token, ControllerUnitKey & outUnit)
{
    HRESULT             hr           = S_OK;
    ControllerUnitKey   unit;
    size_t              slash        = token.find ('/');
    std::string_view    identity;
    std::string_view    serialPrefix (kpszSerialPrefix);
    std::string_view    guidPrefix   (kpszGuidPrefix);
    std::string_view    slotPrefix   (kpszSlotPrefix);
    std::string_view    productPrefix (kpszProductPrefix);
    size_t              prefixLength = 0;
    bool                isXInput     = false;
    bool                isSerial     = false;
    bool                isGuid       = false;
    bool                isSlot       = false;
    bool                isProduct    = false;
    bool                hasIdentity  = false;
    bool                hasSlot      = false;
    bool                hasProduct   = false;
    int                 slot         = 0;



    hr = ModelFromToken (token.substr (0, slash), unit.model);
    CHR (hr);

    if (slash != std::string_view::npos)
    {
        identity = token.substr (slash + 1);
        isXInput = unit.model.kind == ControllerKind::XInput;
        isSlot    = isXInput  && identity.starts_with (slotPrefix);
        isProduct = isXInput  && identity.starts_with (productPrefix);
        isSerial  = !isXInput && identity.starts_with (serialPrefix);
        isGuid    = !isXInput && identity.starts_with (guidPrefix);

        if (isSlot)
        {
            prefixLength = slotPrefix.size();
            unit.source  = ControllerUnitSource::XInputSlot;
        }
        else if (isProduct)
        {
            prefixLength = productPrefix.size();
            unit.source  = ControllerUnitSource::XInputProduct;
        }
        else if (isSerial)
        {
            prefixLength = serialPrefix.size();
            unit.source  = ControllerUnitSource::Serial;
        }
        else if (isGuid)
        {
            prefixLength = guidPrefix.size();
            unit.source  = ControllerUnitSource::InstanceGuid;
        }

        hasIdentity = (isSlot || isProduct || isSerial || isGuid) && identity.size() > prefixLength;

        CBREx (hasIdentity, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        unit.unitId = std::string (identity.substr (prefixLength));

        if (isSlot)
        {
            hasSlot = TryParseIndex (unit.unitId, slot);

            CBREx (hasSlot && slot < kXInputSlotLimit, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        }

        if (isProduct)
        {
            hasProduct = IsProductIdentity (unit.unitId);

            CBREx (hasProduct, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        }
    }

    outUnit = unit;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsProductIdentity
//
//  Four hex digits of vendor, four of product, and an ordinal from 2 up for a
//  second unit of the same product. An ordinal of 1 is refused rather than
//  read as the first unit, so each unit has exactly one spelling.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerTokens::IsProductIdentity (std::string_view text)
{
    Word    vendor   = 0;
    Word    product  = 0;
    int     ordinal  = 0;
    size_t  hexWidth = 4;



    if (text.size() < hexWidth * 2 + 1 || text[hexWidth] != ':')
    {
        return false;
    }

    if (!TryParseHexWord (text.substr (0, hexWidth), vendor) ||
        !TryParseHexWord (text.substr (hexWidth + 1, hexWidth), product))
    {
        return false;
    }

    if (text.size() == hexWidth * 2 + 1)
    {
        return true;
    }

    if (text[hexWidth * 2 + 1] != ':')
    {
        return false;
    }

    return TryParseIndex (text.substr (hexWidth * 2 + 2), ordinal) && ordinal >= 2;
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
//  GamePortAdapterToToken
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllerTokens::GamePortAdapterToToken (GamePortAdapter adapter)
{
    std::string  token = kpszAdapterNone;



    if (adapter == GamePortAdapter::SiriusJoyport)
    {
        token = kpszAdapterSiriusJoyport;
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GamePortAdapterFromToken
//
//  Anything but a known adapter, including a missing value, is None: a stale
//  or hand-edited preference leaves the game port as it has always been
//  rather than attaching something the user did not choose.
//
////////////////////////////////////////////////////////////////////////////////

GamePortAdapter ControllerTokens::GamePortAdapterFromToken (std::string_view token)
{
    GamePortAdapter  adapter = GamePortAdapter::None;



    if (token == kpszAdapterSiriusJoyport)
    {
        adapter = GamePortAdapter::SiriusJoyport;
    }

    return adapter;
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
