#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceAttachments
//
//  Puts the images from --hd and --smartport on the units of a machine's
//  config, for one run. Nothing is saved.
//
////////////////////////////////////////////////////////////////////////////////

class BlockDeviceAttachments
{
public:
    using Attachment = CommandLineOptions::EmulatorOptions::BlockAttachment;

    //  Fails with the reason in the fixed error layout when a slot has no
    //  block device card, a unit is not one the card has, or the machine has
    //  no disk port, and then nothing is applied. With a resolver, the ROMs
    //  of the drives the attachments add are looked up too, since the
    //  config's load didn't include those drives.
    static HRESULT  Apply (const std::vector<Attachment>             & hd,
                           const std::vector<Attachment>             & smartPort,
                           MachineConfig                             & ioConfig,
                           std::string                               & outError,
                           const std::vector<fs::path>               & searchPaths = {},
                           const MachineConfigLoader::FileResolver   & resolver    = {});

private:
    static HRESULT  ApplyHd        (const Attachment & attachment, MachineConfig & ioConfig, std::string & outError);
    static HRESULT  ApplySmartPort (const Attachment & attachment, MachineConfig & ioConfig, std::string & outError);
};
