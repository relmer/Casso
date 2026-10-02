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
////////////////////////////////////////////////////////////////////////////////

void TapeRecorder::Splice (TapeAudio & tape, const RecordingCapture & capture, double cpuClockHz)
{
    double  cyclesPerSample = cpuClockHz / tape.sampleRate;
    size_t  first           = (size_t) llround (capture.startSample);
    size_t  count           = (size_t) ((double) (capture.endCycle - capture.startCycle) / cyclesPerSample);
    size_t  toggle          = 0;
    bool    isHigh          = false;



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
            isHigh = !isHigh;
            toggle++;
        }

        tape.samples[first + i] = isHigh ? kLevel : -kLevel;
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
