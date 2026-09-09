#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2eDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eDefinition::Get
//
//  Built once on first use and handed out by reference thereafter. The values
//  are the machine's own and never vary, so there is nothing to configure and
//  nothing to invalidate.
//
////////////////////////////////////////////////////////////////////////////////

const MachineDefinition & Apple2eDefinition::Get()
{
    static const MachineDefinition  s_definition =
    {
        .id              = "Apple2e",
        .cpu             = "6502",
        .cpuManufacturer = "MOS Technology",
        .ram             = { { .address = 0x0000, .size = 0xC000 },
                               { .address = 0x0000, .size = 0xC000, .bank = "aux" } },
        .internalDevices = { { .type = "apple2e-family-keyboard" },
                             { .type = "apple2-family-speaker" },
                             { .type = "apple2e-family-softswitches" },
                             { .type = "apple2e-family-mmu" },
                             { .type = "language-card" } },
        .videoModes      = { "apple2-text40",
                             "apple2-text80",
                             "apple2-lores",
                             "apple2-hires",
                             "apple2-doublehires" },
        .keyboardType    = "apple2e-family-layout"
    };



    return (s_definition);
}
