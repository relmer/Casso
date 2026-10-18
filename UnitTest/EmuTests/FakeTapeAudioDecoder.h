#pragma once

#include "Devices/Tape/ITapeAudioDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeTapeAudioDecoder
//
//  Stands in for the Media Foundation decoder: hands back whatever audio the
//  test loaded into it, or fails with the result the test chose, and counts
//  calls.
//
////////////////////////////////////////////////////////////////////////////////

class FakeTapeAudioDecoder : public ITapeAudioDecoder
{
public:
    TapeAudio  audio;
    HRESULT    result    = S_OK;
    int        callCount = 0;

    HRESULT  Decode (std::span<const Byte> bytes, TapeAudio & outAudio, std::string & error) override
    {
        UNREFERENCED_PARAMETER (bytes);

        callCount++;
        outAudio = audio;

        if (FAILED (result))
        {
            error = "Fake decoder failure.";
        }

        return result;
    }
};
