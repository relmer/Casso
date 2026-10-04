#include "Pch.h"

#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Tape/TapeRecorder.h"
#include "Devices/Tape/TapeSignalDecoder.h"
#include "Devices/Tape/WavCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecorder::Splice
//
//  Each sample from the recording point to the end of the recording takes the
//  output level at its own time: low to start, flipped at every toggle. The
//  tape grows if the recording runs past its end.
//
//  A HELD LEVEL IS NOT RECORDED. A recorder's input is AC-coupled, so while
//  the computer leaves its output alone -- before the first toggle, after
//  the last, between records -- the tape settles back to the center line and
//  takes only the deck's own faint hiss. That is what every real recording
//  has after its data, and what tape readers that end a record at the first
//  cycle out of range count on: a level held to the end of the file never
//  ends the record, and CiderPress II drops it. The hiss sits far under the
//  decoder's silence floor, so Casso reads silence there.
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecorder::Splice (TapeAudio & tape, const RecordingCapture & capture, double cpuClockHz)
{
    double    cyclesPerSample = cpuClockHz / tape.sampleRate;
    size_t    first           = (size_t) llround (capture.startSample);
    size_t    count           = (size_t) ((double) (capture.endCycle - capture.startCycle) / cyclesPerSample);
    size_t    toggle          = 0;
    bool      isHigh          = false;
    uint64_t  lastEdge        = capture.startCycle;
    uint64_t  settleCycles    = (uint64_t) (kSettleSeconds * cpuClockHz);
    uint32_t  hissSeed        = 0x2545F491;



    if (tape.sampleRate == 0 || capture.endCycle <= capture.startCycle)
    {
        return;
    }

    if (tape.samples.size() < first + count)
    {
        tape.samples.resize (first + count, 0.0f);
    }

    for (size_t i = 0; i < count; i++)
    {
        uint64_t  cycle = capture.startCycle + (uint64_t) ((double) i * cyclesPerSample);

        while (toggle < capture.toggleCycles.size() && capture.toggleCycles[toggle] <= cycle)
        {
            isHigh   = !isHigh;
            lastEdge = capture.toggleCycles[toggle];
            toggle++;
        }

        if (cycle - lastEdge > settleCycles)
        {
            hissSeed ^= hissSeed << 13;
            hissSeed ^= hissSeed >> 17;
            hissSeed ^= hissSeed << 5;

            tape.samples[first + i] = kHissLevel * ((float) (hissSeed & 0xFFFF) / 32767.5f - 1.0f);
        }
        else
        {
            tape.samples[first + i] = isHigh ? kLevel : -kLevel;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecorder::Commit
//
//  Reads the tape back, splices the recording in, writes it whole to a
//  temporary and puts that in place in one step, then decodes the result so
//  the deck plays what is now on the tape. The audio is not kept afterward.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecorder::Commit (
    IDiskFileIo            & fileIo,
    const std::string      & path,
    const RecordingCapture & capture,
    double                   cpuClockHz,
    TapeImage              & image,
    std::string            & error)
{
    HRESULT            hr       = S_OK;
    std::vector<Byte>  bytes;
    TapeAudio          tape;
    std::string        tempPath = path + ".tmp";



    hr = fileIo.ReadAllBytes (path, bytes);
    CHRF (hr, error = "The tape could not be read back to record onto.");

    hr = WavCodec::Decode (bytes, tape, error);
    CHR (hr);

    Splice (tape, capture, cpuClockHz);
    WavCodec::Encode (tape, bytes);

    hr = fileIo.WriteAllBytes (tempPath, bytes);
    CHRF (hr, error = "The recording could not be written.");

    hr = fileIo.ReplaceAtomically (tempPath, path);
    CHRF (hr, error = "The recording could not be written.");

    image            = TapeImage();
    image.path       = path;
    image.format     = TapeFormat::Wav;
    image.isWritable = true;
    TapeSignalDecoder::Decode (tape, image.signal);

Error:
    return hr;
}
