#pragma once

#include "Pch.h"

#include "Core/ComponentRegistry.h"
#include "Core/EmuCpu.h"
#include "Core/InterruptController.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Apple2cPlus/Apple2cPlusAccelerator.h"
#include "Machines/Apple2/Common/VideoScanner.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "Shell/MachineRefs.h"
#include "Video/VideoOutput.h"

class Apple2cRomBank;
class Apple2eMmu;
class Apple3System;
class AppleMouse;
class CharacterRomData;
class IAciaEndpoint;
class Prng;
class SiriusJoyport;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineParts
//
//  What a machine is made of. The bus, the registry and the interrupt
//  controller are fixed; the rest are rebuilt on every machine switch, and
//  a part the machine doesn't have is null: a ][ has no MMU, and only a //c
//  has the firmware bank and the IOU mouse. A component that uses a part
//  has a reference to this struct and reads the slot each time, so after a
//  machine switch replaces the part, no pointer to the old one remains.
//  MachineHost has the one instance and destroys it as a unit, in the
//  reverse of the order below.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineParts
{
    MachineParts();
    ~MachineParts();

    MachineParts (const MachineParts &)             = delete;
    MachineParts & operator= (const MachineParts &) = delete;

    // 4K of page tables, on the heap for its size.
    std::unique_ptr<MemoryBus>                   memoryBus;
    ComponentRegistry                            registry;
    InterruptController                          interruptController;

    // The Apple ///'s parts, ahead of the CPU, which has a reference to its
    // memory map. Null on every Apple II.
    std::unique_ptr<Apple3System>                apple3;

    std::unique_ptr<EmuCpu>                      cpu;
    std::unique_ptr<Prng>                        prng;

    // The ACIA endpoints are apart from the devices, since an IAciaEndpoint
    // is not a MemoryDevice.
    std::vector<std::unique_ptr<MemoryDevice>>   ownedDevices;
    std::vector<std::unique_ptr<IAciaEndpoint>>  ownedAciaEndpoints;
    std::vector<std::unique_ptr<VideoOutput>>    videoModes;

    // 4K of glyphs, on the heap for its size.
    std::unique_ptr<CharacterRomData>            charRom;

    // Raw pointers into the collections above, reset whenever one is
    // rebuilt.
    MachineRefs                                  refs;

    std::unique_ptr<Apple2eMmu>                  mmu;
    std::unique_ptr<Apple2cRomBank>              apple2cRomBank;
    std::unique_ptr<AppleMouse>                  mouse;
    std::unique_ptr<Apple2cPlusAccelerator>      accelerator;
    std::unique_ptr<SiriusJoyport>               joyport;
    std::unique_ptr<VideoTiming>                 videoTiming;

    // Owns nothing; the builder points it at each new machine's parts.
    VideoScanner                                 videoScanner;
};
