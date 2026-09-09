#include "Pch.h"

#include "Machines/Apple2/Apple2c/Apple2cDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cDefinition::Get
//
//  Built once on first use and handed out by reference thereafter. The values
//  are the machine's own and never vary, so there is nothing to configure and
//  nothing to invalidate.
//
////////////////////////////////////////////////////////////////////////////////

const MachineDefinition & Apple2cDefinition::Get()
{
    static const MachineDefinition  s_definition =
    {
        .id              = "Apple2c",
        .cpu             = "65C02",
        .cpuManufacturer = "Rockwell",
        .ram             = { { .address = 0x0000, .size = 0xC000 },
                               { .address = 0x0000, .size = 0xC000, .bank = "aux" } },
        .internalDevices = { { .type = "apple2e-keyboard" },
                             { .type = "apple2-speaker" },
                             { .type = "apple2e-softswitches" },
                             { .type = "apple2e-mmu" },
                             { .type = "language-card" } },
        .videoModes      = { "apple2-text40",
                             "apple2-text80",
                             "apple2-lores",
                             "apple2-hires",
                             "apple2-doublehires" },
        .keyboardType    = "apple2e-full"
    };



    return (s_definition);
}
