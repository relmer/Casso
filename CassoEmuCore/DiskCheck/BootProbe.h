#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"

class MachineBuilder;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  BootVerdict, BootReport
//
//  What booting a disk image on a machine with no window found: whether it
//  booted, the reason as a sentence for an activity list, the screen at the
//  end as BGRA pixels, and the text screen's rows.
//
////////////////////////////////////////////////////////////////////////////////

enum class BootVerdict
{
    Unclear,
    Booted,
    DidNotBoot,
    NotBootable,
};

struct BootReport
{
    BootVerdict               verdict    = BootVerdict::Unclear;
    std::string               reason;
    std::vector<uint32_t>     frame;
    int                       width      = 0;
    int                       height     = 0;
    std::vector<std::string>  rows;
    bool                      isTextMode = true;
    std::vector<Word>         endPcs;                 // the program counter at short intervals once the boot ended
};





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe
//
//  A disk image booted in slot 6, drive 1 of a machine, in slices, with a
//  sample after each: a hash of the screen, the drive's quarter track and
//  motor, and the program counter.
//
//  Booted: the screen differs from the screen when the drive first started,
//  and the drive's motor was off for settledSlices slices, or at the limit
//  its head was on one track for that long: a loader that is reading has
//  the motor on and the head moving. Did not
//  boot: the screen is as it was when the drive started, at the limit; or
//  the machine is at the monitor's prompt, or at BASIC's after an error.
//  Not bootable: the screen has ProDOS's message that it found no system
//  file. Any other end is unclear. The run ends once booted has been true
//  for bootedSlices slices.
//
////////////////////////////////////////////////////////////////////////////////

class BootProbe
{
public:
    struct Limits
    {
        uint64_t  sliceCycles   = 1'000'000;
        int       slices        = 120;
        int       settledSlices = 20;
        int       bootedSlices  = 60;
    };

    //  One slice's sample.
    struct Sample
    {
        uint64_t  frameHash    = 0;
        int       quarterTrack = 0;
        bool      isMotorOn    = false;
        Word      pc           = 0;
    };

    //  What Judge reads: the samples, the index of the first one with the
    //  drive's motor on, and the text screen at the end.
    struct Trace
    {
        std::vector<Sample>       samples;
        int                       driveStarted = -1;
        std::vector<std::string>  rows;
        bool                      isTextMode   = true;

        //  The program counter at short intervals once the boot ended, and
        //  the memory from the lowest to the highest of them and of the
        //  settled slices', read when they are within kLoopSpan bytes below
        //  the I/O space: the loop a program that has stopped is in.
        std::vector<Word>         endPcs;
        std::vector<Byte>         loopBytes;
    };

    static constexpr int  kDiskSlot  = 6;
    static constexpr int  kDiskDrive = 0;      // drive 1, as the disk store counts from 0
    static constexpr int  kTextRows  = 24;
    static constexpr int  kTextCols  = 40;
    static constexpr int  kLoopSpan  = 255;
    static constexpr int  kEndPcs    = 256;
    static constexpr int  kEndCycles = 997;    // between two of them, a prime so a loop's period doesn't hide its instructions

    //  The image mounted on the machine, which is power cycled first, then
    //  run to a verdict. The machine is the caller's, built by the builder.
    //  With a stop flag, the boot ends with E_ABORT once the flag is set,
    //  as another thread sets it when the result won't be used.
    static HRESULT      Probe (MachineHost & host, MachineBuilder & builder, const std::vector<Byte> & image, DiskFormat format, const Limits & limits, BootReport & outReport,
                               const std::atomic<bool> * stop = nullptr);

    static BootVerdict  Judge (const Trace & trace, const Limits & limits, std::string & outReason);

    //  Whether the program is in a short loop with no reference to the
    //  I/O space, $C000 to $C0FF, so with no key, button or paddle read
    //  and no sound, as a program that jumped into data is. A loop in
    //  ROM, as the monitor's wait for a key is, isn't one.
    static bool  IsStuck  (const Trace & trace);
    static void  ReadLoop (MachineHost & host, const Limits & limits, Trace & ioTrace);

    //  Whether the samples so far are a boot: the screen differs from the
    //  one at driveStarted, and the motor was off for settledSlices.
    static bool         IsSettled    (const Trace & trace, const Limits & limits);
    static bool         IsHeadAtRest (const Trace & trace, const Limits & limits);

    //  The verdict as the manifest has it: booted, did-not-boot,
    //  not-bootable or unclear.
    static const char * GetVerdictName (BootVerdict verdict);

    //  The machine's screen as Casso draws it on a color monitor.
    static void         RenderFrame (MachineHost & host, MachineBuilder & builder, std::vector<uint32_t> & outFrame, int & outWidth, int & outHeight);

    //  The 40-column text screen's rows, through the processor's memory.
    static void         ReadTextRows (MachineHost & host, std::vector<std::string> & outRows);

private:
    static uint64_t     HashFrame (const std::vector<uint32_t> & frame);
    static bool         HasRow    (const std::vector<std::string> & rows, std::string_view text);

    static constexpr uint32_t  s_kTextArgb  = 0xFFFFFFFF;
    static constexpr Word      s_kTextPage1 = 0x0400;
    static constexpr Word      s_kTextPage2 = 0x0800;
};
