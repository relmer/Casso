#include "Pch.h"

#include "Core/DeviceDisplayNames.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceDisplayNames::Get
//
////////////////////////////////////////////////////////////////////////////////

std::string DeviceDisplayNames::Get (const std::string & type)
{
    static const struct { const char * id; const char * display; }  s_kNames[] =
    {
        { "disk-ii",                        "Disk ][" },
        { "smartport",                      "SmartPort" },
        { "mockingboard",                   "Mockingboard A (sound)" },
        { "mockingboard-c",                 "Mockingboard C (sound + speech)" },
        { kpszNetworkCardDevice,            kpszNetworkCardName },
        { "passport",                       "Passport MIDI" },
        { "serial",                         "Super Serial Card" },
        { "super-serial-card",              "Super Serial Card" },
        { "parallel",                       "Parallel printer" },
        { "parallel-printer",               "Parallel printer" },
        { "videx",                          "Videx 80-Column" },
        { "ramworks",                       "RamWorks" },
        { "ramfactor",                      "RAMFactor" },
        { "saturn128",                      "Saturn 128K" },
        { "language-card",                  "Language Card" },
        { "extended-80-column",             "Extended 80-Column Card" },
        { "cassette",                       "Cassette" },
        { "speaker",                        "Speaker" },
        { "keyboard",                       "Keyboard" },
        { "joystick",                       "Joystick" },
        { "paddle",                         "Paddle" },
        { "monitor",                        "Monitor" },
        { "apple2-family-keyboard",         "Keyboard" },
        { "apple2-family-speaker",          "Speaker" },
        { "apple2-family-softswitches",     "Soft switches" },
        { "apple2-family-cassette",         "Cassette port" },

        // The //e-generation keyboard/soft-switch controllers are shared by the
        // //e and the //c, so the label stays machine-neutral (the machine name
        // is already at the top of the panel) rather than hardcoding //e.
        { "apple2e-family-keyboard",        "Keyboard" },
        { "apple2e-family-softswitches",    "Soft switches" },
        { "apple2e-family-mmu",             "Memory management unit" },
    };



    for (const auto & each : s_kNames)
    {
        if (type == each.id)
        {
            return each.display;
        }
    }

    return type;
}
