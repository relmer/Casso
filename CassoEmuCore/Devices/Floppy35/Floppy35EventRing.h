#pragma once

#include "Pch.h"

#include "Core/SpscEventRing.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Floppy35EventRing
//
//  A 3.5-inch event sink that queues each event for the UI thread. A full
//  ring drops the event and counts it.
//
////////////////////////////////////////////////////////////////////////////////

class Floppy35EventRing : public SpscEventRing<Floppy35Event>, public IFloppy35EventSink
{
public:
    void OnFloppy35Event (const Floppy35Event & event) override
    {
        bool  pushed = TryPush (event);



        IGNORE_RETURN_VALUE (pushed, false);
    }
};
