#pragma once

#include "Devices/Tape/RecordingCapture.h"
#include "Devices/Tape/TapeAudio.h"
#include "Devices/Tape/TapeImage.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecorder
//
//  Writes a recording onto a WAV tape the way a deck records: over whatever
//  was there, from where the tape stood, running on past the end if the
//  recording is longer. The output toggles become a square wave at the tape's
//  own sample rate; the samples before the recording point are not touched.
//
////////////////////////////////////////////////////////////////////////////////

class TapeRecorder
{
public:
    static void     Splice (TapeAudio              & tape,
                            const RecordingCapture & capture,
                            double                   cpuClockHz);

    static HRESULT  Commit (IDiskFileIo            & fileIo,
                            const std::string      & path,
                            const RecordingCapture & capture,
                            double                   cpuClockHz,
                            TapeImage              & image,
                            std::string            & error);

    static constexpr float  kLevel = 0.8f;   // of full scale, high and low
};
