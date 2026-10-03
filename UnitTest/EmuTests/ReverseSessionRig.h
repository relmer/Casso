#pragma once

#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSessionRig
//
//  A //e running a guest loop that touches every kind of state reverse
//  execution has to rewind: it polls the keyboard and stores each key at
//  $0300, times paddle 0 into $0301, reads the Disk II data latch into
//  $0302, counts in $0304 and writes the count to the Mockingboard, and every
//  sixteenth pass calls a routine at $0900 that writes sixteen bytes to the
//  disk and pushes and pops one byte before it returns.
//
//  The machine is power cycled first, with a pattern disk in slot 6 drive 1
//  whose flushes go to a sink rather than a file.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseSessionRig
{
public:
    static constexpr Word    kProgramStart = 0x0800;
    static constexpr Word    kLoopTop      = 0x0806;
    static constexpr Word    kCallSite     = 0x0834;
    static constexpr Word    kReturnSite   = 0x0837;
    static constexpr Word    kWriteRoutine = 0x0900;
    static constexpr Word    kRoutinePla   = 0x0929;
    static constexpr Word    kRoutineRts   = 0x092A;
    static constexpr Word    kKeyStore     = 0x0300;
    static constexpr Word    kCounter      = 0x0304;
    static constexpr int     kDiskSlot     = 6;
    static constexpr int     kDiskDrive    = 0;
    static constexpr size_t  kDiskPattern  = 13;


    static void Prepare (TestMachine & machine)
    {
        static constexpr Byte  kLoop[] =
        {
            0xAD, 0xE9, 0xC0,       // 0800  LDA $C0E9    motor on
            0xAD, 0xEA, 0xC0,       // 0803  LDA $C0EA    drive 1
            0xAD, 0x00, 0xC0,       // 0806  LDA $C000
            0x10, 0x06,             // 0809  BPL $0811
            0x8D, 0x00, 0x03,       // 080B  STA $0300
            0x8D, 0x10, 0xC0,       // 080E  STA $C010
            0xAD, 0x70, 0xC0,       // 0811  LDA $C070    paddle trigger
            0xA2, 0x00,             // 0814  LDX #$00
            0xAD, 0x64, 0xC0,       // 0816  LDA $C064
            0x10, 0x03,             // 0819  BPL $081E
            0xE8,                   // 081B  INX
            0xD0, 0xF8,             // 081C  BNE $0816
            0x8E, 0x01, 0x03,       // 081E  STX $0301
            0xAD, 0xEC, 0xC0,       // 0821  LDA $C0EC    data latch
            0x8D, 0x02, 0x03,       // 0824  STA $0302
            0xEE, 0x04, 0x03,       // 0827  INC $0304
            0xAD, 0x04, 0x03,       // 082A  LDA $0304
            0x8D, 0x01, 0xC4,       // 082D  STA $C401    Mockingboard VIA 1
            0x29, 0x0F,             // 0830  AND #$0F
            0xD0, 0x03,             // 0832  BNE $0837
            0x20, 0x00, 0x09,       // 0834  JSR $0900
            0x4C, 0x06, 0x08,       // 0837  JMP $0806
        };
        static constexpr Byte  kWrite[] =
        {
            0xAD, 0xED, 0xC0,       // 0900  LDA $C0ED    Q6 high
            0xAD, 0xEE, 0xC0,       // 0903  LDA $C0EE    Q7 low
            0xA9, 0xFF,             // 0906  LDA #$FF
            0x8D, 0xEF, 0xC0,       // 0908  STA $C0EF    write mode
            0x0D, 0xEC, 0xC0,       // 090B  ORA $C0EC
            0xA0, 0x10,             // 090E  LDY #$10
            0xAD, 0x04, 0x03,       // 0910  LDA $0304
            0x09, 0x80,             // 0913  ORA #$80
            0x8D, 0xED, 0xC0,       // 0915  STA $C0ED    load
            0x0D, 0xEC, 0xC0,       // 0918  ORA $C0EC    shift
            0xEA, 0xEA, 0xEA, 0xEA, // 091B  NOP x4
            0x88,                   // 091F  DEY
            0xD0, 0xEE,             // 0920  BNE $0910
            0xAD, 0xEE, 0xC0,       // 0922  LDA $C0EE    read mode
            0xAD, 0xEC, 0xC0,       // 0925  LDA $C0EC
            0x48,                   // 0928  PHA
            0x68,                   // 0929  PLA
            0x60,                   // 092A  RTS
        };
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
        HRESULT            hr  = S_OK;
        size_t             i   = 0;



        machine.PowerCycle();

        for (i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> (i * kDiskPattern);
        }

        machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

        hr = machine.GetDiskStore().MountFromBytes (kDiskSlot, kDiskDrive, "reverse.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        machine.GetRefs().diskController->SetExternalDisk (kDiskDrive, machine.GetDiskStore().GetImage (kDiskSlot, kDiskDrive));

        for (i = 0; i < sizeof (kLoop); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kProgramStart + i), kLoop[i]);
        }

        for (i = 0; i < sizeof (kWrite); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kWriteRoutine + i), kWrite[i]);
        }

        machine.GetCpu()->SetPC (kProgramStart);
    }


    //  One keyframe every framesPerKeyframe frames.
    static ReverseSettings MakeSettings (uint64_t framesPerKeyframe)
    {
        ReverseSettings  settings;



        settings.keyframes.intervalCycles = KeyframeSettings::kFrameCycles * framesPerKeyframe;

        return settings;
    }


    static std::vector<Byte> Save (MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        return writer.GetBytes();
    }


    static uint64_t Checksum (MachineHost & machine)
    {
        std::vector<Byte>  state = Save (machine);



        return KeyframeStore::ComputeChecksum (state.data(), state.size());
    }


    //  The host inputs a scripted session feeds in at the start of a slice,
    //  as the UI thread would: a key every seventh slice, a paddle move every
    //  fifth.
    static void Feed (MachineHost & machine, size_t slice)
    {
        static constexpr size_t  kKeyEvery    = 7;
        static constexpr size_t  kPaddleEvery = 5;
        static constexpr size_t  kKeyCount    = 26;
        static constexpr size_t  kPaddleStep  = 37;



        if (slice % kKeyEvery == kKeyEvery - 1)
        {
            machine.GetRefs().keyboard->PressKey (static_cast<Byte> ('A' + (slice / kKeyEvery) % kKeyCount));
        }

        if (slice % kPaddleEvery == kPaddleEvery - 1)
        {
            machine.GetRefs().iieSoftSwitches->SetPaddle (0, static_cast<Byte> (slice * kPaddleStep));
        }
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseProbe
//
//  A stop test for the rig: a breakpoint on one address, optionally only
//  while a byte of memory holds a value, and a write watchpoint on one
//  address through the bus's watch sink.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseProbe : public IReverseStopTest, public IWatchSink
{
public:
    static constexpr int  kNone = -1;

    int   breakPc       = kNone;
    int   conditionAddr = kNone;
    Byte  conditionByte = 0;
    int   watchAddr     = kNone;
    bool  isPending     = false;


    bool ShouldStopBefore (MachineHost & machine, Word pc) override
    {
        static constexpr Word  kOffsetMask = 0xFF;
        bool                   isStop      = breakPc != kNone && pc == breakPc;
        const Byte           * page        = nullptr;



        if (isStop && conditionAddr != kNone)
        {
            page   = machine.GetMemoryBus().GetShadowReadPage (static_cast<Word> (conditionAddr));
            isStop = page != nullptr && page[conditionAddr & kOffsetMask] == conditionByte;
        }

        return isStop;
    }


    bool TakePendingStop() override
    {
        bool  wasPending = isPending;



        isPending = false;

        return wasPending;
    }


    IWatchSink * GetWatchSink() override
    {
        return this;
    }


    void OnWatchedAccess (Word address, Byte, BusAccess access, std::optional<Byte>) override
    {
        if (access == BusAccess::Write && address == watchAddr)
        {
            isPending = true;
        }
    }


    void Watch (MachineHost & machine, Word address)
    {
        watchAddr = address;
        machine.GetMemoryBus().SetWatchSink (this);
        machine.GetMemoryBus().SetWatchedPage (address >> 8, true);
    }
};
