#include "Pch.h"

#include "Machines/Apple2/Apple2/Apple2.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2::GetRam
//
//  48K in one bank. The machine shipped in 4K and 16K configurations too, but
//  Casso emulates the full complement, which is what every program of the era
//  past the first year assumes.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RamRegion> Apple2::GetRam() const
{
    std::vector<RamRegion>  ram = { { .address = 0x0000, .size = 0xC000 } };



    return (ram);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Apple2::GetInternalDevices
//
//  The motherboard: keyboard, speaker, soft switches, game port.
//
//  Everything here is on the board rather than in a slot, which is what makes
//  it the machine's rather than its owner's. What went into the slots is the
//  owner's and lives in the JSON.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<InternalDevice> Apple2::GetInternalDevices() const
{
    std::vector<InternalDevice>  devices = { { .type = "apple2-family-keyboard" },
                                             { .type = "apple2-family-speaker" },
                                             { .type = "apple2-family-softswitches" },
                                             { .type = "apple2-family-gameport" } };



    return (devices);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Apple2::GetVideoModes
//
//  40-column text, lo-res and hi-res. 80 columns and double hi-res need the
//  auxiliary bank the //e introduced, so they are not this machine's to offer.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Apple2::GetVideoModes() const
{
    std::vector<std::string>  modes = { "apple2-text40", "apple2-lores", "apple2-hires" };



    return (modes);
}
