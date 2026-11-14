#include "Pch.h"

#include "DiskCheck/BootProbe.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineHost.h"
#include "Ui/Chrome/ChromeMetrics.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::Probe
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BootProbe::Probe (MachineHost & host, MachineBuilder & builder, const std::vector<Byte> & image, DiskFormat format, const Limits & limits, BootReport & outReport,
                          const std::atomic<bool> * stop)
{
    HRESULT      hr      = S_OK;
    Trace        trace;
    int          settled = 0;
    std::string  path    = format == DiskFormat::Woz ? "probe.woz" : "probe.dsk";



    outReport = BootReport();

    host.PowerCycle();

    hr = host.GetDiskStore().MountFromBytes (kDiskSlot, kDiskDrive, path, format, image);
    CHR (hr);

    for (int slice = 0; slice < limits.slices && settled < limits.bootedSlices; slice++)
    {
        Sample             sample;
        Disk2Controller  * drive     = host.GetRefs().diskController;
        bool               isStopped = stop != nullptr && stop->load();

        CBREx (!isStopped, E_ABORT);

        host.RunCycles (limits.sliceCycles);

        RenderFrame (host, builder, outReport.frame, outReport.width, outReport.height);

        sample.frameHash    = HashFrame (outReport.frame);
        sample.quarterTrack = drive != nullptr ? drive->GetQuarterTrack() : 0;
        sample.isMotorOn    = drive != nullptr && drive->IsMotorOn();
        sample.pc           = host.GetCpu()->GetPC();

        trace.samples.push_back (sample);

        if (trace.driveStarted < 0 && sample.isMotorOn)
        {
            trace.driveStarted = slice;
        }

        settled = IsSettled (trace, limits) ? settled + 1 : 0;
    }

    ReadLoop (host, limits, trace);

    outReport.endPcs = trace.endPcs;

    ReadTextRows (host, trace.rows);

    trace.isTextMode = !host.GetSoftSwitchMirror().graphicsMode;

    outReport.rows       = trace.rows;
    outReport.isTextMode = trace.isTextMode;
    outReport.verdict    = Judge (trace, limits, outReport.reason);

    host.GetDiskStore().Eject (kDiskSlot, kDiskDrive);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::IsSettled
//
////////////////////////////////////////////////////////////////////////////////

bool BootProbe::IsSettled (const Trace & trace, const Limits & limits)
{
    size_t  count   = trace.samples.size();
    size_t  window  = (size_t) std::max (limits.settledSlices, 1);
    bool    settled = trace.driveStarted >= 0 && count > (size_t) trace.driveStarted + window;



    if (settled)
    {
        settled = trace.samples.back().frameHash != trace.samples[(size_t) trace.driveStarted].frameHash;
    }

    for (size_t at = count - window; settled && at < count; at++)
    {
        settled = !trace.samples[at].isMotorOn;
    }

    return settled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::IsHeadAtRest
//
//  Whether the head was on one track for the last settledSlices samples. A
//  program that runs with the drive's motor on, as Archon does, has the
//  head at rest; a loader that is reading has it moving.
//
////////////////////////////////////////////////////////////////////////////////

bool BootProbe::IsHeadAtRest (const Trace & trace, const Limits & limits)
{
    size_t  count  = trace.samples.size();
    size_t  window = (size_t) std::max (limits.settledSlices, 1);
    bool    atRest = count >= window;



    for (size_t at = count - window; atRest && at < count; at++)
    {
        atRest = trace.samples[at].quarterTrack == trace.samples.back().quarterTrack;
    }

    return atRest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::ReadLoop
//
//  The machine runs a quarter of a second more, sampled often. The settled
//  slices' samples are a second apart, so a program that is in one short
//  loop in all of them has been there for as long as the screen was settled.
//
////////////////////////////////////////////////////////////////////////////////

void BootProbe::ReadLoop (MachineHost & host, const Limits & limits, Trace & ioTrace)
{
    static constexpr Word  kIoSpace = 0xC000;
    size_t                 count    = ioTrace.samples.size();
    size_t                 window   = (std::min) (count, (size_t) (std::max) (limits.settledSlices, 1));
    Word                   lowest   = 0xFFFF;
    Word                   highest  = 0;



    ioTrace.endPcs.clear();
    ioTrace.loopBytes.clear();

    for (int i = 0; i < kEndPcs; i++)
    {
        host.RunCycles (kEndCycles);
        ioTrace.endPcs.push_back (host.GetCpu()->GetPC());
    }

    for (Word pc : ioTrace.endPcs)
    {
        lowest  = (std::min) (lowest, pc);
        highest = (std::max) (highest, pc);
    }

    for (size_t at = count - window; at < count; at++)
    {
        lowest  = (std::min) (lowest, ioTrace.samples[at].pc);
        highest = (std::max) (highest, ioTrace.samples[at].pc);
    }

    if (highest >= lowest && highest - lowest <= kLoopSpan && highest < kIoSpace - 2)
    {
        for (int address = lowest; address <= highest + 2; address++)
        {
            ioTrace.loopBytes.push_back (host.GetMemoryBus().ReadByte ((Word) address));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::IsStuck
//
//  An instruction that reads or writes the I/O space has $C0 as its
//  operand's high byte, so a loop with no such byte doesn't access it.
//
////////////////////////////////////////////////////////////////////////////////

bool BootProbe::IsStuck (const Trace & trace)
{
    static constexpr Byte  kIoPage = 0xC0;



    return !trace.loopBytes.empty() && std::find (trace.loopBytes.begin(), trace.loopBytes.end(), kIoPage) == trace.loopBytes.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::Judge
//
////////////////////////////////////////////////////////////////////////////////

BootVerdict BootProbe::Judge (const Trace & trace, const Limits & limits, std::string & outReason)
{
    BootVerdict  verdict  = BootVerdict::Unclear;
    std::string  last;
    bool         hasError = HasRow (trace.rows, "ERR");



    for (const std::string & row : trace.rows)
    {
        last = row.empty() ? last : row;
    }

    if (trace.samples.empty() || trace.driveStarted < 0)
    {
        outReason = "The machine never started the drive.";
    }
    else if (HasRow (trace.rows, "UNABLE TO LOAD PRODOS"))
    {
        verdict   = BootVerdict::NotBootable;
        outReason = "The disk has ProDOS's boot code and no system file to start.";
    }
    else if (trace.isTextMode && last.starts_with ("*"))
    {
        verdict   = BootVerdict::DidNotBoot;
        outReason = "The machine stopped at the monitor's prompt.";
    }
    else if (trace.isTextMode && last.starts_with ("]") && hasError)
    {
        verdict   = BootVerdict::DidNotBoot;
        outReason = "The machine stopped at a BASIC prompt after an error.";
    }
    else if (trace.samples.back().frameHash == trace.samples[(size_t) trace.driveStarted].frameHash)
    {
        verdict   = BootVerdict::DidNotBoot;
        outReason = "The screen at the end was the screen when the drive started.";
    }
    else if ((IsSettled (trace, limits) || IsHeadAtRest (trace, limits)) && IsStuck (trace))
    {
        verdict   = BootVerdict::DidNotBoot;
        outReason = "The program did not appear to be running valid code: it stopped in a short loop with no input or output.";
    }
    else if (IsSettled (trace, limits))
    {
        verdict   = BootVerdict::Booted;
        outReason = "The screen changed and the drive stopped.";
    }
    else if (IsHeadAtRest (trace, limits))
    {
        verdict   = BootVerdict::Booted;
        outReason = "The screen changed and the drive's head was at rest at the time limit.";
    }
    else
    {
        outReason = "The screen changed, and the drive was running at the time limit.";
    }

    return verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::GetVerdictName
//
////////////////////////////////////////////////////////////////////////////////

const char * BootProbe::GetVerdictName (BootVerdict verdict)
{
    switch (verdict)
    {
        case BootVerdict::Booted:      return "booted";
        case BootVerdict::DidNotBoot:  return "did-not-boot";
        case BootVerdict::NotBootable: return "not-bootable";
        default:                       return "unclear";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::RenderFrame
//
//  As ShellRenderer::RenderFramebuffer draws a color monitor's frame: the
//  active mode, then the four text rows of a mixed screen. The flash state
//  is fixed, so two frames of one screen have one hash.
//
////////////////////////////////////////////////////////////////////////////////

void BootProbe::RenderFrame (MachineHost & host, MachineBuilder & builder, std::vector<uint32_t> & outFrame, int & outWidth, int & outHeight)
{
    static constexpr int      kMixedFirstRow = 20;
    static constexpr int      kMixedLastRow  = 24;
    MachineRefs             & refs           = host.GetRefs();
    const SoftSwitchMirror  & mirror         = host.GetSoftSwitchMirror();
    bool                      is80           = refs.iieSoftSwitches != nullptr && refs.iieSoftSwitches->Is80ColMode();



    outWidth  = ChromeMetrics::kFramebufferWidthPx;
    outHeight = ChromeMetrics::kFramebufferHeightPx;

    outFrame.assign ((size_t) outWidth * (size_t) outHeight, 0xFF000000);

    if (refs.text40 == nullptr)
    {
        return;
    }

    refs.text40->SetOnColor    (s_kTextArgb);
    refs.text40->SetFlashState (true);
    refs.text40->InvalidateCache();

    if (refs.text80 != nullptr)
    {
        refs.text80->SetOnColor    (s_kTextArgb);
        refs.text80->SetFlashState (true);
        refs.text80->InvalidateCache();
    }

    if (refs.hiRes != nullptr)
    {
        refs.hiRes->SetMonochrome (false);
    }

    if (refs.doubleHiRes != nullptr)
    {
        refs.doubleHiRes->SetMonochrome (false);
    }

    builder.SelectVideoMode();

    if (refs.activeVideoMode != nullptr)
    {
        refs.activeVideoMode->Render (nullptr, outFrame.data(), outWidth, outHeight);
    }

    if (mirror.mixedMode && mirror.graphicsMode && is80 && refs.text80 != nullptr)
    {
        refs.text80->SetPage2 (false);
        refs.text80->RenderRowRange (kMixedFirstRow, kMixedLastRow, nullptr, outFrame.data(), outWidth, outHeight);
    }
    else if (mirror.mixedMode && mirror.graphicsMode)
    {
        refs.text40->SetPage2 (mirror.page2);
        refs.text40->RenderRowRange (kMixedFirstRow, kMixedLastRow, nullptr, outFrame.data(), outWidth, outHeight);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::ReadTextRows
//
//  A row's first byte is at the page's base plus the standard interleave.
//  A screen byte is inverse, flashing or normal; its low seven bits are the
//  character, with the control range drawn as the letters above it.
//
////////////////////////////////////////////////////////////////////////////////

void BootProbe::ReadTextRows (MachineHost & host, std::vector<std::string> & outRows)
{
    static constexpr Byte  kSevenBits = 0x7F;
    static constexpr Byte  kFirstText = 0x20;
    static constexpr Byte  kToLetters = 0x40;
    MemoryBus            & bus        = host.GetMemoryBus();
    Word                   page       = host.GetSoftSwitchMirror().page2 ? s_kTextPage2 : s_kTextPage1;



    outRows.clear();

    for (int row = 0; row < kTextRows; row++)
    {
        Word         base = (Word) (page + ((row & 7) << 7) + ((row >> 3) * kTextCols));
        std::string  text;

        for (int col = 0; col < kTextCols; col++)
        {
            Byte  value = (Byte) (bus.ReadByte ((Word) (base + col)) & kSevenBits);

            text.push_back ((char) (value < kFirstText ? value + kToLetters : value));
        }

        text.erase (text.find_last_not_of (' ') + 1);
        outRows.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::HashFrame
//
//  FNV-1a over the pixels.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t BootProbe::HashFrame (const std::vector<uint32_t> & frame)
{
    static constexpr uint64_t  kOffset = 14695981039346656037ULL;
    static constexpr uint64_t  kPrime  = 1099511628211ULL;
    uint64_t                   hash    = kOffset;



    for (uint32_t pixel : frame)
    {
        hash = (hash ^ pixel) * kPrime;
    }

    return hash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbe::HasRow
//
////////////////////////////////////////////////////////////////////////////////

bool BootProbe::HasRow (const std::vector<std::string> & rows, std::string_view text)
{
    return std::any_of (rows.begin(), rows.end(), [text] (const std::string & row) { return row.find (text) != std::string::npos; });
}
