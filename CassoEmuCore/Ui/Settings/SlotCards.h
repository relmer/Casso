#pragma once

#include "Pch.h"

struct HardwareEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  SlotCardChoice
//
//  One card the Machine tab offers for a slot: the device id, its label,
//  whether the machine can host it, and for one it can't, the machines that
//  can. An empty device is "no card".
//
////////////////////////////////////////////////////////////////////////////////

struct SlotCardChoice
{
    std::string   device;
    std::wstring  label;
    bool          allowed = true;
    std::wstring  tip;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SlotCards
//
//  The Machine tab's edits of a machine's storage cards, over its hardware
//  entries: putting a SmartPort, hard disk or 3.5-inch card in a slot with
//  its default drives, taking a card out, attaching an image to a drive, and
//  adding or removing a drive on a chain up to the card's limit.
//
//  A card's default drives: an HD20SC with no image at SCSI ID 6, a ProFile
//  5 MB, one UniDisk 3.5, one Apple 3.5 Drive.
//
////////////////////////////////////////////////////////////////////////////////

class SlotCards
{
public:
    //  "No card" and every storage card, each marked with whether
    //  `machineId` hosts it.
    static std::vector<SlotCardChoice>  GetChoices (const std::string & machineId);

    //  The index of the entry for `slot`, or -1.
    static int  FindSlot (const std::vector<HardwareEntry> & entries, int slot);

    //  Puts `device` in `slot`, replacing what the slot has; an empty device
    //  takes the card out. Fails with ERROR_NOT_SUPPORTED, and `outError`,
    //  for a card the machine can't host.
    static HRESULT  SetCard (std::vector<HardwareEntry> & entries, const std::string & machineId,
                             int slot, const std::string & device, std::wstring & outError);

    //  Attaches the image at `path` to the drive on `port` of the entry at
    //  `index`; an empty path empties the drive.
    static HRESULT  SetPortImage (std::vector<HardwareEntry> & entries, size_t index, size_t port, const std::string & path);

    //  Adds a drive at the end of a chain card's ports, failing with
    //  ERROR_TOO_MANY_NAMES, and `outError`, at the card's limit.
    static HRESULT  AddChainDrive (std::vector<HardwareEntry> & entries, size_t index, std::wstring & outError);

    //  Removes the drive on `port` of a chain card.
    static HRESULT  RemoveChainDrive (std::vector<HardwareEntry> & entries, size_t index, size_t port);

    //  "Slot 5: UniDisk 3.5 interface card".
    static std::string  MakeDisplayName (int slot, const std::string & label);
};
