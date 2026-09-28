#include "Pch.h"

#include "Controllers/JoyportSetting.h"

#include "Config/MachineInputPrefs.h"
#include "Controllers/ControllerTokens.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IsInEffect
//
//  Whether the running machine reads the Joyport: the setting is Atari mode
//  and the machine has the annunciators the Joyport selects its switches
//  with. The //c has none, so there it reads as off whatever the setting,
//  and the setting itself is left as it was for the next machine that can
//  use it.
//
////////////////////////////////////////////////////////////////////////////////

bool JoyportSetting::IsInEffect (GamePortAdapter setting, bool hasAnnunciators)
{
    return hasAnnunciators && setting == GamePortAdapter::SiriusJoyport;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMousePaddleOffered
//
//  Whether the picker lists the mouse as paddle. Not while the Joyport is in
//  effect: an Atari stick has no paddle for the mouse to stand in for, and
//  the paddle inputs read as no paddle connected then.
//
////////////////////////////////////////////////////////////////////////////////

bool JoyportSetting::IsMousePaddleOffered (bool isJoyportInEffect)
{
    return !isJoyportInEffect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveAtLaunch
//
//  The setting a launch starts with. A global token that has been set wins,
//  and an unknown one reads as None, both as they stand. An empty token
//  means the setting has never been set: the launch adopts the value the
//  launched machine saved before the setting went global, which is None on
//  a machine without annunciators, and reports it adopted so the caller
//  saves it and no later launch adopts again.
//
////////////////////////////////////////////////////////////////////////////////

JoyportLaunchSetting JoyportSetting::ResolveAtLaunch (
    const std::string  & globalToken,
    const JsonValue    * launchedUiPrefs,
    bool                 launchedHasAnnunciators)
{
    JoyportLaunchSetting  resolved;



    if (!globalToken.empty())
    {
        resolved.setting = ControllerTokens::GamePortAdapterFromToken (globalToken);
        return resolved;
    }

    resolved.setting   = MachineInputPrefs::ReadGamePortAdapter (launchedUiPrefs, launchedHasAnnunciators);
    resolved.isAdopted = true;
    return resolved;
}
