#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigChoice
//
//  The text a session uses for a machine's configuration, and why.
//
//  text           The disk text, or the embedded text when useEmbedded is set.
//  useEmbedded    Set when the disk stamp is higher than the embedded stamp.
//  diskStamp      The $cassoMachineVersion of the disk text, 0 when unstamped.
//  embeddedStamp  The $cassoMachineVersion of the embedded text.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineConfigChoice
{
    string  text;
    bool    useEmbedded   = false;
    int     diskStamp     = 0;
    int     embeddedStamp = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigSource
//
//  Chooses between a machine config on disk and the one embedded in this build.
//  A file written by a newer build can have settings this build can't make,
//  such as a card it has no firmware for, so a disk stamp higher than the
//  embedded stamp selects the embedded text for that session.
//
//  Neither function writes the file. Replacing a stale file is
//  AssetBootstrap::EnsureMachineConfigs, through MachineConfigUpgrade::Plan.
//
////////////////////////////////////////////////////////////////////////////////

class MachineConfigSource
{
public:
    static MachineConfigChoice  ChooseText     (const string & diskText, const string & embeddedText);
    static HRESULT              ReadConfigText (HINSTANCE      hInstance,
                                                const wstring & machineName,
                                                const fs::path & diskPath,
                                                string         & outText);

private:
    static HRESULT              GetEmbeddedText (HINSTANCE       hInstance,
                                                 const wstring & machineName,
                                                 string        & outText);
};
