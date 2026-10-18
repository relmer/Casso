#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingCapture
//
//  One stretch of recording: where on the tape it began, the bus cycles it
//  ran between, and the bus cycle of every toggle of the cassette output in
//  between.
//
////////////////////////////////////////////////////////////////////////////////

struct RecordingCapture
{
    double                 startSample = 0.0;
    uint64_t               startCycle  = 0;
    uint64_t               endCycle    = 0;
    std::vector<uint64_t>  toggleCycles;
};
