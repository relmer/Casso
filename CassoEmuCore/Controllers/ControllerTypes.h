#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerTypes
//
//  Plain data shared by every part of game-controller support: how a
//  controller is recognized (model and unit keys), how one of its controls is
//  identified, one normalized reading of all its controls, and what one input
//  source asks of the game port. The rules that use these types live in the
//  classes that own them, not here.
//
////////////////////////////////////////////////////////////////////////////////

enum class ControllerKind
{
    XInput,
    DirectInput,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerFormFactor
//
//  What the device looks like in the hand, which is what the picker draws.
//  Separate from ControllerKind, which says which API reads it: an Xbox pad
//  and a USB gamepad are one shape read two ways, and a flight stick and a
//  gamepad are two shapes read the same way.
//
//  DirectInput reports this in DIDEVICEINSTANCE::dwDevType; XInput needs no
//  detection, being a gamepad by definition.
//
////////////////////////////////////////////////////////////////////////////////

enum class ControllerFormFactor
{
    Gamepad,
    Joystick,   // flight sticks included: one stick with a base
    Wheel
};





enum class ControllerUnitSource
{
    None,
    Serial,
    InstanceGuid,
};





enum class ControlKind
{
    Axis,
    Trigger,
    Button,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelKey
//
//  Every unit of one controller model shares this key. An XInput key always
//  carries vendor and product 0: every Xbox-class controller shares one key,
//  because XInput reports them all through one fixed layout and because the
//  same controller reports different product IDs on USB and on Bluetooth. Its
//  real IDs reach the user only through ControllerDeviceInfo::description.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerModelKey
{
    ControllerKind  kind      = ControllerKind::DirectInput;
    Word            vendorId  = 0;
    Word            productId = 0;

    bool operator== (const ControllerModelKey &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerUnitKey
//
//  One physical controller. unitId is empty for XInput controllers, which are
//  recognized by model only; for DirectInput it holds the HID serial number
//  when the device reports one, otherwise its instance GUID in text form.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerUnitKey
{
    ControllerModelKey    model;
    std::string           unitId;
    ControllerUnitSource  source = ControllerUnitSource::None;

    bool operator== (const ControllerUnitKey &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControlId
//
//  One control on a controller. index is the axis (0-7: X, Y, Z, Rx, Ry, Rz,
//  slider 0, slider 1), trigger (0-1), button (0-127) or hat (0-3, for the
//  four D-pad kinds).
//
////////////////////////////////////////////////////////////////////////////////

struct ControlId
{
    ControlKind  kind  = ControlKind::Button;
    int          index = 0;

    bool operator== (const ControlId &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerSample
//
//  One reading of every control, already normalized by the backend that read
//  it: axes in [-1, 1], triggers in [0, 1], and each hat as a set of D-pad
//  direction bits. A sample with connected false is a rest sample.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerSample
{
    static constexpr int   kAxisCount    = 8;
    static constexpr int   kTriggerCount = 2;
    static constexpr int   kButtonCount  = 128;
    static constexpr int   kHatCount     = 4;

    static constexpr Byte  kHatUp        = 0x01;
    static constexpr Byte  kHatDown      = 0x02;
    static constexpr Byte  kHatLeft      = 0x04;
    static constexpr Byte  kHatRight     = 0x08;

    std::array<float, kAxisCount>     axes      = {};
    std::array<float, kTriggerCount>  triggers  = {};
    std::bitset<kButtonCount>         buttons;
    std::array<Byte, kHatCount>       hats      = {};
    bool                              connected = false;

    bool operator== (const ControllerSample &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerDeviceInfo
//
//  One attached controller as enumeration reports it.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerDeviceInfo
{
    static constexpr int  kNoXInputSlot = -1;

    ControllerUnitKey       unit;
    std::wstring            description;
    int                     xinputSlot  = kNoXInputSlot;
    ControllerFormFactor    formFactor  = ControllerFormFactor::Gamepad;
    std::vector<ControlId>  controls;
};





////////////////////////////////////////////////////////////////////////////////
//
//  GamePortContribution
//
//  What one input source asks of the game port. An absent paddle means the
//  source does not drive the axes; buttons are PB0, PB1 and PB2.
//
////////////////////////////////////////////////////////////////////////////////

struct GamePortContribution
{
    static constexpr int  kButtonCount = 3;

    std::optional<std::array<Byte, 2>>  paddle;
    std::bitset<kButtonCount>           buttons;

    bool operator== (const GamePortContribution &) const = default;
};
