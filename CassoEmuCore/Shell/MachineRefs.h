#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRefs
//
//  Per-machine observer pointers. Every entry is a raw pointer into one of
//  the unique_ptr-owning collections MachineHost holds -- its owned-device
//  vector or its video-mode vector. They are caches for quick access only,
//  never own anything, so they MUST be reset every time the owning
//  collection is rebuilt, or they'll dangle into freed memory. Bundling
//  them in one struct makes that a single assignment (`m_refs = {};`) in
//  the teardown that precedes a rebuild, instead of a checklist of
//  individual nullptr assignments that the next field to be added will
//  inevitably miss.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineRefs
{
    class AppleKeyboard *         keyboard         = nullptr;
    class AppleSoftSwitchBank *   softSwitches     = nullptr;
    class AppleGamePort *         gamePort         = nullptr;
    class AppleSpeaker *          speaker          = nullptr;
    class RamDevice *             mainRamDev       = nullptr;
    class Disk2Controller *       diskController   = nullptr;
    class MockingboardCard *      mockingboard     = nullptr;
    class VideoOutput *           activeVideoMode  = nullptr;
    class PrinterCard *           printerCard      = nullptr;

    // The //e-and-later halves of the two devices above: the same
    // objects as `softSwitches` / `keyboard` when the machine has the
    // //e variants, null on a ][ or ][+. Both are resolved once at build
    // time from the configured device type, in the same statement that
    // sets the base pointer -- so each is non-null in exactly the cases
    // where downcasting the base would have succeeded, and callers that
    // need the derived surface (80COL, 80STORE, DHIRES, ALTCHARSET) read
    // a pointer instead of re-deriving it at every use. Null is the
    // ][ / ][+ answer, which every caller already had to handle.
    class Apple2eSoftSwitchBank * iieSoftSwitches  = nullptr;
    class Apple2eKeyboard *       iieKeyboard      = nullptr;

    // Video modes, addressed by name. All five exist on every machine --
    // SelectVideoMode switches between them per frame from soft-switch
    // state and cannot afford to construct one mid-render -- so these are
    // non-null together, from CreateVideoModes until teardown.
    //
    // These replaced positional lookups into m_videoModes. That vector
    // still OWNS the modes, but nothing outside CreateVideoModes indexes
    // it: an index carries no type, so every use site had to restate
    // which slot meant which mode and downcast to match, and a mode
    // inserted anywhere but the end would have silently re-pointed all
    // of them at the wrong renderer.
    class AppleTextMode *         text40           = nullptr;
    class AppleLoResMode *        loRes            = nullptr;
    class AppleHiResMode *        hiRes            = nullptr;
    class AppleDoubleHiResMode *  doubleHiRes      = nullptr;
    class Apple80ColTextMode *    text80           = nullptr;
};
