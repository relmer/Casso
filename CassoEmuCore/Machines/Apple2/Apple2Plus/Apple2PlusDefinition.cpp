#include "Pch.h"

#include "Machines/Apple2/Apple2Plus/Apple2PlusDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2PlusDefinition::Get
//
//  Built once on first use and handed out by reference thereafter. The values
//  are the machine's own and never vary, so there is nothing to configure and
//  nothing to invalidate.
//
////////////////////////////////////////////////////////////////////////////////

const MachineDefinition & Apple2PlusDefinition::Get()
{
    static const MachineDefinition  s_definition =
    {
        .id              = "Apple2Plus",
        .cpu             = "6502",
        .cpuManufacturer = "MOS Technology",
        .ram             = { { .address = 0x0000, .size = 0xC000 } },
        .internalDevices = { { .type = "apple2-family-keyboard" },
                             { .type = "apple2-family-speaker" },
                             { .type = "apple2-family-softswitches" },
                             { .type = "apple2-family-gameport" } },
        .videoModes      = { "apple2-text40",
                             "apple2-lores",
                             "apple2-hires" },
        .keyboardType    = "apple2-family-layout"
    };



    return (s_definition);
}
