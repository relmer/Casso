#pragma once

#include "Pch.h"
#include "Devices/Disk/ExternalChangePolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IIntentChannel
//
//  Telling a running emulator what a write to an image was meant to do.
//
//  IT CARRIES ONLY THE INTENT, NEVER THE CHANGE. A change can come from a text
//  editor, a copy, a second emulator or a script, and none of those can speak
//  here; the change is found by watching the file. This says what the one writer
//  that CAN speak meant by it, and nothing else.
//
//  StateIntent RETURNS NOTHING, and that is the contract rather than an
//  oversight. Delivery is best effort: an emulator that does not receive it
//  falls back to the user's declared answer, which is correct behavior. No
//  caller could act on an error, and a build that failed over an undelivered
//  hint would be worse than the bug this feature fixes.
//
//  THE SEAM LIVES IN CORE BECAUSE ITS CALLERS RUN IN CassoCli.exe. A shim in
//  the emulator's shell would not merely be poor layering, it would not link.
//
////////////////////////////////////////////////////////////////////////////////

class IIntentChannel
{
public:
    virtual ~IIntentChannel () = default;

    //  Say what a write to `imagePath` should do to any emulator holding it.
    //
    //  THE PATH GOES AS THE WRITER RESOLVED IT, ABSOLUTE. A receiver holds its
    //  own spelling of the same file and matches after normalizing, so a
    //  relative path would name nothing on the other side.
    //
    //  STATED AFTER THE COMMIT, NEVER BEFORE. The receiver reads the image when
    //  it acts, so an intent arriving first would describe contents that are not
    //  on disk yet.
    virtual void  StateIntent (const std::string & imagePath,
                               PickUpIntent        intent) = 0;
};
