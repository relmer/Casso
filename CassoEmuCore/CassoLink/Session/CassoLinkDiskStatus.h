#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendStatus.h"
#include "CassoLink/Session/CassoLinkCapture.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskStatus
//
//  A disk read or capture in a FullSendStatus, as both a Full Send and a
//  single disk job draw it: a track's mark once its read ends, and a raw
//  capture's positions as its progress arrives and once it ends.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkDiskStatus
{
public:
    //  A track's mark from what its read added: unreadable sectors, then
    //  sectors that weren't clean, then a verified read or a plain one.
    static FullSendTrackMark  GetTrackMark (size_t newUnreadable, size_t newMarks, bool verifying);

    //  A capture's progress. In the second pass, a position below the one
    //  read that has no read was skipped.
    static void  NoteCapture       (FullSendStatus & status, int drive, const CassoLinkCapture::Progress & where);

    //  A finished capture: the positions after the last one read were
    //  skipped, and the weak, unstable and repaired positions are marked or
    //  listed, with the capture's times.
    static void  NoteCaptureResult (FullSendStatus & status, int drive, const CassoLinkCapture::Result & result);
};
