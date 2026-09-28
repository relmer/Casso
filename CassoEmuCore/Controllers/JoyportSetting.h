#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"
#include "Core/JsonValue.h"





// What the launch settled the Joyport setting to, and whether it adopted the
// launched machine's saved value, which the caller then saves globally so no
// later launch adopts again.
struct JoyportLaunchSetting
{
    GamePortAdapter  setting   = GamePortAdapter::None;
    bool             isAdopted = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportSetting
//
//  The Joyport setting is global: Apple mode (None) or Atari mode (the Sirius
//  Joyport), the same on every machine that can take one, because whether it
//  should be on follows the game being played rather than the machine. These
//  are the rules every consumer reads it through, so the machine, the
//  controller service, the picker and the Controllers page cannot disagree
//  about what the running machine reads.
//
////////////////////////////////////////////////////////////////////////////////

class JoyportSetting
{
public:

    static bool                  IsInEffect           (GamePortAdapter setting, bool hasAnnunciators);
    static bool                  IsMousePaddleOffered (bool isJoyportInEffect);
    static JoyportLaunchSetting  ResolveAtLaunch      (const std::string & globalToken,
                                                       const JsonValue   * launchedUiPrefs,
                                                       bool                launchedHasAnnunciators);
};
