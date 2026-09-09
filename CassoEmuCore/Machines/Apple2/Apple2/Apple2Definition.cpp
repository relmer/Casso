#include "Pch.h"

#include "Machines/Apple2/Apple2/Apple2Definition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2Definition::Get
//
//  Built once on first use and handed out by reference thereafter. The values
//  are the machine's own and never vary, so there is nothing to configure and
//  nothing to invalidate.
//
////////////////////////////////////////////////////////////////////////////////

const MachineDefinition & Apple2Definition::Get()
{
    static const MachineDefinition  s_definition =
    {
        .id              = "Apple2",
        .cpu             = "6502",
        .cpuManufacturer = "MOS Technology",
        .ram             = { { .address = 0x0000, .size = 0xC000 } },
        .internalDevices = { { .type = "apple2-keyboard" },
                             { .type = "apple2-speaker" },
                             { .type = "apple2-softswitches" },
                             { .type = "apple2-gameport" } },
        .videoModes      = { "apple2-text40", "apple2-lores", "apple2-hires" },
        .keyboardType    = "apple2-uppercase"
    };

    return (s_definition);
}
