#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2e.h"

#include "Machines/MachineDeviceTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2e::GetRam
//
//  Two banks of 48K at the same addresses: main and aux. The MMU decides which
//  one a read or a write lands on, which is why the second bank is described
//  here rather than mapped somewhere else -- both are the machine's memory.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RamRegion> Apple2e::GetRam() const
{
    std::vector<RamRegion>  ram = { { .address = 0x0000, .size = 0xC000 },
                                    { .address = 0x0000, .size = 0xC000, .bank = "aux" } };



    return (ram);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2e::GetInternalDevices
//
//  The redesigned motherboard. Only the speaker survives from the ][+.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<InternalDevice> Apple2e::GetInternalDevices() const
{
    std::vector<InternalDevice>  devices = { { .type = "apple2e-family-keyboard" },
                                             { .type = "apple2-family-speaker" },
                                             { .type = "apple2e-family-softswitches" },
                                             { .type = MachineDeviceTypes::kMmu },
                                             { .type = "language-card" } };



    return (devices);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2e::GetVideoModes
//
//  The original three plus the two the auxiliary bank makes possible. 80-column
//  text and double hi-res are listed here and not on the ][ for a physical
//  reason rather than a policy one: without a second bank there is nowhere for
//  the interleaved half of the picture to live.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Apple2e::GetVideoModes() const
{
    std::vector<std::string>  modes = { "apple2-text40",
                                        "apple2-text80",
                                        "apple2-lores",
                                        "apple2-hires",
                                        "apple2-doublehires" };



    return (modes);
}
