#pragma once

#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingGamePortSink
//
//  Records every state the mixer writes, and can be told to refuse the next
//  few writes the way the machine sink does while a rebuild holds the machine.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingGamePortSink : public IGamePortSink
{
public:

    struct AppliedWrite
    {
        GamePortState  state;
        bool           wroteEveryField = false;
    };

    bool TryApply (const GamePortState & target, const GamePortState * lastApplied) override
    {
        bool  accepted = refuseNext <= 0;



        if (accepted)
        {
            writes.push_back ({ target, lastApplied == nullptr });
        }
        else
        {
            refuseNext--;
            refusedCount++;
        }

        return accepted;
    }

    std::vector<AppliedWrite>  writes;
    int                        refuseNext   = 0;
    int                        refusedCount = 0;
};
