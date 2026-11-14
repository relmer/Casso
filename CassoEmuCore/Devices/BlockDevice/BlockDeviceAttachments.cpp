#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Devices/BlockDevice/BlockDeviceAttachments.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apply
//
//  The attachments go on a copy, so the config changes only when all apply.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceAttachments::Apply (
    const std::vector<Attachment>  & hd,
    const std::vector<Attachment>  & smartPort,
    MachineConfig                  & ioConfig,
    std::string                    & outError)
{
    HRESULT        hr     = S_OK;
    MachineConfig  config = ioConfig;



    for (const Attachment & attachment : hd)
    {
        hr = ApplyHd (attachment, config, outError);
        CHR (hr);
    }

    for (const Attachment & attachment : smartPort)
    {
        hr = ApplySmartPort (attachment, config, outError);
        CHR (hr);
    }

    ioConfig = std::move (config);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyHd
//
//  The card's first unit, without a unit number, is the lowest SCSI ID with a
//  drive, or ID 6 when none has one, and the first drive on any other card.
//  A unit with no drive gets the card's drive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceAttachments::ApplyHd (const Attachment & attachment, MachineConfig & ioConfig, std::string & outError)
{
    constexpr int          kDefaultScsiId = 6;
    HRESULT                hr             = S_OK;
    SlotConfig           * slot           = nullptr;
    const BlockCardSpec  * card           = nullptr;
    int                    unit           = attachment.unit;
    std::string            present = "nothing";



    for (SlotConfig & entry : ioConfig.slots)
    {
        if (entry.slot == attachment.slot && entry.enabled)
        {
            slot    = &entry;
            card    = BlockDeviceCatalog::FindCard (entry.device);
            present = entry.device.empty()           ? present
                    : (entry.device == kpszDiskIiDevice) ? std::string ("a Disk II")
                                                         : "a " + entry.device;
        }
    }

    CBRF (card != nullptr,
          outError = std::format ("Error: no block device in slot {}\n"
                                  "       The --hd flag attaches to a SCSI, ProFile or UniDisk 3.5\n"
                                  "       interface card. Slot {} has {}.",
                                  attachment.slot, attachment.slot, present));

    if (unit < 0)
    {
        unit = (std::string (card->device) == kpszAppleScsiCardDevice) ? kDefaultScsiId : 0;

        for (size_t id = 0; id < slot->ports.size() && std::string (card->device) == kpszAppleScsiCardDevice; id++)
        {
            if (!slot->ports[id].device.empty())
            {
                unit = (int) id;
                break;
            }
        }
    }

    CBRF (unit < card->portCount,
          outError = std::format ("Error: no unit {} on the card in slot {}\n"
                                  "       The {} has units 0 to {}.",
                                  unit, attachment.slot, card->label, card->portCount - 1));

    if (slot->ports.size() <= (size_t) unit)
    {
        slot->ports.resize ((size_t) unit + 1);
    }

    if (slot->ports[(size_t) unit].device.empty())
    {
        slot->ports[(size_t) unit].device = card->drives[0];
    }

    slot->ports[(size_t) unit].image = attachment.image;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplySmartPort
//
//  A position past the end of the chain adds UniDisk 3.5 drives up to it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceAttachments::ApplySmartPort (const Attachment & attachment, MachineConfig & ioConfig, std::string & outError)
{
    HRESULT       hr       = S_OK;
    PortConfig  * diskPort = nullptr;
    size_t        position = (size_t) attachment.chain - 1;



    for (PortConfig & port : ioConfig.ports)
    {
        if (port.name == "disk")
        {
            diskPort = &port;
        }
    }

    CBRF (diskPort != nullptr,
          outError = "Error: no disk port\n"
                     "       The --smartport flag attaches to a drive on the //c or\n"
                     "       //c Plus disk port.");

    while (diskPort->chain.size() <= position)
    {
        PortConfig  drive;



        drive.device = kpszUniDisk35Device;
        diskPort->chain.push_back (drive);
    }

    diskPort->chain[position].image = attachment.image;

Error:
    return hr;
}
